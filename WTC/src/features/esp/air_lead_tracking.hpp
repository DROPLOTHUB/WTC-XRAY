#pragma once
#include "air_lead_prediction.hpp"
namespace esp::air_lead_tracking {
inline vec3_t plus(vec3_t a,vec3_t b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline vec3_t minus(vec3_t a,vec3_t b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline vec3_t scale(vec3_t a,float s){return {a.x*s,a.y*s,a.z*s};}
inline bool bounded(vec3_t v,float maximum){return air_lead_model::finite(v)&&air_lead_model::distance_between(v,{})<=maximum;}
inline vec3_t filter(vec3_t old,vec3_t next,float dt,float tau){return plus(old,scale(minus(next,old),1.f-std::exp(-dt/tau)));}
enum class source {none,api,positions};
inline const char* label(source s){return s==source::api ? "Velocidad API" : s==source::positions ? "Velocidad derivada de posiciones" : "Sin velocidad fiable";}
struct motion_state {
    vec3_t position{},measured{},filtered_velocity{},filtered_acceleration{},native_velocity{},native_acceleration{};
    double position_time=0,native_time=0,last_time=0;
    bool have_position=false,have_measured=false,have_native=false,have_time=false;
    source previous=source::none;
};
struct estimate {vec3_t velocity{},acceleration{};float dt=.03f;source provenance=source::none;bool valid=false,reset=false;};
inline estimate observe(motion_state& state,vec3_t position,double time,
    const vec3_t* native_velocity=nullptr,const vec3_t* native_acceleration=nullptr) {
    estimate out{};
    if(!air_lead_model::finite(position)||!std::isfinite(time)){state={};out.reset=true;return out;}
    const double elapsed=state.have_time ? time-state.last_time : 0;
    if(state.have_time && (elapsed<=0 || elapsed>.55)){state={};out.reset=true;}
    out.dt=elapsed>0 && elapsed<=.55 ? float(elapsed) : .03f;
    bool measured_now=false;vec3_t measured{};
    if(state.have_position) {
        const double dt=time-state.position_time;
        if(dt>=.025 && dt<=.55) {
            measured=scale(minus(position,state.position),float(1/dt));
            if(bounded(measured,950.f)) {
                measured_now=true;
                const auto acceleration=scale(minus(measured,state.measured),float(1/dt));
                state.filtered_velocity=state.have_measured ? filter(state.filtered_velocity,measured,float(dt),.18f) : measured;
                state.filtered_acceleration=state.have_measured && bounded(acceleration,160.f)
                    ? filter(state.filtered_acceleration,acceleration,float(dt),.32f) : vec3_t{};
                state.measured=measured;state.have_measured=true;
            } else {state.have_measured=false;state.filtered_acceleration={};out.reset=true;}
            state.position=position;state.position_time=time;
        }
    } else {state.position=position;state.position_time=time;state.have_position=true;}
    bool native_ok=native_velocity && bounded(*native_velocity,950.f);
    // A fresh measured displacement can disprove a zero-filled native read.
    // Do not reject a genuine stop just because an old EMA is still moving.
    if(native_ok && air_lead_model::distance_between(*native_velocity,{})<.5f && measured_now &&
        air_lead_model::distance_between(measured,{})>10.f)native_ok=false;
    if(native_ok) {
        out.velocity=*native_velocity;out.provenance=source::api;out.valid=true;
        const double dt=time-state.native_time;
        if(native_acceleration && bounded(*native_acceleration,160.f))out.acceleration=*native_acceleration;
        else if(state.have_native && dt>=.025 && dt<=.55) {
            const auto derived=scale(minus(*native_velocity,state.native_velocity),float(1/dt));
            if(bounded(derived,160.f))out.acceleration=filter(state.native_acceleration,derived,float(dt),.20f);
        }
        state.native_velocity=*native_velocity;state.native_acceleration=out.acceleration;
        state.native_time=time;state.have_native=true;
    } else {
        state.have_native=false;
        if(state.have_measured) {out.velocity=state.filtered_velocity;out.acceleration=state.filtered_acceleration;out.provenance=source::positions;out.valid=true;}
    }
    if(state.previous!=out.provenance)out.reset=true;
    state.previous=out.provenance;state.last_time=time;state.have_time=true;
    return out;
}
inline vec3_t lead_velocity(vec3_t previous,vec3_t current,float dt,vec3_t fallback) {
    if(!std::isfinite(dt)||dt<.001f||dt>.2f)return fallback;
    const auto derivative=scale(minus(current,previous),1.f/dt);
    return bounded(derivative,4000.f) ? derivative : fallback;
}
struct solve_context {
    float speed=0,mass=0,caliber=0,drag=0,delay=0;
    int weapon=-1,selected=-1;
    bool caliber_mm=false,heaviest=false,weapon_origin=false,turn=false,global_fallback=false;
    bool api_velocity=false,api_acceleration=false;
    bool operator==(const solve_context&) const=default;
};
}
