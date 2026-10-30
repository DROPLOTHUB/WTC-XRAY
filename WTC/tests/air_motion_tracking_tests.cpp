#include <cmath>
struct vec3_t {float x=0,y=0,z=0;};
#include "../src/features/esp/air_lead_tracking.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
using namespace esp::air_lead_tracking;
int main(){
    motion_state state{};vec3_t velocity{200,0,0},acceleration{0,30,0};
    auto sample=observe(state,{},1,&velocity,&acceleration);assert(sample.valid && sample.provenance==source::native && sample.acceleration.y==30);
    esp::air_lead_model::TurnState turn{};
    assert(std::fabs(esp::air_lead_model::update_turn_state(sample.velocity,sample.acceleration,sample.dt,1,&turn)-.15f)<1e-5f);
    velocity={201.5f,0,0};sample=observe(state,{10,0,0},1.05,&velocity);assert(sample.acceleration.x>0 && sample.acceleration.x<=30.01f);
    // Stops are not overridden by an old moving EMA.
    velocity={};observe(state,{20,0,0},1.1,&velocity);sample=observe(state,{20,0,0},1.15,&velocity);
    assert(sample.valid && sample.provenance==source::native && sample.velocity.x==0);
    sample=observe(state,{20,0,0},2);assert(!sample.valid && sample.reset);
    sample=observe(state,{20,0,0},2.03);assert(sample.valid && sample.velocity.x==0);
    state={};assert(!observe(state,{},0).valid);sample=observe(state,{6,0,0},.03);assert(sample.valid && std::abs(sample.velocity.x-200)<.001f);
    sample=observe(state,{6000,0,0},.06);assert(sample.reset && !sample.valid);
    sample=observe(state,{6000,0,0},.09);assert(sample.valid && sample.velocity.x==0);
    sample=observe(state,{6000,0,0},.08);assert(sample.reset && !sample.valid);
    velocity={100,0,0};acceleration={0,std::numeric_limits<float>::quiet_NaN(),0};
    sample=observe(state,{6000,0,0},.12,&velocity,&acceleration);assert(sample.valid && sample.acceleration.y==0);
    assert(std::abs(lead_velocity({},{6,0,0},.03f,{}).x-200)<.001f);
    assert(lead_velocity({},{6000,0,0},.03f,{7,0,0}).x==7);
    assert(lead_velocity({},{6,0,0},0,{7,0,0}).x==7);
    solve_context first{};auto next=first;next.speed=800;assert(!(first==next));first=next;next.weapon=3;assert(!(first==next));
    first=next;next.turn=true;assert(!(first==next));first=next;next.delay=.1f;assert(!(first==next));
    std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/esp/esp.hpp");assert(in);
    const std::string code{std::istreambuf_iterator<char>(in),{}};
    assert(code.find("vec3_t origin = frame.local_pos;")!=std::string::npos);
    assert(code.find("frame.local_pos + weapon_local")==std::string::npos);
    assert(code.find("air_intercept::solve(encounter,")!=std::string::npos);
    assert(code.find("shooter.sample.acceleration,air_velocity,air_acceleration")!=std::string::npos);
    assert(code.find("motion.dt,unit_base,&cache.turn_state")!=std::string::npos);
    assert(code.find("previous_context==context")!=std::string::npos);
    assert(code.find("if(offsets::air_acceleration_offset_available)")!=std::string::npos);
    std::puts("PASS native/derived movement, current acceleration drives turn, stop, gaps, teleport, invalid samples, ammo context and aircraft origin");
}
