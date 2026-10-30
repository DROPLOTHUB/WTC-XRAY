#include <cmath>
struct vec3_t {float x=0,y=0,z=0;};
#include "../src/features/esp/air_intercept.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
#include <chrono>
#include <string_view>
#include <vector>
using namespace esp::air_intercept;
static double verify(const encounter& e,const ammunition& ammo,const solution& s) {
    const auto own=e.shooter_known ? e.shooter_velocity : vec3_t{};
    const auto acc=e.shooter_known ? e.shooter_acceleration : vec3_t{};
    const auto origin_now=align(e.origin,own,acc,e.sample_age);
    const auto target_now=align(e.target,e.target_velocity,e.target_acceleration,e.sample_age);
    const auto launch=esp::air_lead_model::propagate_state(origin_now,own,acc,e.fire_delay,e.shooter_omega,e.turn);
    const vector direction=unit(as_double(s.point)-as_double(launch.point));
    // Independent fine midpoint integration: separate method and 0.1ms steps.
    vector p{},v=direction*ammo.speed+as_double(launch.velocity);
    const double k=drag_k(ammo,.5f*(origin_now.y+target_now.y));
    const auto acceleration=[&](vector speed){return speed*(-k*length(speed))+vector{0,-9.81,0};};
    const int steps=int(std::ceil(s.time/.0001));const double h=s.time/steps;
    for(int i=0;i<steps;++i){const auto vm=v+acceleration(v)*(h*.5);p=p+vm*h;v=v+acceleration(vm)*h;}
    const auto target=esp::air_lead_model::propagate(target_now,e.target_velocity,e.target_acceleration,s.time+e.fire_delay,e.target_omega,e.turn);
    return length(p-(as_double(target)-as_double(launch.point)));
}
int main(int argc,char** argv){
    encounter e{};e.target={0,0,1000};e.shooter_known=true;e.shooter_velocity={250,0,0};e.target_velocity=e.shooter_velocity;
    ammunition ammo{800,0,0,0};auto s=solve(e,ammo);assert(s.valid);assert(std::abs(s.point.x)<.02f);assert(verify(e,ammo,s)<.16);
    e.target_velocity={-250,0,0};s=solve(e,ammo);assert(s.valid && s.point.x<-400);assert(verify(e,ammo,s)<.16);
    e.shooter_velocity={0,0,250};e.target_velocity={0,0,250};s=solve(e,ammo);assert(s.valid);assert(std::abs(s.time-1.25f)<.002f);
    e.shooter_velocity={200,0,0};e.target_velocity=e.shooter_velocity;e.target_acceleration={30,0,0};e.shooter_acceleration=e.target_acceleration;
    s=solve(e,ammo);assert(s.valid && s.point.x>10);assert(verify(e,ammo,s)<.16); // own acceleration is NOT inherited after launch
    e.fire_delay=.3f;e.sample_age=.05f;s=solve(e,ammo);assert(s.valid && verify(e,ammo,s)<.16);
    e.turn=true;e.target_velocity={0,0,200};e.target_acceleration={30,0,0};e.target_omega=.15f;e.shooter_omega=.1f;
    ammo={850,.045f,12.7f,0};s=solve(e,ammo);assert(s.valid);assert(verify(e,ammo,s)<.2);
    double worst=0;int accepted=0,rejected=0,max_work=0;
    for(float range:{200.f,600.f,1200.f,2000.f})for(float lateral:{-200.f,0.f,200.f})for(bool turn:{false,true}) {
        encounter c{};c.origin={10000,3000,-5000};c.target={10000,3100,-5000+range};c.shooter_known=true;
        c.shooter_velocity={80,5,230};c.shooter_acceleration={3,0,0};c.target_velocity={lateral,12,120};c.target_acceleration={18,2,-8};
        c.target_omega=.12f;c.shooter_omega=.03f;c.sample_age=.04f;c.fire_delay=.08f;c.turn=turn;
        auto result=solve(c,ammo);assert(result.steps<=max_steps);max_work=(std::max)(max_work,result.steps);
        if(!result.valid){++rejected;continue;}++accepted;worst=(std::max)(worst,verify(c,ammo,result));
    }
    assert(accepted>=16 && worst<.22);
    encounter close{};close.shooter_known=true;close.shooter_velocity={0,0,500};close.target={0,0,30};close.target_velocity={0,0,-950};
    auto fast=solve(close,{800,0,0,0});assert(fast.valid && fast.time<.02f && verify(close,{800,0,0,0},fast)<.16);
    assert(!solve(encounter{},ammo).valid);
    std::mt19937 rng(516);std::uniform_real_distribution<float> random(0,1);
    int stress_valid=0,stress_failed=0;double stress_worst=0;int failure_counts[5]{};
    for(int i=0;i<200;++i) {
        encounter c{};c.origin={50000,500+6000*random(rng),-40000};c.target={c.origin.x+500*(random(rng)-.5f),c.origin.y+200*(random(rng)-.5f),c.origin.z+150+2200*random(rng)};
        c.shooter_known=true;c.shooter_velocity={400*(random(rng)-.5f),40*(random(rng)-.5f),300*(random(rng)-.5f)};
        c.target_velocity={400*(random(rng)-.5f),40*(random(rng)-.5f),300*(random(rng)-.5f)};
        c.shooter_acceleration={20*(random(rng)-.5f),5,0};c.target_acceleration={50*(random(rng)-.5f),20*(random(rng)-.5f),20*(random(rng)-.5f)};
        esp::air_lead_model::TurnState ts,ss;
        c.target_omega=esp::air_lead_model::update_turn_state(c.target_velocity,c.target_acceleration,.03f,1,&ts);
        c.shooter_omega=esp::air_lead_model::update_turn_state(c.shooter_velocity,c.shooter_acceleration,.03f,2,&ss);
        c.sample_age=.12f*random(rng);c.fire_delay=.3f*random(rng);c.turn=i%2==0;
        const ammunition b{600+500*random(rng),.03f+.12f*random(rng),i%3?12.7f:20.f,0};
        const auto result=solve(c,b);assert(result.steps<=max_steps);max_work=(std::max)(max_work,result.steps);
        if(result.valid){++stress_valid;stress_worst=(std::max)(stress_worst,verify(c,b,result));}
        else {++stress_failed;++failure_counts[static_cast<int>(result.status)];
            if(argc>1 && std::string_view(argv[1])=="--diagnose")std::printf("Rejected #%d range=%.0f speed=%.0f status=%d attempted_t=%.4f residual=%.2f\n",i,c.target.z-c.origin.z,b.speed,int(result.status),result.time,result.error);
        }
    }
    assert(stress_valid>=100 && stress_worst<.22);
    std::printf("STRESS accepted=%d rejected=%d (horizon=%d convergence=%d budget=%d) worst independent miss=%.6f m max_steps=%d\n",
        stress_valid,stress_failed,failure_counts[2],failure_counts[3],failure_counts[4],stress_worst,max_work);
    if(argc>1 && std::string_view(argv[1])=="--benchmark") {
        encounter c{};c.target={0,300,800};c.target_velocity={150,10,80};c.target_acceleration={0,20,0};c.shooter_velocity={20,0,180};c.shooter_known=true;c.turn=true;c.target_omega=.1f;
        std::vector<double> times;double checksum=0;
        for(int repeat=0;repeat<7;++repeat){const auto start=std::chrono::steady_clock::now();for(int i=0;i<1000;++i){c.target.z=800+float(i%10);const auto result=solve(c,ammo);assert(result.valid);checksum+=result.time;}
            times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/1000);}
        std::sort(times.begin(),times.end());std::printf("BENCH median7 A2A vector solve %.3f us; 1000 solves/repetition checksum=%.6f\n",times[3],checksum);
    }
    e={};e.target={0,0,10000};assert(!solve(e,ammo).valid);
    e.target={0,0,1000};e.target_velocity.x=std::numeric_limits<float>::quiet_NaN();assert(!solve(e,ammo).valid);
    ammo.speed=0;assert(!solve(encounter{},ammo).valid);
    std::printf("PASS own/target velocity, no post-launch own acceleration, firing delay, turn+drag; trajectories=%d rejected=%d worst independent miss=%.6f m max_steps=%d/%d\n",accepted,rejected,worst,max_work,max_steps);
}
