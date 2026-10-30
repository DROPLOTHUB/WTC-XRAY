#include <cmath>
struct vec3_t {float x=0,y=0,z=0;};
#include "../src/features/esp/air_lead_prediction.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
int main() {
    using esp::air_lead_model::solve;
    using esp::air_lead_model::distance_between;
    const vec3_t muzzle{0,0,0},target{0,0,900};
    const auto static_target=solve(muzzle,target,{}, {},800,0,0,0,0);
    assert(static_target.valid && std::fabs(static_target.point.x)<.01f);
    const auto moving=solve(muzzle,target,{300,0,0},{},800,0,0,0,0);
    const auto opposite=solve(muzzle,target,{-300,0,0},{},800,0,0,0,0);
    assert(moving.valid && opposite.valid && moving.point.x>300.f && opposite.point.x< -300.f);
    const vec3_t nan{std::numeric_limits<float>::quiet_NaN(),0,0};
    assert(!solve(muzzle,target,nan,{},800,0,0,0,0).valid);
    assert(!solve(muzzle,target,{300,0,0},{},0,0,0,0,0).valid);
    const vec3_t turning{180,0,0},acc{0,0,80};
    const auto straight=solve(muzzle,target,turning,acc,800,0,0,0,.4f,0,false);
    const auto turn=solve(muzzle,target,turning,acc,800,0,0,0,.4f,0,true);
    assert(straight.valid && turn.valid && distance_between(straight.point,turn.point)>1.f);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    std::ifstream esp_file(root/"features/esp/esp.hpp");assert(esp_file);
    const std::string esp{std::istreambuf_iterator<char>(esp_file),{}};
    const auto begin=esp.find("auto run_air_lead =");
    const auto end=esp.find("if (sdk::aimbot_enabled && frame.local_is_plane)",begin);
    assert(begin!=std::string::npos && end!=std::string::npos);
    const std::string lead=esp.substr(begin,end-begin);
    assert(lead.find("prediction_velocity = air_velocity;")!=std::string::npos);
    assert(lead.find("get_shooter_velocity")==std::string::npos);
    assert(lead.find("&shooter_velocity")==std::string::npos);
    assert(lead.find("300.0f")==std::string::npos);
    std::ifstream ammo_file(root/"features/aimbot/aimbot.hpp");assert(ammo_file);
    const std::string ammo{std::istreambuf_iterator<char>(ammo_file),{}};
    assert(ammo.find("!sdk::air_lead_use_heaviest_weapon && GetBallisticsInfo()")!=std::string::npos);
    assert(ammo.find("aircraft_global_fallback=true;")!=std::string::npos);
    std::puts("PASS historical A2A equation and local container fallback without synthetic muzzle speed");
}
