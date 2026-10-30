#include "../src/features/aimbot/aircraft_ballistics_policy.hpp"
#include <cassert>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
int main(){
 using namespace aimbot::aircraft_ballistics_policy;
 assert(real_speed(800.f));assert(real_speed(50.f));assert(real_speed(4000.f));
 assert(!real_speed(0.f));assert(!real_speed(-1.f));assert(!real_speed(4001.f));
 assert(!real_speed(std::numeric_limits<float>::quiet_NaN()));
 assert(!real_speed(std::numeric_limits<float>::infinity()));
 assert(usable_drag_geometry(.1f,.02f));
 assert(!usable_drag_geometry(0.f,.02f));assert(!usable_drag_geometry(.1f,0.f));
 assert(!usable_drag_geometry(.1f,20.f));
 // Missing optional geometry must not invalidate a measured muzzle speed.
 assert(real_speed(800.f) && !usable_drag_geometry(0.f,0.f));
 const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
 std::ifstream in(root/"features/aimbot/aimbot.hpp");assert(in);
 const std::string s{std::istreambuf_iterator<char>(in),{}};
 assert(s.find("if (!have_geometry) {mass=0.f;caliber=0.f;drag=0.f;}")!=std::string::npos);
 assert(s.find("if (!aircraft_ballistics_policy::real_speed(speed)) {")!=std::string::npos);
 assert(s.find("uintptr_t local_unit=snapshot_owner")!=std::string::npos);
 assert(s.find("owner!=last_owner || epoch!=last_epoch")!=std::string::npos);
 assert(s.find("ReadOptional(ammo+0x20,&speed,sizeof(speed))")!=std::string::npos);
 std::puts("PASS real aircraft speed, optional drag geometry, snapshot owner and cache invalidation");
}
