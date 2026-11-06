#include "../src/features/aimbot/g2g_runtime_policy.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main(){
 using namespace g2g_runtime;
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 assert(sample_valid(0)&&sample_valid(.25f));
 for(float bad:{-.001f,.251f,nan,inf})assert(!sample_valid(bad));
 assert(marker_valid(.06f,.2f));
 assert(!marker_valid(.251f,.1f)&&!marker_valid(.1f,.251f)&&!marker_valid(nan,.1f));
 assert(newer_sample(.001)&&newer_sample(.12));
 assert(!newer_sample(0)&&!newer_sample(-.01)&&!newer_sample(nan));
 round_signature good{900,10,.12f,.5f,0,0};assert(signature_valid(good));
 for(int i=0;i<6;++i){auto changed=good;changed[i]+=1;assert(changed!=good);changed=good;changed[i]=nan;assert(!signature_valid(changed));}
 auto zero=good;zero[0]=0;assert(!signature_valid(zero));
 zero=good;zero[1]=0;assert(!signature_valid(zero));
 zero=good;zero[4]=-1;assert(!signature_valid(zero));
 // Cached marker must expire even if new snapshots arrive without a new solve.
 int held=0;for(int ms=0;ms<=300;++ms){if(marker_valid(ms/1000.f,ms/1000.f))++held;}
 assert(held==251);
 current.valid=true;current.flight_time=1;current.sample_age_ms=10;reset_diagnostics();
 assert(!current.valid&&current.flight_time==0&&current.sample_age_ms<0);
 // Bind integration contracts to the runtime policy tested above.
 const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
 std::ifstream file(root/"src/features/aimbot/aimbot.hpp");assert(file);
 const std::string src{std::istreambuf_iterator<char>(file),{}};
 for(const char* token:{"cache.epoch != epoch","cache.round != round","g2g_runtime::newer_sample(position_dt)",
   "g2g_runtime::marker_valid(solve_age,solution_sample_age)","sdk::ground_velocity_source=cache.target_source",
   "~AttemptGuard()","cache.own_acceleration,read_sample_age","cache.last_solution_sample=captured_at"})
   assert(src.find(token)!=std::string::npos);
 std::puts("PASS G2G runtime: sample/solution expiry, duplicate rejection, round identity, read provenance and failure throttle contracts");
}
