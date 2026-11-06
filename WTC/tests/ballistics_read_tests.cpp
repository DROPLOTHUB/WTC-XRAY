#include "../src/features/aimbot/ballistic_validation.hpp"
#include "../src/core/offset_api_mapping.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <cstdio>
struct vec {float x,y,z;};
int main(){
    constexpr std::uintptr_t unit=0x100000, movement=0x200000;
    constexpr std::uintptr_t unit_field=0x2100, speed_field=0x5C;
    vec sample{12.f,0.f,-3.f}, result{};
    std::uintptr_t pointer=movement,source=0;
    int reads=0;
    auto reader=[&](std::uintptr_t address,void* dest,std::size_t size){
        ++reads;
        if(address==unit+unit_field && size==sizeof(pointer)){
            std::memcpy(dest,&pointer,size);return true;
        }
        if(address==movement+speed_field && size==sizeof(sample)){
            std::memcpy(dest,&sample,size);return true;
        }
        return false;
    };
    using ballistic_validation::read_ground_velocity;
    assert(read_ground_velocity(unit,unit_field,speed_field,reader,result,source));
    assert(reads==2 && source==movement && result.x==12.f && result.z==-3.f);
    // Never interpret the pointer bytes as three floats at Unit.m_Velocity.
    reads=0;assert(!read_ground_velocity(unit,unit_field,0,reader,result,source) && reads==0);
    sample={};assert(read_ground_velocity(unit,unit_field,speed_field,reader,result,source));
    assert(result.x==0 && result.y==0 && result.z==0);
    sample.x=std::numeric_limits<float>::quiet_NaN();
    assert(!read_ground_velocity(unit,unit_field,speed_field,reader,result,source));
    sample={151.f,0.f,0.f};assert(!read_ground_velocity(unit,unit_field,speed_field,reader,result,source));
    pointer=0;reads=0;assert(!read_ground_velocity(unit,unit_field,speed_field,reader,result,source));
    assert(reads==1 && source==0);
    ballistic_validation::round_data round{920.f,8.3f,0.125f,0.52f,0.f};
    assert(ballistic_validation::valid_round(round));
    assert(std::fabs(ballistic_validation::caliber_mm(round)-125.f)<0.01f);
    float normalized=0.f;
    assert(ballistic_validation::normalize_caliber(0.125f,normalized) && normalized==0.125f);
    assert(ballistic_validation::normalize_caliber(125.f,normalized));
    assert(std::fabs(normalized-0.125f)<0.00001f);
    assert(!ballistic_validation::normalize_caliber(5000.f,normalized));
    round.caliber_m=125.f;assert(!ballistic_validation::valid_round(round));
    round.caliber_m=0.125f;round.speed=4001.f;assert(!ballistic_validation::valid_round(round));
    round.speed=920.f;round.mass=std::numeric_limits<float>::infinity();
    assert(!ballistic_validation::valid_round(round));
    offset_api_payload::manifest remote{};
    assert(!offset_api_mapping::ground_velocity_available(remote));
    assert(!offset_api_mapping::round_ballistics_available(remote));
    remote.entries["Unit.m_Velocity"]=unit_field;
    remote.entries["GroundMovement.m_Velocity"]=speed_field;
    assert(offset_api_mapping::ground_velocity_available(remote));
    remote.entries["Game.m_BallisticsCont"]=0x3F0;
    remote.entries["Ballistics.m_MuzzleSpeed"]=0x2140;
    remote.entries["Ballistics.m_Caliber"]=0x2150;
    remote.entries["Ballistics.m_Mass"]=0x214C;
    remote.entries["Ballistics.m_Length"]=0x2154;
    assert(offset_api_mapping::round_ballistics_available(remote));
    remote.entries["Ballistics.m_Caliber"]=0;
    assert(!offset_api_mapping::round_ballistics_available(remote));
    std::puts("PASS pointer-to-ground-velocity, stopped tank, active round units and API provenance");
}
