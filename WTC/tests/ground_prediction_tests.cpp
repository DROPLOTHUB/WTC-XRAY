#include <cmath>
#include <cstddef>
#include <limits>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include "../src/game/datatypes/vector3.hpp"
#include "../src/features/aimbot/ground_prediction.hpp"
using namespace ground_prediction;
static int checks = 0;
void check(bool ok, const char* name) { ++checks; if (!ok) { std::printf("FAIL: %s\n",name); std::exit(1); } }
// Independent small-step RK4 integration, not the production trace helper.
vec3_t impact(const vec3_t& origin, const Solution& s, const Ballistics& b, float k) {
    vec3_t p=origin, v=(s.point-origin).normalized()*b.speed;
    const int n=std::max(1,static_cast<int>(std::ceil(s.time/0.001f)));
    const float dt=s.time/n;
    auto a=[&](vec3_t w) { const float speed=w.length(); return w*(-k*(b.drag>0?mach_factor(speed):1.0f)*speed)+vec3_t{0,-9.81f,0}; };
    for(int i=0;i<n;++i) {
        const vec3_t a1=a(v), v2=v+a1*(dt*0.5f), a2=a(v2), v3=v+a2*(dt*0.5f), a3=a(v3), v4=v+a3*dt, a4=a(v4);
        p += (v+v2*2+v3*2+v4)*(dt/6);
        v += (a1+a2*2+a3*2+a4)*(dt/6);
    }
    return p;
}
int main() {
    Ballistics vacuum{900,10,0,0,0};
    check(screen_visible(500,350,0,0,1920,1080),"screen marker visible");
    check(screen_visible(2100,550,1920,0,1920,1080),"secondary origin respected");
    check(!screen_visible(2010,500,0,0,1920,1080),"point projected offscreen");
    check(!screen_visible(-4,500,0,0,1920,1080),"left clipped");
    check(!screen_visible(std::numeric_limits<float>::quiet_NaN(),500,0,0,1920,1080),"NaN projection rejected");
    check(!screen_visible(500,300,0,0,0,1080),"invalid monitor rejected");
    check(std::abs(turn_rate({10,0,0},{0,0,2})-0.2f)<1e-6f,"signed turn units");
    check(turn_rate({10,0,0},{0,0,-2})<0,"opposite turn");
    const auto stop=propagate({}, {2,0,0}, {-10,0,0}, 4);
    check(std::abs(stop.x-0.2f)<0.001f,"braking stops without reversing");
    const auto left=propagate({}, {10,0,0},{0,0,2},2), right=propagate({}, {10,0,0},{0,0,-2},2);
    check(std::abs(left.x-right.x)<0.001f && std::abs(left.z+right.z)<0.001f,"turn symmetry");
    check((propagate({}, {10,0,0},{},2)-vec3_t{20,0,0}).length()<0.001f,"constant velocity");
    check(propagate({}, {}, {10,0,0},2).length()==0,"rest has no invented heading");
    check(std::abs(align_position({}, {12,0,0}, {10,0,0}, 0.2f).x-2.2f)<0.001f,"current velocity aligned with older position");
    check(std::abs(align_position({}, {2,0,0}, {-10,0,0}, 0.2f).x-0.6f)<0.001f,"braking alignment");
    const vec3_t origin{0,100,0};
    float worst=0; int max_work=0;
    for(float range : {100.0f,500.0f,1000.0f,2000.0f})
    for(float height : {-50.0f,0.0f,100.0f})
    for(float lateral : {-20.0f,0.0f,20.0f})
    for(float drag : {0.0f,0.0003f}) {
        Ballistics b=vacuum; b.drag=drag;
        const vec3_t target{range,100+height,0}, velocity{2,0,lateral}, acc{-1,0,0.5f};
        const auto s=solve(origin,target,velocity,acc,b,0.1f);
        check(s.valid,"reachable moving target converges");
        check(s.steps<=max_iterations*max_steps,"bounded integration work");
        const auto hit=impact(origin,s,b,drag_coefficient(b,std::max(origin.y,target.y)));
        const float error=(hit-propagate(target,velocity,acc,s.time+0.1f)).length();
        worst=std::max(worst,error); max_work=std::max(max_work,s.steps);
        check(error<0.5f,"independent RK4 miss below 0.5 m");
    }
    // Wider physical envelope: non-convergent/out-of-horizon cases must fail
    // within the same budget; every accepted result is independently checked.
    int stress_valid=0, stress_rejected=0;
    for(float speed : {300.0f,600.0f,1500.0f})
    for(float range : {100.0f,500.0f,1000.0f,2000.0f})
    for(float area : {0.0003f,0.002f,0.005f}) {
        Ballistics b{speed,10,0,0,area};
        const vec3_t target{range,100,0}, velocity{0,0,15};
        auto s=solve(origin,target,velocity,{},b);
        check(s.steps<=max_iterations*max_steps,"stress bounded work");
        if (!s.valid) { ++stress_rejected; continue; }
        ++stress_valid;
        const auto hit=impact(origin,s,b,drag_coefficient(b,100));
        const float error=(hit-propagate(target,velocity,{},s.time)).length();
        worst=std::max(worst,error); max_work=std::max(max_work,s.steps);
        if (error>=1.0f) std::printf("stress speed=%.0f range=%.0f area=%.4f error=%.4f\n",speed,range,area,error);
        check(error<1.0f,"stress independent miss below 1 m");
    }
    std::printf("Stress accepted=%d rejected=%d\n",stress_valid,stress_rejected);
    const auto plain=solve(origin,{1000,100,0},{0,0,15},{},vacuum);
    const auto aged=solve(origin,{1000,100,0},{0,0,15},{},vacuum,0.1f);
    check(plain.valid && aged.valid && aged.point.z>plain.point.z+1.4f,"sample age compensation");
    Ballistics drag=vacuum; drag.drag=0.0003f;
    const auto slower=solve(origin,{1000,100,0},{},{},drag);
    const auto baseline=solve(origin,{1000,100,0},{},{},vacuum);
    check(slower.valid && slower.time>baseline.time,"drag increases flight time");
    const vec3_t shift{30000,2000,-30000};
    const auto translated=solve(origin+shift,vec3_t{1000,100,0}+shift,{},{},vacuum);
    check(translated.valid && (translated.point-shift-baseline.point).length()<0.02f,"large coordinate translation");
    check(std::abs(translated.time-baseline.time)<0.0001f,"translation preserves flight time");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    check(!solve(origin,{nan,0,0},{},{},vacuum).valid,"NaN rejected");
    check(!solve(origin,{50000,0,0},{},{},vacuum).valid,"out of horizon rejected");
    check(!solve(origin,origin,{},{},vacuum).valid,"zero range rejected");
    Ballistics invalid=vacuum; invalid.speed=0;
    check(!solve(origin,{1000,0,0},{},{},invalid).valid,"zero muzzle speed rejected");
    vec3_t p{},v{900,0,0}; step(p,v,0.1f,false,0.5f);
    check(v.x>0 && v.x<900,"drag never accelerates or reverses horizontal velocity");
    const auto start=std::chrono::steady_clock::now();
    double sink=0;
    for(int i=0;i<10000;++i) { auto s=solve(origin,{1000.0f+(i%100),100,0},{0,0,15},{-1,0,1},drag); sink+=s.time; }
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
    std::printf("PASS %d checks; worst RK4 miss %.4f m; max observed steps %d (cap %d); mean solve %.2f us; checksum %.2f\n",checks,worst,max_work,max_iterations*max_steps,us,sink);
}
