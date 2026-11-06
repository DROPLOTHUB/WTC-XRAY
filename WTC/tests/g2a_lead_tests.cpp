#include <cmath>
struct vec3_t {float x=0,y=0,z=0;};
#include "../src/features/esp/g2a_lead.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
using namespace esp;
// Independent midpoint trajectory integration verifies velocity inheritance and
// vertical airborne motion, rather than comparing a solver to itself.
static double miss(vec3_t origin,vec3_t target,vec3_t own,vec3_t enemy,float age,
    const ballistic_validation::round_data& round,const air_intercept::solution& s,vec3_t accel={},float omega=0) {
    using namespace air_intercept;
    ammunition ammo{};assert(g2a_lead::ammunition(round,ammo));
    const auto launch=as_double(origin)+as_double(own)*age;
    const auto target_now=as_double(target)+as_double(enemy)*age-as_double(accel)*(.5*age*age);
    const double k=drag_k(ammo,float((launch.y+target_now.y)*.5));
    vector p{},v=unit(as_double(s.point)-launch)*round.speed+as_double(own);
    const auto acc=[&](vector value){return value*(-k*length(value))+vector{0,-9.81,0};};
    const int n=int(std::ceil(s.time/.0001));const double dt=s.time/n;
    for(int i=0;i<n;++i){const auto mid=v+acc(v)*(dt*.5);p=p+mid*dt;v=v+acc(mid)*dt;}
    // Independent midpoint integration of target acceleration/rotation; no propagate/solve call.
    vector tp=target_now,tv=as_double(enemy),axis=unit(cross(tv,as_double(accel)));
    const double along=dot(unit(tv),as_double(accel));
    const auto target_acc=[&](vector value){return omega>0 ?
        cross(axis,value)*omega+unit(value)*along : as_double(accel);};
    double elapsed=0;
    while(elapsed<s.time) {
        const bool active=elapsed<.85;
        const double h=(std::min)(.0001,(std::min)(double(s.time)-elapsed,active ? .85-elapsed : double(s.time)-elapsed));
        const auto a=active ? target_acc(tv) : vector{};
        const auto mid=tv+a*(h*.5);tp=tp+mid*h;
        tv=tv+(active ? target_acc(mid) : vector{})*h;elapsed+=h;
    }
    return length(launch+p-tp);
}
int main() {
    const ballistic_validation::round_data round{900,.12f,.02f,.1f,0};
    air_intercept::ammunition ammo{};
    assert(g2a_lead::ammunition(round,ammo));
    assert(std::abs(ammo.caliber_mm-20)<.001f);
    assert(std::abs(ammo.drag_area-3.14159265f*.01f*.01f*.1f)<1e-10f);
    const vec3_t origin{0,10,0},target{0,210,800};
    int count=0;
    for(float own_x:{-25.f,0.f,25.f})for(float enemy_x:{-180.f,0.f,180.f})for(float climb:{-30.f,0.f,30.f}) {
        const vec3_t own{own_x,0,0},enemy{enemy_x,climb,20};
        const auto s=g2a_lead::solve(origin,target,own,true,enemy,true,.06f,round);
        assert(s.valid && s.time>0 && s.time<=4 && s.steps<=air_intercept::max_steps);
        assert(miss(origin,target,own,enemy,.06f,round,s)<.2);++count;
    }
    const auto stationary=g2a_lead::solve(origin,target,{},true,{},true,0,round);
    const auto moving=g2a_lead::solve(origin,target,{25,0,0},true,{},true,0,round);
    assert(stationary.valid && moving.valid && moving.point.x<stationary.point.x-10);
    assert(!g2a_lead::solve(origin,target,{},false,{},true,0,round).valid);
    assert(!g2a_lead::solve(origin,target,{},true,{},false,0,round).valid);
    assert(!g2a_lead::solve(origin,target,{},true,{},true,.201f,round).valid);
    assert(!g2a_lead::solve(origin,target,{},true,{},true,-.01f,round).valid);
    assert(!g2a_lead::solve(origin,target,{151,0,0},true,{},true,0,round).valid);
    assert(!g2a_lead::solve(origin,target,{},true,{951,0,0},true,0,round).valid);
    for(int field=0;field<4;++field) {
        auto invalid=round;
        if(field==0)invalid.speed=0;if(field==1)invalid.mass=0;
        if(field==2)invalid.caliber_m=20;if(field==3)invalid.length_m=0;
        assert(!g2a_lead::solve(origin,target,{},true,{},true,0,invalid).valid);
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    assert(!g2a_lead::solve(origin,target,{nan,0,0},true,{},true,0,round).valid);
    assert(!g2a_lead::solve(origin,{0,100,50000},{},true,{},true,0,round).valid);
    for(vec3_t acceleration : {vec3_t{0,35,0},vec3_t{0,-35,0},vec3_t{0,0,45},vec3_t{12,30,-20},vec3_t{20,0,0}}) {
        g2a_lead::motion_track ot{},et{};
        vec3_t own_v{15,0,0},enemy_v{160,0,0};
        const auto own=g2a_lead::observe(ot,origin,1,&own_v,nullptr,150);
        const auto enemy=g2a_lead::observe(et,target,1,&enemy_v,&acceleration,950);
        for(float age : {0.f,.06f,.19f}) {
            const auto result=g2a_lead::solve_motion(origin,target,own,enemy,age,round);
            assert(result.valid && result.steps<=air_intercept::max_steps);
            assert(miss(origin,target,own_v,enemy_v,age,round,result,acceleration,enemy.omega)<.2);
            ++count;
        }
        auto invalid=enemy;invalid.motion.acceleration={nan,0,0};
        assert(!g2a_lead::solve_motion(origin,target,own,invalid,0,round).valid);
        invalid=enemy;invalid.omega=3;
        assert(!g2a_lead::solve_motion(origin,target,own,invalid,0,round).valid);
        assert(!g2a_lead::solve_motion(origin,target,own,enemy,.201f,round).valid);
    }
    g2a_lead::motion_track history{};
    assert(!g2a_lead::observe(history,{},1,nullptr,nullptr,150).motion.valid);
    auto estimated=g2a_lead::observe(history,{1,0,0},1.05,nullptr,nullptr,150);
    assert(estimated.motion.valid && estimated.motion.provenance==air_lead_tracking::source::positions);
    assert(std::abs(estimated.motion.velocity.x-20)<.01);
    assert(!g2a_lead::observe(history,{1,0,0},1.05,nullptr,nullptr,150).motion.valid); // duplicate
    vec3_t v{100,0,0},a{0,30,0};
    assert(g2a_lead::observe(history,{},2,&v,&a,950).motion.valid);
    assert(!g2a_lead::observe(history,{10000,0,0},2.05,&v,&a,950).motion.valid); // teleport
    assert(!history.turn.valid && !history.history.have_time);
    assert(g2a_lead::observe(history,{},3,&v,&a,950).motion.valid);
    assert(!g2a_lead::observe(history,{},3.3,&v,&a,950).motion.valid); // gap
    assert(g2a_lead::observe(history,{},4,&v,&a,950).motion.valid);
    assert(!g2a_lead::observe(history,{},3.9,&v,&a,950).motion.valid); // reversal
    v={151,0,0};assert(!g2a_lead::observe(history,{},5,&v,nullptr,150).motion.valid);
    v={100,0,0};a={0,161,0};assert(!g2a_lead::observe(history,{},5,&v,&a,950).motion.valid);
    a={};v={100,0,0};history={};
    assert(g2a_lead::observe(history,{},6,&v,nullptr,950).motion.valid);
    v.x=101;
    auto derivative=g2a_lead::observe(history,{5,0,0},6.05,&v,nullptr,950);
    assert(derivative.motion.valid && derivative.motion.acceleration.x>0);
    v.x=140;
    assert(!g2a_lead::observe(history,{10,0,0},6.10,&v,nullptr,950).motion.valid); // impossible velocity jump
    // Failed native reads must use CURRENT positional evidence, never cached native motion.
    history={};v={20,0,0};
    assert(g2a_lead::observe(history,{},7,&v,nullptr,150).motion.valid);
    auto fallback=g2a_lead::observe(history,{1,0,0},7.05,nullptr,nullptr,150);
    assert(fallback.motion.valid && fallback.motion.provenance==air_lead_tracking::source::positions);
    assert(fallback.motion.reset);
    // Render at 1 kHz, snapshots at 125 Hz or 10 Hz: never observe a duplicate
    // or undersized interval, and retain a valid estimate between observations.
    for (int period : {8,100}) {
        using clock = std::chrono::steady_clock;
        clock::time_point sample{},next{};
        g2a_lead::motion_track track{};
        g2a_lead::motion_sample held{};
        const vec3_t velocity{20,0,0};
        int observations=0;
        for(int ms=1000;ms<2000;++ms) {
            const auto now=clock::time_point{}+std::chrono::milliseconds(ms);
            const auto captured=clock::time_point{}+std::chrono::milliseconds(ms/period*period);
            const double interval=std::chrono::duration<double>(captured-sample).count();
            if(now>=next && interval>=.025) {
                sample=captured;next=now+std::chrono::milliseconds(30);
                const double stamp=std::chrono::duration<double>(captured.time_since_epoch()).count();
                held=g2a_lead::observe(track,{float(stamp*20),0,0},stamp,&velocity,nullptr,150);
                ++observations;
            }
            assert(held.motion.valid);
            assert(std::chrono::duration<double>(now-sample).count()<=.2);
        }
        assert(observations>=10 && observations<=34);
        const auto expired=sample+std::chrono::milliseconds(201);
        assert(std::chrono::duration<float>(expired-sample).count()>.2f);
    }
    // Bind the cadence regression to production's actual gates and timestamp ownership.
    std::ifstream input(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/esp/esp.hpp");
    assert(input);
    const std::string src{std::istreambuf_iterator<char>(input),{}};
    assert(src.find("now >= g2a_cache.next && interval >= .025 && !invalidate_solution")!=std::string::npos);
    assert(src.find("held_age > .2f")!=std::string::npos);
    assert(src.find("curr_snapshot.captured_at < g2a_cache.latest")!=std::string::npos);
    assert(src.find("std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g2a_cache.sample)")!=std::string::npos);
    g2a_lead::reset_diagnostics();assert(g2a_lead::sample_age_ms<0 && g2a_lead::flight_time==0);
    std::printf("G2A: %d independent trajectory cases and fail-closed checks passed\n",count);
}
