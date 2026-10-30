// Frozen v5.10 reference for numerical comparison only.
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
        std::uint32_t owner_uid = 0;
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
        float dt_s, std::uint32_t owner_uid,
        TurnState* state)
    {
        if (!state || !finite(vel) || !finite(acceleration))
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
        along = (std::clamp)(along, -kMaxAlong, kMaxAlong);

        const float an_x = acceleration.x - along * v_hat_x;
        const float an_y = acceleration.y - along * v_hat_y;
        const float an_z = acceleration.z - along * v_hat_z;
        const float an_mag = std::sqrt(an_x * an_x + an_y * an_y + an_z * an_z);

        const float omega_instant = an_mag / speed;

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
    inline vec3_t propagate(const vec3_t& pos, const vec3_t& vel,
        const vec3_t& acceleration, float seconds,
        float omega, bool turn = false)
    {
        if (!finite(pos) || !finite(vel) || !finite(acceleration))
            return pos;

        // Saturación defensiva de ω (incluye NaN).
        if (!std::isfinite(omega))
            omega = 0.0f;
        else
            omega = (std::clamp)(omega, 0.0f, kMaxOmega);

        seconds = (std::clamp)(seconds, 0.0f, 8.0f);
        const float active = (std::min)(seconds, 0.85f);

        const float speed_sq =
            vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;

        // Trayectoria lineal (fallback y caso sin giro). Solo se calcula
        // cuando se va a usar.
        const auto linear_trajectory = [&]() {
            const float lin_x =
                acceleration.x * (active * (seconds - 0.5f * active));
            const float lin_y =
                acceleration.y * (active * (seconds - 0.5f * active));
            const float lin_z =
                acceleration.z * (active * (seconds - 0.5f * active));
            return vec3_t{
                pos.x + vel.x * seconds + lin_x,
                pos.y + vel.y * seconds + lin_y,
                pos.z + vel.z * seconds + lin_z
            };
            };

        if (!turn || speed_sq < 400.0f || omega < 1e-4f)
            return linear_trajectory();

        const float speed = std::sqrt(speed_sq);
        const float v_hat_x = vel.x / speed;
        const float v_hat_y = vel.y / speed;
        const float v_hat_z = vel.z / speed;

        // Componente tangencial (cambia |v|).
        float along =
            (vel.x * acceleration.x + vel.y * acceleration.y +
                vel.z * acceleration.z) / speed;
        along = (std::clamp)(along, -kMaxAlong, kMaxAlong);

        // Eje de rotación = v × a. Perpendicular a v y a a_normal.
        const float ax_x = vel.y * acceleration.z - vel.z * acceleration.y;
        const float ax_y = vel.z * acceleration.x - vel.x * acceleration.z;
        const float ax_z = vel.x * acceleration.y - vel.y * acceleration.x;
        const float ax_mag = std::sqrt(ax_x * ax_x + ax_y * ax_y + ax_z * ax_z);

        if (ax_mag < 1e-4f)
            return linear_trajectory();

        const float axis_x = ax_x / ax_mag;
        const float axis_y = ax_y / ax_mag;
        const float axis_z = ax_z / ax_mag;

        // Suelo de velocidad: NO acelera objetivos lentos, solo evita que
        // |v| cruce cero si `along` es muy negativo.
        const float speed_floor = (std::min)(kMinSpeedFloor, speed);

        float rx = pos.x, ry = pos.y, rz = pos.z;
        float cvx = vel.x, cvy = vel.y, cvz = vel.z;
        float elapsed = 0.0f;

        while (elapsed < seconds)
        {
            const bool in_active = (elapsed < active);

            float dt;
            if (in_active)
            {
                dt = (std::min)((std::min)(0.05f, active - elapsed),
                    seconds - elapsed);
            }
            else
            {
                dt = seconds - elapsed; // un solo salto recto tras el activo
            }
            if (dt < 1e-6f)
                break;

            if (in_active)
            {
                const float mid = elapsed + 0.5f * dt;
                const float turn_time = (std::min)(mid, active);
                const float target_speed =
                    (std::max)(speed_floor, speed + along * turn_time);

                const float angle = omega * dt;
                const float c = std::cos(angle);
                const float s = std::sin(angle);

                // Rodrigues: v' = v·c + (axis × v)·s + axis·(axis·v)·(1-c)
                const float cross_x = axis_y * cvz - axis_z * cvy;
                const float cross_y = axis_z * cvx - axis_x * cvz;
                const float cross_z = axis_x * cvy - axis_y * cvx;

                const float dot = axis_x * cvx + axis_y * cvy + axis_z * cvz;

                float nvx = cvx * c + cross_x * s + axis_x * dot * (1.0f - c);
                float nvy = cvy * c + cross_y * s + axis_y * dot * (1.0f - c);
                float nvz = cvz * c + cross_z * s + axis_z * dot * (1.0f - c);

                const float n_mag = std::sqrt(nvx * nvx + nvy * nvy + nvz * nvz);
                if (n_mag > 1e-4f)
                {
                    const float scale = target_speed / n_mag;
                    nvx *= scale; nvy *= scale; nvz *= scale;
                }
                cvx = nvx; cvy = nvy; cvz = nvz;
            }

            rx += cvx * dt;
            ry += cvy * dt;
            rz += cvz * dt;
            elapsed += dt;
        }

        return vec3_t{ rx, ry, rz };
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
            !finite(acceleration) ||
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

        for (int i = 0; i < 22; ++i)
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