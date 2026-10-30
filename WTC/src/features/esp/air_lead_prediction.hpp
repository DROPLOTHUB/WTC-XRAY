#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Modelo exclusivo del indicador aéreo; no modifica el predictor del aim terrestre.
namespace esp::air_lead_model
{
    // ========================================================================
    // Constantes del modelo
    // ========================================================================

    // Horizonte máximo de búsqueda de intercepción (segundos). El coste de
    // solve() crece linealmente con este valor (barrido + bisección).
    // 4 s cubre ~3.2 km a 800 m/s; súbelo si necesitas tiros más largos.
    inline constexpr float kMaxHorizonSeconds = 4.0f;
    // A 0.25 s bracket / 2^15 is under 8 microseconds.
    inline constexpr int kBisectionIterations = 15;

    // Omega máxima soportada (rad/s). ~143°/s.
    inline constexpr float kMaxOmega = 2.5f;

    // Suelo físico de velocidad en la integración (m/s). No acelera targets
    // lentos: solo evita cruzar cero en giros con along muy negativo.
    inline constexpr float kMinSpeedFloor = 40.0f;

    // Constante de tiempo del filtro EMA de ω (segundos).
    inline constexpr float kTurnTau = 0.20f;

    // Saturación de la aceleración tangencial (m/s²). ~5g en la dirección de v̂.
    inline constexpr float kMaxAlong = 50.0f;

    // ========================================================================
    // Utilidades geométricas (sin depender de operadores de vec3_t)
    // ========================================================================

    inline bool finite(const vec3_t& p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }

