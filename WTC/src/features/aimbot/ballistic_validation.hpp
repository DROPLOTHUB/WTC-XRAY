#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ballistic_validation {
struct round_data {
    float speed=0, mass=0, caliber_m=0, length_m=0, max_distance=0;
};
// Accept either metres or millimetres from the provider/game, but never
// infer units from a value outside both physically plausible ranges.
inline bool normalize_caliber(float raw,float& meters) {
    if(!std::isfinite(raw))return false;
    if(raw>=0.005f && raw<=0.5f){meters=raw;return true;}
    if(raw>=5.f && raw<=500.f){meters=raw*0.001f;return true;}
    return false;
}
inline bool valid_round(const round_data& r) {
    return std::isfinite(r.speed) && r.speed>=50.f && r.speed<=4000.f &&
           std::isfinite(r.mass) && r.mass>0.f && r.mass<=5000.f &&
           std::isfinite(r.caliber_m) && r.caliber_m>=0.005f && r.caliber_m<=0.5f &&
           std::isfinite(r.length_m) && r.length_m>=0.f && r.length_m<=10.f &&
           std::isfinite(r.max_distance) && r.max_distance>=0.f && r.max_distance<=500000.f;
}
inline float caliber_mm(const round_data& r) {return r.caliber_m*1000.f;}
inline bool plausible_pointer(std::uintptr_t value) {
    return value>=0x10000 && value<=0x00007FFFFFFFF000ULL;
}
// Unit.m_Velocity is a POINTER according to the provider's schema.
// GroundMovement.m_Velocity is a vec3_t at a second, independent offset.
template<class Vector,class Reader>
bool read_ground_velocity(std::uintptr_t unit,std::uintptr_t pointer_offset,
    std::uintptr_t vector_offset,Reader&& reader,Vector& result,std::uintptr_t& source) {
    source=0;
    if(!plausible_pointer(unit)||pointer_offset==0||pointer_offset>0x100000||
       vector_offset==0||vector_offset>0x1000)return false;
    std::uintptr_t movement=0;
    if(!reader(unit+pointer_offset,&movement,sizeof(movement))||!plausible_pointer(movement))return false;
    Vector candidate{};
    if(!reader(movement+vector_offset,&candidate,sizeof(candidate)))return false;
    if(!std::isfinite(candidate.x)||!std::isfinite(candidate.y)||!std::isfinite(candidate.z))return false;
    const double speed_squared=double(candidate.x)*candidate.x+
        double(candidate.y)*candidate.y+double(candidate.z)*candidate.z;
    if(speed_squared>150.0*150.0)return false;
    result=candidate;source=movement;
    return true; // Zero velocity is valid; a failed memory read never is.
}
}
