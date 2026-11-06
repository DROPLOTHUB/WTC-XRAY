#pragma once
#include "air_intercept.hpp"
#include "air_lead_tracking.hpp"
#include "../aimbot/ballistic_validation.hpp"
#include "g2a_settings.hpp"

namespace esp::g2a_lead {

inline bool bounded(const vec3_t& v, double limit) {
    return air_lead_model::finite(v) &&
        double(v.x)*v.x+double(v.y)*v.y+double(v.z)*v.z<=limit*limit;
}
// Reuse the existing ground caliber/length drag-area convention, not the
// aircraft solver's generic caliber-only fallback. This is a geometry MODEL,
// not measured Cd: neither guided weapons nor rocket propulsion are simulated.
// air_intercept applies 1/2*rho*area/mass, while ground uses rho*area/mass.
inline bool ammunition(const ballistic_validation::round_data& round,
                       air_intercept::ammunition& out) {
    out={};
    if(!ballistic_validation::valid_round(round) || round.length_m<=0) return false;
    const float radius=round.caliber_m*.5f;
    const float area=3.14159265f*radius*radius*round.length_m;
    if(!std::isfinite(area) || area<=0) return false;
    out={round.speed,round.mass,round.caliber_m*1000.f,area};
    return true;
}
// G2A owns its histories: never modifies the A2A tracker or ground predictor.
struct motion_sample {
    air_lead_tracking::estimate motion{};
    float omega=0;
    const char* acceleration="Sin aceleracion fiable";
};
struct motion_track {
    air_lead_tracking::motion_state history{};
    air_lead_model::TurnState turn{};
};
inline motion_sample observe(motion_track& track,vec3_t position,double time,
    const vec3_t* velocity,const vec3_t* acceleration,float speed_limit) {
    motion_sample out{};
    auto& h=track.history;
    if(!air_lead_model::finite(position) || !std::isfinite(time) ||
       (velocity && !bounded(*velocity,speed_limit)) || (acceleration && !bounded(*acceleration,160))) {
        track={};return out;
    }
    const double dt=h.have_time ? time-h.last_time : 0;
    if(h.have_time && (dt<.025 || dt>.2)) {track={};return out;}
    // Discontinuities cannot be hidden by a plausible native velocity.
    if(h.have_position && air_lead_model::distance_between(position,h.position)>speed_limit*dt+2) {
        track={};return out;
    }
    // Reject implausible velocity jumps instead of silently converting them to zero acceleration.
    if(h.have_native && velocity && dt>0 &&
       !bounded(air_lead_tracking::scale(air_lead_tracking::minus(*velocity,h.native_velocity),float(1/dt)),160)) {
        track={};return out;
    }
    const bool derivative=h.have_native && velocity;
    const bool positions=h.have_measured;
    out.motion=air_lead_tracking::observe(h,position,time,velocity,acceleration);
    if(!out.motion.valid)return out; // retain first position to warm up fallback
    if(!bounded(out.motion.velocity,speed_limit)) {track={};return {};}
    if(out.motion.reset)track.turn={};
    out.acceleration=acceleration && out.motion.provenance==air_lead_tracking::source::api ? "Aceleracion API" :
        derivative || positions ? "Aceleracion estimada (filtrada)" : "Aceleracion no disponible: velocidad constante";
    out.omega=air_lead_model::update_turn_state(out.motion.velocity,out.motion.acceleration,out.motion.dt,1,&track.turn);
    return out;
}
inline air_intercept::solution solve_motion(const vec3_t& origin,const vec3_t& target,
    const motion_sample& own,const motion_sample& enemy,float age,const ballistic_validation::round_data& round) {
    air_intercept::ammunition ammo{};
    if(!own.motion.valid || !enemy.motion.valid || !bounded(own.motion.velocity,150) ||
       !bounded(enemy.motion.velocity,950) || !bounded(own.motion.acceleration,160) ||
       !bounded(enemy.motion.acceleration,160) || !std::isfinite(enemy.omega) || enemy.omega<0 ||
       enemy.omega>air_lead_model::kMaxOmega || !std::isfinite(age) || age<0 || age>.2f ||
       !ammunition(round,ammo))return {};
    air_intercept::encounter e{};
    e.origin=origin;e.target=target;e.shooter_velocity=own.motion.velocity;
    e.shooter_acceleration=own.motion.acceleration;e.target_velocity=enemy.motion.velocity;
    e.target_acceleration=enemy.motion.acceleration;e.target_omega=enemy.omega;
    e.turn=true;e.shooter_known=true;e.sample_age=age;
    return air_intercept::solve(e,ammo);
}
inline air_intercept::solution solve(const vec3_t& origin,const vec3_t& target,
    const vec3_t& own_velocity,bool own_known,const vec3_t& target_velocity,bool target_known,
    float age,const ballistic_validation::round_data& round) {
    air_intercept::ammunition ammo{};
    // Never silently assume a stationary shooter/target after a failed read.
    if(!own_known || !target_known || !bounded(own_velocity,150) ||
       !bounded(target_velocity,950) || !std::isfinite(age) || age<0 || age>.2f ||
       !ammunition(round,ammo)) return {};
    air_intercept::encounter e{};
    e.origin=origin;e.target=target;e.shooter_velocity=own_velocity;
    e.target_velocity=target_velocity;e.shooter_known=true;e.sample_age=age;
    // Constant world velocity is deliberate: no fabricated acceleration or turns.
    return air_intercept::solve(e,ammo);
}
inline const char* reason(air_intercept::failure failure) {
    switch(failure) {
    case air_intercept::failure::none:return "G2A: trayectoria aproximada (geometria de municion)";
    case air_intercept::failure::horizon:return "G2A fuera del horizonte (4 s)";
    case air_intercept::failure::budget:return "G2A limite de calculo";
    case air_intercept::failure::convergence:return "G2A sin convergencia";
    default:return "G2A sin municion/movimiento fiable o snapshot >200 ms";
    }
}
}
