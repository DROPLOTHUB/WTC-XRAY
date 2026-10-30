#include <cmath>
struct vec3_t {float x=0,y=0,z=0;};
#define air_lead_model air_lead_v510_reference
#include "fixtures/air_lead_v510_reference.hpp"
#undef air_lead_model
#include "../src/features/esp/air_lead_prediction.hpp"
#include "../src/features/aimbot/aircraft_ballistics_policy.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <string>
int main(){
    using namespace esp::air_lead_model;
    using aimbot::aircraft_ballistics_policy::caliber_for_air_solver;
    const vec3_t origin{},target{0,0,900},vel{100,0,0};
    const auto mm=solve(origin,target,vel,{},850,.045f,caliber_for_air_solver(12.7f,true),0,0);
    const auto meters=solve(origin,target,vel,{},850,.045f,caliber_for_air_solver(.0127f,false),0,0);
    assert(mm.valid && meters.valid);
    assert(distance_between(mm.point,meters.point)<.001f && std::fabs(mm.time-meters.time)<.00001f);
    TurnState state{};
    const auto first=update_turn_state({200,0,0},{0,20,0},.03f,0x100000001ULL,&state);
    const auto second=update_turn_state({200,0,0},{0,40,0},.03f,0x200000001ULL,&state);
    assert(std::fabs(first-.1f)<.00001f && std::fabs(second-.2f)<.00001f);
    assert(state.owner_uid==0x200000001ULL);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const auto preserved=state;
    assert(update_turn_state(vel,{},nan,7,&state)==0.f);
    assert(state.owner_uid==preserved.owner_uid && state.omega==preserved.omega);
    assert(!solve(origin,target,vel,{},850,0,0,0,0,nan).valid);
    assert(!solve(origin,target,vel,{},850,0,0,0,nan).valid);
    // Deterministic comparison against the shipped 22-step solver.
    std::mt19937 rng(510);
    std::uniform_real_distribution<float> unit(0.f,1.f);
    float max_point=0,max_time=0;int valid=0;
    for(int i=0;i<2000;++i){
        vec3_t o{0,5000*unit(rng),0};
        vec3_t t{(unit(rng)-.5f)*1000,o.y+(unit(rng)-.5f)*300,100+4000*unit(rng)};
        vec3_t v{(unit(rng)-.5f)*500,(unit(rng)-.5f)*80,(unit(rng)-.5f)*300};
        vec3_t a{0,0,30*unit(rng)};
        float speed=500+700*unit(rng),mass=.02f+.2f*unit(rng),caliber=7+23*unit(rng);
        float drag=(i%3==0)?0.f:.00002f,omega=unit(rng),delay=.2f*unit(rng);
        const bool turn=false; // legacy straight-flight regression; exact turns have independent analytic tests
        const auto old=esp::air_lead_v510_reference::solve(o,t,v,a,speed,mass,caliber,drag,omega,delay,turn);
        const auto now=solve(o,t,v,a,speed,mass,caliber,drag,omega,delay,turn);
        assert(old.valid==now.valid);
        if(!now.valid)continue;
        ++valid;max_point=(std::max)(max_point,distance_between(old.point,now.point));
        max_time=(std::max)(max_time,std::fabs(old.time-now.time));
    }
    assert(valid>100 && max_point<.02f && max_time<.00001f);
    std::ifstream f(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/esp/esp.hpp");assert(f);
    const std::string source{std::istreambuf_iterator<char>(f),{}};
    assert(source.find("local_bal.caliber,local_bal.caliber_in_mm")!=std::string::npos);
    assert(source.find("motion.dt,unit_base,&cache.turn_state")!=std::string::npos);
    std::printf("PASS caliber m/mm, 64-bit owners, finite timing; 2000 cases (%d valid), max point %.6f m, max time %.9f s\n",valid,max_point,max_time);
}
