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
// Independent double-precision small-step RK4; no production step/trace/Mach helper.
struct DVec {
    double x=0,y=0,z=0;
    DVec operator+(DVec b) const { return {x+b.x,y+b.y,z+b.z}; }
    DVec operator*(double s) const { return {x*s,y*s,z*s}; }
};
vec3_t impact(const vec3_t& origin, const Solution& s, const Ballistics& b, float k, const vec3_t& own_velocity = {}) {
    const vec3_t launch=(s.point-origin).normalized()*b.speed+own_velocity;
    DVec p{},v{launch.x,launch.y,launch.z};
    const int n=std::max(1,static_cast<int>(std::ceil(s.time/0.0005)));
    const double dt=double(s.time)/n;
    auto a=[&](DVec w) {
        const double speed=std::sqrt(w.x*w.x+w.y*w.y+w.z*w.z), m=speed/343.0;
        const double factor=b.drag>0 ? (m<0.8?1:(m<1.2?1+4.5*(m-0.8):2.8/std::sqrt(m))) : 1;
        return w*(-k*factor*speed)+DVec{0,-9.81,0};
    };
    for(int i=0;i<n;++i) {
        const auto a1=a(v),v2=v+a1*(dt/2),a2=a(v2),v3=v+a2*(dt/2),a3=a(v3),v4=v+a3*dt,a4=a(v4);
        p=p+(v+v2*2+v3*2+v4)*(dt/6);
        v=v+(a1+a2*2+a3*2+a4)*(dt/6);
    }
    return origin+vec3_t{float(p.x),float(p.y),float(p.z)};
}
// Independently integrate the bounded horizontal heading/speed model with RK4.
vec3_t target_rk4(vec3_t p, vec3_t v, vec3_t a, float time) {
    const double speed=std::hypot(double(v.x),double(v.z));
    const double t=std::clamp(double(time),0.0,8.25);
    if(speed<0.1) return p+v*float(t);
    const double along=std::clamp((double(v.x)*a.x+double(v.z)*a.z)/speed,-12.0,8.0);
    const double omega=speed<1?0:std::clamp((double(v.x)*a.z-double(v.z)*a.x)/(speed*speed),-0.8,0.8);
    double active=std::min(t,0.6);
    if(along< -0.01) active=std::min(active,speed/-along);
    auto velocity=[&](double u) {
        const double phase=omega*std::min(u,active);
        const double s=std::max(0.0,speed+along*std::min(u,active))/speed;
        return DVec{(v.x*std::cos(phase)-v.z*std::sin(phase))*s,v.y,
            (v.z*std::cos(phase)+v.x*std::sin(phase))*s};
    };
    DVec out{};
    // Split at the acceleration/stop boundary so RK4 never spans the kink.
    for(int segment=0;segment<2;++segment) {
        const double begin=segment?active:0,end=segment?t:active;
        const int n=std::max(1,int(std::ceil((end-begin)/0.0005)));
        const double h=(end-begin)/n;
        for(int i=0;i<n;++i) {
            const double u=begin+i*h;
            out=out+(velocity(u)+velocity(u+h/2)*4+velocity(u+h))*(h/6);
        }
    }
    return p+vec3_t{float(out.x),float(out.y),float(out.z)};
}
vec3_t rotate(vec3_t v) { return {-v.z,v.y,v.x}; }
vec3_t mirror(vec3_t v) { return {v.x,v.y,-v.z}; }
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
    // Independently integrate world launch velocity, including own lateral motion and drag.
    float own_worst=0;
    int own_cases=0;
    for(float area : {0.f,0.0003f})
    for(vec3_t own : {vec3_t{25,0,0},vec3_t{-25,0,0},vec3_t{0,0,25},vec3_t{0,0,-25},vec3_t{12,2,-8}})
    for(vec3_t enemy : {vec3_t{},vec3_t{0,0,25},vec3_t{0,0,-25}}) {
        Ballistics b=vacuum; b.drag=area;
        const vec3_t target{1000,130,0};
        const auto s=solve(origin,target,enemy,{},b,0,own);
        check(s.valid,"moving shooter converges");
        check(s.steps<=max_iterations*max_steps,"moving shooter bounded work");
        const auto hit=impact(origin,s,b,drag_coefficient(b,130),own);
        const float err=(hit-(target+enemy*s.time)).length();
        own_worst=std::max(own_worst,err);++own_cases;
        check(err<0.5f,"moving shooter independent RK4 miss below 0.5 m");
    }
    const auto compensated=solve(origin,{1000,100,0},{},{},vacuum,0,{0,0,25});
    check(compensated.valid && compensated.point.z< -20,"own lateral motion aims against inherited drift");
    const auto together=solve(origin,{1000,100,0},{0,0,25},{},vacuum,0,{0,0,25});
    check(together.valid && std::abs(together.point.z)<0.2f,"equal world motion cancels lateral lead in vacuum");
    const auto explicit_stopped=solve(origin,{1000,100,0},{},{},vacuum,0,{});
    check(explicit_stopped.valid && (explicit_stopped.point-baseline.point).length()<1e-6f,"stopped shooter preserves previous solution");
    const auto shifted_own=solve(origin+shift,vec3_t{1000,100,0}+shift,{},{},vacuum,0,{0,0,25});
    check(shifted_own.valid && (shifted_own.point-shift-compensated.point).length()<0.03f,"moving shooter translation invariance");
    check(!solve(origin,{1000,100,0},{},{},vacuum,0,{151,0,0}).valid,"unphysical own velocity rejected");
    std::printf("G2G own motion: %d cases; worst RK4 miss %.4f m\n",own_cases,own_worst);
    const float nan=std::numeric_limits<float>::quiet_NaN();
    check(!solve(origin,{1000,100,0},{},{},vacuum,0,{nan,0,0}).valid,"NaN own velocity rejected");
    check(!solve(origin,{nan,0,0},{},{},vacuum).valid,"NaN rejected");
    check(!solve(origin,{50000,0,0},{},{},vacuum).valid,"out of horizon rejected");
    check(!solve(origin,origin,{},{},vacuum).valid,"zero range rejected");
    Ballistics invalid=vacuum; invalid.speed=0;
    check(!solve(origin,{1000,0,0},{},{},invalid).valid,"zero muzzle speed rejected");
    vec3_t p{},v{900,0,0}; step(p,v,0.1f,false,0.5f);
    check(v.x>0 && v.x<900,"drag never accelerates or reverses horizontal velocity");
    // Analytic propagation must agree across straight, small-turn, capped-turn,
    // acceleration and stop boundaries, including both turn signs.
    float propagation_worst=0;
    for(vec3_t vel : {vec3_t{2,0.4f,0},vec3_t{25,-0.2f,6},vec3_t{0.05f,0,0}})
    for(vec3_t acc : {vec3_t{},vec3_t{-10,0,3},vec3_t{8,0,-30},vec3_t{0,0,0.00001f}})
    for(float t : {0.f,0.1f,0.2f,0.6f,2.f,8.f}) {
        const float err=(propagate({},vel,acc,t)-target_rk4({},vel,acc,t)).length();
        propagation_worst=std::max(propagation_worst,err);
        check(err<0.0001f,"analytic target propagation matches independent RK4");
    }
    float steep_worst=0;
    int steep_cases=0;
    for(vec3_t offset : {vec3_t{0,1000,0},vec3_t{1,1500,0},vec3_t{30,900,20},
        vec3_t{30,-900,20},vec3_t{1500,700,900},vec3_t{1500,-700,-900}})
    for(vec3_t own : {vec3_t{},vec3_t{90,15,-60}})
    for(float area : {0.f,0.0003f}) {
        const Ballistics b{900,10,0,0,area};
        const vec3_t target=origin+offset,enemy{5,0,-8},acc{-3,0,2};
        const auto s=solve(origin,target,enemy,acc,b,0.1f,own);
        check(s.valid,"steep/elevation moving shooter converges");
        check(s.time>0 && s.time<=8 && s.error<=0.15f && s.steps<=5120,"steep acceptance and work bounds");
        const float err=(impact(origin,s,b,drag_coefficient(b,std::max(origin.y,target.y)),own)
            -target_rk4(target,enemy,acc,s.time+0.1f)).length();
        steep_worst=std::max(steep_worst,err); ++steep_cases;
        check(err<0.25f,"steep independent RK4 miss below 0.25 m");
        for(bool reflection : {false,true}) {
            auto transform=[&](vec3_t v) { return reflection?mirror(v):rotate(v); };
            const auto other=solve(transform(origin),transform(target),transform(enemy),transform(acc),b,0.1f,transform(own));
            check(other.valid && std::abs(other.time-s.time)<0.00002f &&
                (other.point-transform(s.point)).length()<0.005f,"solve mirror/rotation invariance");
        }
    }
    // Forward inheritance admits a real sub-8 s intercept even when range/muzzle > 8 s.
    const Ballistics slow{100,10,0,0,0};
    const auto inherited=solve({}, {1200,0,0},{},{},slow,0,{100,0,0});
    check(inherited.valid && inherited.time<8,"inherited speed avoids false horizon rejection");
    check((impact({},inherited,slow,0,{100,0,0})-vec3_t{1200,0,0}).length()<0.2f,"horizon seed independent RK4");
    const auto impossible=solve({}, {1000,0,0},{},{},slow,0,{-100,0,0});
    check(!impossible.valid && impossible.steps<=5120,"nonconvergent inherited launch fails boundedly");
    const auto fleeing=solve({}, {500,0,0},{200,0,0},{},slow);
    check(!fleeing.valid && fleeing.steps<=5120,"faster fleeing target fails boundedly");
    check(!solve(origin,{1000,100,0},{nan,0,0},{},vacuum).valid,"NaN target velocity rejected");
    check(!solve(origin,{1000,100,0},{},{0,nan,0},vacuum).valid,"NaN acceleration rejected");
    check(!solve(origin,{1000,100,0},{},{},vacuum,nan).valid,"NaN age rejected");
    check(!solve({nan,0,0},{1000,100,0},{},{},vacuum).valid,"NaN origin rejected");
    vec3_t trace_hit{}; float trace_time=0; int trace_steps=0;
    check(!trace(origin,{1000,100,0},vacuum,0,nan,trace_hit,trace_time,trace_steps),"trace rejects NaN range");
    check(!trace(origin,{1000,100,0},vacuum,nan,1000,trace_hit,trace_time,trace_steps),"trace rejects NaN drag coefficient");
    check(!trace(origin,{1000,100,0},vacuum,0,1000,trace_hit,trace_time,trace_steps,{}, {nan,0,0}),"trace rejects NaN plane normal");
    check(trace_steps==0,"invalid traces perform no integration");
    // Crossing must be a fixed plane, not a horizontal sphere reached early
    // through inherited lateral drift. The vertical case has no horizontal radius.
    trace_steps=0;
    check(trace({}, {1000,0,0},vacuum,0,1000,trace_hit,trace_time,trace_steps,{0,0,90},{1,0,0}),"lateral plane crossing succeeds");
    check(std::abs(trace_hit.x-1000)<0.001f && std::abs(trace_time-1000.f/900)<0.00001f,"lateral drift does not advance LOS crossing");
    trace_steps=0;
    check(trace({}, {0,1000,0},vacuum,0,1000,trace_hit,trace_time,trace_steps,{}, {0,1,0}),"vertical plane crossing succeeds");
    check(std::abs(trace_hit.y-1000)<0.001f,"vertical LOS crossing hits fixed elevation");
    const Ballistics boundary{1000,10,0,0,0};
    trace_steps=0;
    check(trace({}, {8000,0,0},boundary,0,8000,trace_hit,trace_time,trace_steps,{}, {1,0,0})
        && trace_time==8 && trace_steps==512,"trace includes exact 8 second boundary");
    trace_steps=0;
    check(!trace({}, {8000,0,0},boundary,0,8001,trace_hit,trace_time,trace_steps,{}, {1,0,0})
        && trace_steps==512,"trace rejects crossing beyond 8 seconds");
    for(vec3_t acc : {vec3_t{-10,0,0},vec3_t{-10,0,6}}) {
        const vec3_t target{1000,150,0},vel{2,0,0},own{30,0,-20};
        const auto s=solve(origin,target,vel,acc,drag,0.2f,own);
        check(s.valid,"braking-to-stop intercept converges");
        check((impact(origin,s,drag,drag_coefficient(drag,150),own)
            -target_rk4(target,vel,acc,s.time+0.2f)).length()<0.2f,"stopping target independent RK4 intercept");
    }
    for(auto field : {&Ballistics::speed,&Ballistics::mass,&Ballistics::caliber,&Ballistics::length,&Ballistics::drag})
    for(float bad : {nan,std::numeric_limits<float>::infinity()}) {
        Ballistics b=vacuum; b.*field=bad;
        check(!solve(origin,{1000,100,0},{},{},b).valid,"nonfinite ballistics rejected");
    }
    check(!solve(origin,{1000,100,0},{std::numeric_limits<float>::max(),0,0},{},vacuum).valid,"overflowing future position rejected");
    const Ballistics fallback{900,10,0.1f,0.5f,0};
    check(drag_coefficient(fallback,100)>0,"finite geometry fallback retained");
    check(drag_coefficient(vacuum,100)==0,"finite missing area retains vacuum fallback");
    check(std::abs(mach_factor(343)-1.9f)<0.00001f,"existing transonic Mach semantics retained");
    std::printf("Analytic propagation worst %.7f m; steep %d cases worst RK4 %.4f m\n",propagation_worst,steep_cases,steep_worst);
    const auto start=std::chrono::steady_clock::now();
    double sink=0;
    for(int i=0;i<10000;++i) { auto s=solve(origin,{1000.0f+(i%100),100,0},{0,0,15},{-1,0,1},drag); sink+=s.time; }
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
    std::printf("PASS %d checks; worst RK4 miss %.4f m; max observed steps %d (cap %d); mean solve %.2f us; checksum %.2f\n",checks,worst,max_work,max_iterations*max_steps,us,sink);
}
