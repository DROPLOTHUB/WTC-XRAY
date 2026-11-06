#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include "projectile_ecs.hpp"

// Kinematic proximity warning only. No target/guidance/lock information is read.
namespace missile_alerts {
using vec3 = projectile_ecs::Vec3;
struct Settings {
    bool enabled = false;
    bool audio_enabled = false;
    double max_range_m = 3000.0;
    double min_closure_mps = 30.0;
    double max_time_s = 12.0;
    double max_miss_distance_m = 150.0;
    double audio_interval_s = 3.0;
};
inline std::mutex settings_mutex;
inline Settings settings;
inline Settings capture_settings() { std::lock_guard lock(settings_mutex); return settings; }
inline Settings sanitize(Settings value) {
    const auto bounded = [](double v, double fallback, double lo, double hi) {
        return std::isfinite(v) ? (std::clamp)(v, lo, hi) : fallback;
    };
    value.max_range_m = bounded(value.max_range_m, 3000, 1, 100000);
    value.min_closure_mps = bounded(value.min_closure_mps, 30, 1, 10000);
    value.max_time_s = bounded(value.max_time_s, 12, 0.1, 120);
    value.max_miss_distance_m = bounded(value.max_miss_distance_m, 150, 1, 5000);
    value.audio_interval_s = bounded(value.audio_interval_s, 3, 1, 60);
    return value;
}
inline void apply_settings(Settings value) { std::lock_guard lock(settings_mutex); settings = sanitize(value); }
struct Readout {
    bool approaching = false;
    double range_m = 0;
    double closure_mps = 0;
    double time_to_closest_s = 0;
    double miss_distance_m = 0;
    // Unit direction from local observer to threat in WORLD axes (not camera).
    vec3 direction{};
    double world_bearing_degrees = 0; // atan2(z,x), not a compass heading
    vec3 position{};
};
inline bool finite(vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline Readout evaluate(vec3 position, vec3 velocity, bool is_bomb, const char* name,
    vec3 local_position, vec3 local_velocity, bool local_valid, Settings cfg) {
    Readout result;
    cfg = sanitize(cfg);
    if (!cfg.enabled || !local_valid || is_bomb || !name || !*name ||
        projectile_ecs::name_has_flare(name) || projectile_ecs::name_has_mm(name) ||
        !finite(position) || !finite(velocity) || !finite(local_position) || !finite(local_velocity)) return result;
    const double x = double(position.x) - local_position.x, y = double(position.y) - local_position.y,
        z = double(position.z) - local_position.z;
    const double vx = double(velocity.x) - local_velocity.x, vy = double(velocity.y) - local_velocity.y,
        vz = double(velocity.z) - local_velocity.z;
    const double range = std::hypot(x, y, z), speed2 = vx*vx + vy*vy + vz*vz;
    if (range < 0.001 || range > cfg.max_range_m || speed2 < 0.001) return result;
    const double dot = x*vx + y*vy + z*vz, closure = -dot / range;
    const double time = -dot / speed2;
    if (closure < cfg.min_closure_mps || time <= 0 || time > cfg.max_time_s) return result;
    const double miss = std::hypot(x + vx*time, y + vy*time, z + vz*time);
    if (miss > cfg.max_miss_distance_m) return result;
    result.approaching = true;
    result.range_m = range; result.closure_mps = closure;
    result.time_to_closest_s = time; result.miss_distance_m = miss;
    result.direction = {float(x/range), float(y/range), float(z/range)};
    result.world_bearing_degrees = std::atan2(z, x) * 180.0 / 3.14159265358979323846;
    result.position = position;
    return result;
}
// Render-thread-owned state. Aggregate once per frame before requesting audio.
struct State {
    Readout readout{};
    double last_audio_s = -std::numeric_limits<double>::infinity();
    vec3 previous_local{};
    vec3 local_velocity{};
    double previous_local_s = 0;
    bool have_local = false;
    bool velocity_valid = false;
    void reset() { *this = State{}; }
    void begin_frame(vec3 local, bool valid, double now_s) {
        readout = {};
        if (!valid || !finite(local) || !std::isfinite(now_s)) { have_local = false; velocity_valid = false; return; }
        if (have_local && now_s == previous_local_s) return; // cached ECS frame
        velocity_valid = false;
        const double dt = now_s - previous_local_s;
        if (have_local && dt > 0 && dt <= 0.5) {
            local_velocity = {float((local.x-previous_local.x)/dt), float((local.y-previous_local.y)/dt),
                float((local.z-previous_local.z)/dt)};
            // Reject observer teleports rather than fabricate closure warnings.
            velocity_valid = finite(local_velocity) && std::hypot(local_velocity.x, local_velocity.y, local_velocity.z) <= 3000;
        }
        previous_local = local; previous_local_s = now_s; have_local = true;
    }
    void consider(const Readout& candidate) {
        if (candidate.approaching && (!readout.approaching ||
            candidate.time_to_closest_s < readout.time_to_closest_s ||
            (candidate.time_to_closest_s == readout.time_to_closest_s && candidate.range_m < readout.range_m))) readout = candidate;
    }
    bool consume_audio(double now_s, Settings cfg) {
        cfg = sanitize(cfg);
        if (!cfg.enabled || !cfg.audio_enabled || !readout.approaching || !std::isfinite(now_s)) return false;
        if (now_s < last_audio_s) { last_audio_s = now_s; return false; }
        if (now_s - last_audio_s < cfg.audio_interval_s) return false;
        last_audio_s = now_s;
        return true;
    }
};
}