    // Distancia entre dos puntos sin depender de operator- de vec3_t.
    inline float distance_between(const vec3_t& a, const vec3_t& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // ========================================================================
    // Estado del filtro de giro (por objetivo, indexado por uid en el caller)
    // ========================================================================

    struct TurnState
    {
        std::uint64_t owner_uid = 0;
        float         omega = 0.0f;
        float         last_dt = 0.1f;
        bool          valid = false;
    };

    // ========================================================================
    // update_turn_state
    //
    // Llamar EXACTAMENTE UNA VEZ por muestra y objetivo. Muta el estado
    // (EMA time-invariant de ω) y devuelve la ω filtrada y saturada que se
    // pasará luego a propagate() / solve() en modo sólo-lectura.
    //
    // dt_s debe ser el tiempo transcurrido desde la muestra anterior DEL
    // MISMO uid, no el frame_dt global.
    // ========================================================================
    inline float update_turn_state(const vec3_t& vel, const vec3_t& acceleration,
        float dt_s, std::uint64_t owner_uid,
        TurnState* state)
    {
        if (!state || !finite(vel) || !finite(acceleration) || !std::isfinite(dt_s) || dt_s<=0.f)
            return 0.0f;

        const float speed_sq =
            vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;

        // Velocidad demasiado baja: el modelo de giro no aplica.
        // Resetear ω para no arrastrar basura del frame anterior.
        if (speed_sq < 400.0f)
        {
            state->omega = 0.0f;
            state->valid = false;
            return 0.0f;
        }

        const float speed = std::sqrt(speed_sq);
        const float v_hat_x = vel.x / speed;
        const float v_hat_y = vel.y / speed;
        const float v_hat_z = vel.z / speed;

        // Descomposición tangencial / normal de la aceleración.
        float along =
            (vel.x * acceleration.x + vel.y * acceleration.y +
                vel.z * acceleration.z) / speed;

        const float an_x = acceleration.x - along * v_hat_x;
        const float an_y = acceleration.y - along * v_hat_y;
        const float an_z = acceleration.z - along * v_hat_z;
        const float an_mag = std::sqrt(an_x * an_x + an_y * an_y + an_z * an_z);

        const float omega_instant = (std::clamp)(an_mag / speed,0.f,kMaxOmega);

        // EMA time-invariant: alpha = 1 - exp(-dt/tau).
        const float dt_safe = (std::clamp)(dt_s, 0.001f, 1.0f);
        state->last_dt = dt_safe;

        if (!state->valid || state->owner_uid != owner_uid)
        {
            // Target nuevo o estado sin inicializar: adoptar el valor
            // instantáneo sin filtrar (evita heredar ω del anterior).
            state->omega = omega_instant;
            state->owner_uid = owner_uid;
            state->valid = true;
        }
        else
        {
            const float alpha = 1.0f - std::exp(-dt_safe / kTurnTau);
            state->omega = alpha * omega_instant + (1.0f - alpha) * state->omega;
        }

        return (std::clamp)(state->omega, 0.0f, kMaxOmega);
    }

    // ========================================================================
    // propagate
    //
    // FUNCIÓN PURA. No muta estado. Recibe ω ya filtrada. Segura para
    // paralelizar siempre que pos/vel/acc/ω sean inmutables durante la
    // evaluación.
    //
    // Modelo: aceleración tangencial + normal descompuestas; rotación de la
    // velocidad alrededor del eje (v × a) mediante Rodrigues; el horizonte de
    // validez de la aceleración limita también la rotación.
    // ========================================================================
    struct motion_t {vec3_t point{},velocity{};};
    inline motion_t propagate_state(const vec3_t& pos,const vec3_t& vel,
        const vec3_t& acceleration,float seconds,float omega,bool turn=false)
    {
        motion_t out{pos,vel};
        if(!finite(pos)||!finite(vel)||!finite(acceleration)||!std::isfinite(seconds))return out;
        omega=std::isfinite(omega) ? (std::clamp)(omega,0.f,kMaxOmega) : 0.f;
        seconds=(std::clamp)(seconds,0.f,8.f);
        const float active=(std::min)(seconds,.85f);
        const float speed_sq=vel.x*vel.x+vel.y*vel.y+vel.z*vel.z;
        const auto linear=[&]() {
            const float distance=active*(seconds-.5f*active);
            return motion_t{{pos.x+vel.x*seconds+acceleration.x*distance,
                pos.y+vel.y*seconds+acceleration.y*distance,pos.z+vel.z*seconds+acceleration.z*distance},
                {vel.x+acceleration.x*active,vel.y+acceleration.y*active,vel.z+acceleration.z*active}};
        };
        if(!turn||speed_sq<400.f||omega<1e-4f)return linear();
        const double speed=std::sqrt(static_cast<double>(speed_sq));
        const double ax=double(vel.y)*acceleration.z-double(vel.z)*acceleration.y;
        const double ay=double(vel.z)*acceleration.x-double(vel.x)*acceleration.z;
        const double az=double(vel.x)*acceleration.y-double(vel.y)*acceleration.x;
        const double magnitude=std::sqrt(ax*ax+ay*ay+az*az);
        if(magnitude<1e-4)return linear();
        const double ux=vel.x/speed,uy=vel.y/speed,uz=vel.z/speed;
        const double nx=ax/magnitude,ny=ay/magnitude,nz=az/magnitude;
        const double tx=ny*uz-nz*uy,ty=nz*ux-nx*uz,tz=nx*uy-ny*ux;
        const double along=(std::clamp)(ux*acceleration.x+uy*acceleration.y+uz*acceleration.z,
            -double(kMaxAlong),double(kMaxAlong));
        const double floor=(std::min)(double(kMinSpeedFloor),speed);
        const double ramp=along<0 ? (std::min)(double(active),(speed-floor)/-along) : double(active);
        const double w=omega,theta=w*ramp;
        const double sine=std::sin(theta),cosine=std::cos(theta);
        // Exact integrals of (speed + along*t) rotated around v x a.
        // Double precision avoids cancellation for nearly straight flight.
        double forward=speed*sine/w+along*(ramp*sine/w+(cosine-1)/(w*w));
        double sideways=speed*(1-cosine)/w+along*(-ramp*cosine/w+sine/(w*w));
        const double angle=w*active,c=std::cos(angle),s=std::sin(angle);
        if(ramp<active) {
            forward+=floor*(s-sine)/w;
            sideways+=floor*(cosine-c)/w;
        }
        const double final_speed=(std::max)(floor,speed+along*active);
        const double vx=final_speed*(ux*c+tx*s),vy=final_speed*(uy*c+ty*s),vz=final_speed*(uz*c+tz*s);
        const double coast=seconds-active;
        out.point={float(pos.x+ux*forward+tx*sideways+vx*coast),
            float(pos.y+uy*forward+ty*sideways+vy*coast),float(pos.z+uz*forward+tz*sideways+vz*coast)};
        out.velocity={float(vx),float(vy),float(vz)};
        return out;
    }
    inline vec3_t propagate(const vec3_t& pos,const vec3_t& velocity,
        const vec3_t& acceleration,float seconds,float omega,bool turn=false) {
        return propagate_state(pos,velocity,acceleration,seconds,omega,turn).point;
    }

    // ========================================================================
    // solve
    //
    // Recibe ω ya filtrada. NO toca TurnState. Puro.
    // Barrido grueso de 0.25 s hasta kMaxHorizonSeconds + bisección fina.
    // ========================================================================
    struct solution_t
    {
        vec3_t point{};
        float  time = 0.0f;
        bool   valid = false;
    };

    inline solution_t solve(const vec3_t& origin, const vec3_t& target,
        const vec3_t& velocity, const vec3_t& acceleration,
        float muzzle_speed, float mass, float caliber_mm, float drag_area,
        float omega, float prelaunch_seconds = 0.0f, bool turn = false)
    {
        solution_t out{};
        if (!finite(origin) || !finite(target) || !finite(velocity) ||
            !finite(acceleration) || !std::isfinite(prelaunch_seconds) || !std::isfinite(omega) ||
            !std::isfinite(muzzle_speed) || muzzle_speed < 50.0f ||
            muzzle_speed > 4000.0f)
            return out;

        // drag_area es área efectiva; si faltan datos de masa, degradar a
        // balística sin drag en vez de dividir por cero o acelerar la bala.
        float area = drag_area;
        if ((!std::isfinite(area) || area <= 0.0f) &&
            std::isfinite(caliber_mm) && caliber_mm > 0.0f)
        {
            const float radius = caliber_mm * 0.0005f;
            area = 0.32f * 3.14159265f * radius * radius;
        }

        float drag_k = 0.0f;
        if (std::isfinite(mass) && mass > 0.00001f &&
            std::isfinite(area) && area > 0.0f)
        {
            const float altitude = (std::max)(0.0f, 0.5f * (origin.y + target.y));
            const float density  = 1.225f * std::exp(-altitude / 9000.0f);
            drag_k = (std::clamp)(0.5f * density * area / mass, 0.0f, 0.02f);
        }

        const float delay = (std::clamp)(prelaunch_seconds, 0.0f, 0.75f);

        const auto travel = [&](float t) {
            // dv/dt = -k·v²  →  recorrido ln(1 + k·v0·t)/k, disipativo.
            return drag_k < 1e-7f
                ? muzzle_speed * t
                : std::log1p(drag_k * muzzle_speed * t) / drag_k;
        };

        const auto aim_at = [&](float t) {
            const vec3_t p = propagate(target, velocity, acceleration,
                                       t + delay, omega, turn);
            return vec3_t{ p.x, p.y + 4.905f * t * t, p.z }; // drop por gravedad
        };

        const auto residual = [&](float t) {
            return distance_between(aim_at(t), origin) - travel(t);
        };

        // Barrido grueso. kMaxHorizonSeconds / 0.25 s muestras.
        constexpr float kSweepStep  = 0.25f;
        constexpr int   kSweepCount =
            static_cast<int>(kMaxHorizonSeconds / kSweepStep);

        float lo = 0.0f, hi = 0.0f;
        const float previous = residual(0.0f);
        if (!std::isfinite(previous) || previous < 0.01f)
            return out;

        for (int step = 1; step <= kSweepCount; ++step)
        {
            const float t     = kSweepStep * static_cast<float>(step);
            const float error = residual(t);
            if (!std::isfinite(error))
                return out;
            if (error <= 0.0f)
            {
                hi = t;
                lo = t - kSweepStep;
                break;
            }
        }
        if (hi == 0.0f)
            return out; // fuera del horizonte kMaxHorizonSeconds

        for (int i = 0; i < kBisectionIterations; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            if (residual(mid) > 0.0f)
                lo = mid;
            else
                hi = mid;
        }

        out.time  = 0.5f * (lo + hi);
        out.point = aim_at(out.time);
        out.valid = finite(out.point) && std::isfinite(out.time) &&
                    out.time >= 0.0f && out.time <= kMaxHorizonSeconds;
        return out;
    }
}