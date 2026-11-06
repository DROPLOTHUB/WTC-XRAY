#pragma once
#include <algorithm>
#include <cmath>

// Pure ground model: no DMA, rendering, allocation or mutable global state.
// vec3_t must be included by the caller.
namespace ground_prediction {
inline bool finite(const vec3_t& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
// W2S puede aceptar un punto delante de la camara aunque quede fuera del
// monitor. No afirmar "Dibujando" si ImGui recortara el marcador.
inline bool screen_visible(float x,float y,float vx,float vy,float width,float height) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(vx) &&
        std::isfinite(vy) && std::isfinite(width) && std::isfinite(height) &&
        width>0 && height>0 && x>=vx && x<vx+width && y>=vy && y<vy+height;
}
struct Ballistics {
    float speed = 0, mass = 0, caliber = 0, length = 0, drag = 0;
};
struct Solution {
    vec3_t point{};
    float time = 0, error = 0;
    int steps = 0;
    bool valid = false;
};
inline constexpr int max_iterations = 10;
inline constexpr int max_steps = 512;
inline constexpr float time_step = 1.0f / 64.0f;
inline constexpr float horizon = max_steps * time_step;

// Signed horizontal turn rate; velocity is NOT normalized in the numerator.
inline float turn_rate(const vec3_t& v, const vec3_t& a) {
    if (!finite(v) || !finite(a)) return 0;
    const double s2 = double(v.x)*v.x + double(v.z)*v.z;
    return s2 < 1.0 ? 0.0f : static_cast<float>(std::clamp(
        (double(v.x)*a.z-double(v.z)*a.x)/s2, -0.8, 0.8));
}
inline vec3_t propagate(const vec3_t& p, const vec3_t& v, const vec3_t& a, float t) {
    if (!finite(p) || !finite(v) || !finite(a) || !std::isfinite(t)) return p;
    t = std::clamp(t, 0.0f, horizon + 0.25f);
    const double speed = std::hypot(double(v.x), double(v.z));
    if (speed < 0.1f) return p + v*t; // no inferred launch direction at rest
    const double along = std::clamp((double(v.x)*a.x + double(v.z)*a.z)/speed, -12.0, 8.0);
    const double omega = turn_rate(v, a);
    double active = std::min(double(t), 0.6);
    if (along < -0.01) active = std::min(active, speed / -along);
    // Exact integrals of (speed + along*u) * exp(i*omega*u).
    // Series near zero avoid cancellation and include the straight-line limit.
    const double q = omega*active, q2 = q*q;
    double ic, is, jc, js;
    if (std::abs(q) < 0.01) {
        ic = active*(1-q2/6+q2*q2/120);
        is = active*q*(0.5-q2/24+q2*q2/720);
        jc = active*active*(0.5-q2/8+q2*q2/144);
        js = active*active*q*(1.0/3-q2/30+q2*q2/840);
    } else {
        ic = active*std::sin(q)/q;
        is = active*(1-std::cos(q))/q;
        jc = active*active*(q*std::sin(q)+std::cos(q)-1)/q2;
        js = active*active*(std::sin(q)-q*std::cos(q))/q2;
    }
    const double coast = std::max(0.0, speed+along*active)*(t-active);
    const double forward = speed*ic+along*jc+coast*std::cos(q);
    const double sideways = speed*is+along*js+coast*std::sin(q);
    return {static_cast<float>(p.x+(v.x*forward-v.z*sideways)/speed),
        p.y+v.y*t, // terrain bumps are not vertical acceleration
        static_cast<float>(p.z+(v.z*forward+v.x*sideways)/speed)};
}
// Snapshot position is old, DMA velocity is current: integrate backwards from
// current velocity over the short sample age, then solve at age=0. Constant
// recent acceleration is an approximation; never apply the whole age twice.
inline vec3_t align_position(const vec3_t& p, const vec3_t& current_velocity,
    const vec3_t& recent_acceleration, float age) {
    if (!std::isfinite(age)) return p;
    age = std::clamp(age, 0.0f, 0.25f);
    return p + current_velocity*age - recent_acceleration*(0.5f*age*age);
}
inline float drag_coefficient(const Ballistics& b, float altitude) {
    if (!std::isfinite(b.mass) || b.mass <= 0.00001f) return 0.0f;
    float area = b.drag;
    if (!std::isfinite(area) || area <= 0) {
        // Preserve ground reader's existing caliber/length convention.
        if (!std::isfinite(b.caliber) || !std::isfinite(b.length) || b.caliber <= 0 || b.length <= 0)
            return 0;
        area = 3.14159265f * 0.5f * (b.caliber*0.5f)*(b.caliber*0.5f)*b.length;
    }
    const float h = std::clamp(altitude, 0.0f, 18300.0f);
    const float density = 1.225f * (((((2.2871901e-19f*h - 5.8355603e-14f)*h
        + 0.00000000353118f)*h - 0.000095938703f)*h) + 1.0f);
    return std::clamp(density*area/b.mass, 0.0f, 0.1f);
}
inline float mach_factor(float speed) {
    const float m = speed / 343.0f;
    if (m < 0.8f) return 1;
    if (m < 1.2f) return 1 + 4.5f*(m-0.8f);
    return 2.8f/std::sqrt(m);
}
// Positive drag always dissipates energy; rational update cannot reverse velocity.
inline void step(vec3_t& p, vec3_t& v, float k, bool mach, float dt) {
    const vec3_t old = v;
    // Symmetric gravity/drag splitting plus midpoint Mach coefficient avoids
    // first-order range error for fast, high-drag ammunition at the same step cap.
    v.y -= 4.905f*dt;
    const float speed = v.length();
    float factor = 1.0f;
    if (mach) {
        const float mid_speed = speed / (1.0f + k*mach_factor(speed)*speed*(0.5f*dt));
        factor = mach_factor(mid_speed);
    }
    v = v * (1.0f / (1.0f + k*factor*speed*dt));
    v.y -= 4.905f*dt;
    p = p + (old+v)*(0.5f*dt);
}
inline bool trace(const vec3_t& origin, const vec3_t& aim, const Ballistics& b,
    float k, float range, vec3_t& hit, float& time, int& steps, const vec3_t& shooter_velocity = {},
    const vec3_t& plane_normal = {}) {
    const vec3_t d = aim-origin;
    const float len = d.length();
    if (!finite(origin) || !finite(aim) || !finite(shooter_velocity) || !finite(plane_normal)
        || !std::isfinite(len) || len < 0.01f || !std::isfinite(range) || range < 0.01f
        || !std::isfinite(k) || k < 0 || !std::isfinite(b.speed) || b.speed <= 0) return false;
    // Legacy callers supply horizontal range. Solve supplies a fixed 3D LOS normal.
    vec3_t normal = plane_normal;
    if (normal.length() == 0) normal = {d.x,0,d.z};
    const float normal_length = normal.length();
    if (!std::isfinite(normal_length) || normal_length < 0.00001f) return false;
    normal = normal*(1.0f/normal_length);
    // Launch in world velocity; drag acts on the projectile, not on target-relative motion.
    vec3_t p{}, v = d*(b.speed/len) + shooter_velocity; // relative position avoids world-coordinate drift
    float previous_range = 0;
    for (int i=0; i<max_steps; ++i) {
        const vec3_t old = p;
        step(p, v, k, b.drag > 0 && std::isfinite(b.drag), time_step);
        ++steps;
        const float r = p.x*normal.x+p.y*normal.y+p.z*normal.z;
        if (!finite(p) || !finite(v) || !std::isfinite(r)) return false;
        if (r >= range) {
            const float f = std::clamp((range-previous_range)/std::max(0.00001f,r-previous_range),0.0f,1.0f);
            hit = origin + old + (p-old)*f;
            time = (i+f)*time_step;
            return finite(hit) && std::isfinite(time) && time > 0 && time <= horizon;
        }
        if (r <= previous_range) return false;
        previous_range = r;
    }
    return false;
}
inline Solution solve(const vec3_t& origin, const vec3_t& target, const vec3_t& velocity,
    const vec3_t& acceleration, const Ballistics& b, float age = 0,
    const vec3_t& shooter_velocity = {}) {
    Solution result{};
    if (!finite(origin) || !finite(target) || !finite(velocity) || !finite(acceleration)
        || !finite(shooter_velocity) || shooter_velocity.length() > 150.0f
        || !std::isfinite(b.speed) || b.speed < 50 || b.speed > 4000 || !std::isfinite(age)
        || !std::isfinite(b.mass) || !std::isfinite(b.caliber) || !std::isfinite(b.length)
        || !std::isfinite(b.drag)) return result;
    age = std::clamp(age, 0.0f, 0.25f);
    const float k = drag_coefficient(b, std::max(origin.y,target.y));
    const vec3_t los = target-origin;
    const float distance = los.length();
    if (!std::isfinite(distance) || distance < 0.01f || !std::isfinite(k)) return result;
    const vec3_t normal = los*(1.0f/distance);
    const float own_along = shooter_velocity.x*normal.x+shooter_velocity.y*normal.y+shooter_velocity.z*normal.z;
    const float own2 = shooter_velocity.x*shooter_velocity.x+shooter_velocity.y*shooter_velocity.y+shooter_velocity.z*shooter_velocity.z;
    const float transverse2 = std::max(0.0f, own2-own_along*own_along);
    const float forward2 = b.speed*b.speed-transverse2;
    // Vacuum stationary-target seed includes the inherited launch vector, not
    // merely muzzle speed. A seed outside 8 s is not proof of no moving intercept.
    const float closing = forward2 > 0 ? std::sqrt(forward2)+own_along : 0;
    float t = std::min(horizon, distance/(closing > 0.01f ? closing : b.speed));
    vec3_t future = propagate(target,velocity,acceleration,t+age);
    vec3_t correction = vec3_t{0,4.905f*t*t,0}-shooter_velocity*t;
    for (int i=0; i<max_iterations; ++i) {
        const vec3_t aim = future + correction;
        if (!finite(future) || !finite(aim)) return result;
        const vec3_t offset = future-origin;
        const float range = offset.x*normal.x+offset.y*normal.y+offset.z*normal.z;
        vec3_t hit{};
        if (!trace(origin,aim,b,k,range,hit,t,result.steps,shooter_velocity,normal)) return result;
        const vec3_t next = propagate(target,velocity,acceleration,t+age);
        const float err = (next-hit).length();
        if (!std::isfinite(err)) return result;
        result.error = err;
        if (err <= 0.15f) {
            result.point = aim;
            result.time = t;
            result.valid = true;
            return result;
        }
        // Compare impact with target, never compensated aim with uncompensated target.
        // Correct lateral drift as well as drop; elevation alone cannot cancel own motion.
        correction += next-hit;
        future = next;
    }
    return result; // explicit failure, not an arbitrary final iterate
}
}
