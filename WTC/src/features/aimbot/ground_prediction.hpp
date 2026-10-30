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
    const float s2 = v.x*v.x + v.z*v.z;
    return s2 < 1.0f ? 0.0f : std::clamp((v.x*a.z-v.z*a.x)/s2, -0.8f, 0.8f);
}
inline vec3_t propagate(const vec3_t& p, const vec3_t& v, const vec3_t& a, float t) {
    if (!finite(p) || !finite(v) || !finite(a) || !std::isfinite(t)) return p;
    t = std::clamp(t, 0.0f, horizon + 0.25f);
    const float speed = std::sqrt(v.x*v.x + v.z*v.z);
    if (speed < 0.1f) return p + v*t; // no inferred launch direction at rest
    const float along = std::clamp((v.x*a.x + v.z*a.z)/speed, -12.0f, 8.0f);
    const float omega = turn_rate(v, a);
    float active = std::min(t, 0.6f);
    if (along < -0.01f) active = std::min(active, speed / -along);
    // Fixed, small midpoint quadrature, independent of flight duration.
    const int n = std::max(1, static_cast<int>(std::ceil(active / 0.05f)));
    const float dt = active/n;
    vec3_t out = p;
    for (int i=0; i<n; ++i) {
        const float mid = (i+0.5f)*dt;
        const float s = std::max(0.0f, speed + along*mid);
        const float c = std::cos(omega*mid), sn = std::sin(omega*mid);
        out.x += (v.x*c-v.z*sn) * (s/speed) * dt;
        out.z += (v.x*sn+v.z*c) * (s/speed) * dt;
    }
    const float final_speed = std::max(0.0f, speed+along*active);
    const float c = std::cos(omega*active), sn = std::sin(omega*active);
    out.x += (v.x*c-v.z*sn)*(final_speed/speed)*(t-active);
    out.z += (v.x*sn+v.z*c)*(final_speed/speed)*(t-active);
    out.y += v.y*t; // do not extrapolate terrain bumps as vertical acceleration
    return out;
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
    float k, float range, vec3_t& hit, float& time, int& steps) {
    const vec3_t d = aim-origin;
    const float len = d.length();
    if (!std::isfinite(len) || len < 0.01f || range < 0.01f) return false;
    vec3_t p{}, v = d*(b.speed/len); // relative coordinates avoid world-coordinate drift
    float previous_range = 0;
    for (int i=0; i<max_steps; ++i) {
        const vec3_t old = p;
        step(p, v, k, b.drag > 0 && std::isfinite(b.drag), time_step);
        ++steps;
        const float r = std::sqrt(p.x*p.x+p.z*p.z);
        if (!finite(p) || !std::isfinite(r)) return false;
        if (r >= range) {
            const float f = std::clamp((range-previous_range)/std::max(0.00001f,r-previous_range),0.0f,1.0f);
            hit = origin + old + (p-old)*f;
            time = (i+f)*time_step;
            return true;
        }
        if (r <= previous_range && i > 0) return false;
        previous_range = r;
    }
    return false;
}
inline Solution solve(const vec3_t& origin, const vec3_t& target, const vec3_t& velocity,
    const vec3_t& acceleration, const Ballistics& b, float age = 0) {
    Solution result{};
    if (!finite(origin) || !finite(target) || !finite(velocity) || !finite(acceleration)
        || !std::isfinite(b.speed) || b.speed < 50 || b.speed > 4000 || !std::isfinite(age)) return result;
    age = std::clamp(age, 0.0f, 0.25f);
    const float k = drag_coefficient(b, std::max(origin.y,target.y));
    float t = (target-origin).length()/b.speed;
    if (!std::isfinite(t) || t > horizon) return result;
    vec3_t future = propagate(target,velocity,acceleration,t+age);
    float elevation = 4.905f*t*t;
    for (int i=0; i<max_iterations; ++i) {
        const vec3_t aim{future.x, future.y+elevation, future.z};
        const float dx = future.x-origin.x, dz = future.z-origin.z;
        const float range = std::sqrt(dx*dx+dz*dz);
        vec3_t hit{};
        if (!trace(origin,aim,b,k,range,hit,t,result.steps)) return result;
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
        elevation += next.y-hit.y;
        future = next;
    }
    return result; // explicit failure, not an arbitrary final iterate
}
}
