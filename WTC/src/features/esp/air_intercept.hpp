#pragma once
#include "air_lead_prediction.hpp"
namespace esp::air_intercept {
struct vector {double x=0,y=0,z=0;vector operator+(vector b) const{return {x+b.x,y+b.y,z+b.z};} vector operator-(vector b) const{return {x-b.x,y-b.y,z-b.z};} vector operator*(double s) const{return {x*s,y*s,z*s};}};
inline vector as_double(const vec3_t& v){return {v.x,v.y,v.z};}
inline double dot(vector a,vector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline vector cross(vector a,vector b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double length(vector v){return std::sqrt(dot(v,v));}
inline vector unit(vector v){const double n=length(v);return n>1e-9 ? v*(1/n) : vector{};}
inline bool finite(vector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct ammunition {float speed=0,mass=0,caliber_mm=0,drag_area=0;};
struct encounter {
    vec3_t origin{},target{},shooter_velocity{},shooter_acceleration{},target_velocity{},target_acceleration{};
    float shooter_omega=0,target_omega=0,sample_age=0,fire_delay=0;
    bool turn=false,shooter_known=false;
};
enum class failure {none,inputs,horizon,convergence,budget};
inline const char* reason(failure f) {
    switch(f){case failure::inputs:return "Datos A2A no validos";case failure::horizon:return "Fuera del horizonte A2A (4 s)";
    case failure::budget:return "Limite de calculo A2A";case failure::convergence:return "Sin convergencia balistica A2A";default:return "Solucion A2A validada por trayectoria";}
}
struct solution {vec3_t point{};float time=0,error=0;int steps=0;bool valid=false;failure status=failure::inputs;};
inline constexpr int max_steps=8192,max_iterations=7;
inline constexpr double tolerance=.15;
inline vec3_t align(const vec3_t& p,const vec3_t& current_v,const vec3_t& a,float age) {
    return {p.x+current_v.x*age-.5f*a.x*age*age,p.y+current_v.y*age-.5f*a.y*age*age,p.z+current_v.z*age-.5f*a.z*age*age};
}
inline double drag_k(const ammunition& ammo,float altitude) {
    double area=ammo.drag_area;
    if(!std::isfinite(area)||area<=0) {const double radius=ammo.caliber_mm*.0005;area=radius>0 && std::isfinite(radius) ? .32*3.141592653589793*radius*radius : 0;}
    if(!std::isfinite(ammo.mass)||ammo.mass<=.00001||area<=0)return 0;
    return (std::clamp)(.5*1.225*std::exp(-(std::max)(0.f,altitude)/9000.)*area/ammo.mass,0.,.02);
}
struct flight {vector point{},velocity{};bool valid=false;};
// RK4 in relative coordinates. Drag uses total projectile velocity, not muzzle speed.
inline flight trace(vector initial,double duration,double k,int& work,double step_limit=1./64.) {
    flight out{};out.velocity=initial;
    if(!finite(initial)||!std::isfinite(duration)||duration<=0||duration>4.00001||!std::isfinite(k)||k<0||step_limit<=0)return out;
    if(k<1e-9) {out.point=initial*duration+vector{0,-4.905*duration*duration,0};out.velocity.y-=9.81*duration;out.valid=true;return out;}
    const auto acceleration=[&](vector v){return v*(-k*length(v))+vector{0,-9.81,0};};
    double elapsed=0;
    while(elapsed<duration) {
        if(work>=max_steps)return out;
        const double h=(std::min)((std::min)(step_limit,duration-elapsed),.15/(k*(std::max)(1.,length(out.velocity))));
        if(h<1e-10)return out;
        const auto v1=out.velocity,a1=acceleration(v1);
        const auto v2=v1+a1*(h*.5),a2=acceleration(v2);
        const auto v3=v1+a2*(h*.5),a3=acceleration(v3);
        const auto v4=v1+a3*h,a4=acceleration(v4);
        out.point=out.point+(v1+v2*2+v3*2+v4)*(h/6);
        out.velocity=out.velocity+(a1+a2*2+a3*2+a4)*(h/6);
        ++work;elapsed+=h;
        if(!finite(out.point)||!finite(out.velocity))return out;
    }
    out.valid=true;return out;
}
inline solution solve(const encounter& e,const ammunition& ammo) {
    solution out{};
    using air_lead_model::finite;
    if(!finite(e.origin)||!finite(e.target)||!finite(e.target_velocity)||!finite(e.target_acceleration)||
        !finite(e.shooter_velocity)||!finite(e.shooter_acceleration)||!std::isfinite(e.target_omega)||!std::isfinite(e.shooter_omega)||
        !std::isfinite(e.sample_age)||!std::isfinite(e.fire_delay)||!std::isfinite(ammo.speed)||ammo.speed<50||ammo.speed>4000)return out;
    const float age=(std::clamp)(e.sample_age,0.f,.2f),delay=(std::clamp)(e.fire_delay,0.f,.5f);
    const auto own_v=e.shooter_known ? e.shooter_velocity : vec3_t{};
    const auto own_a=e.shooter_known ? e.shooter_acceleration : vec3_t{};
    const auto origin_now=align(e.origin,own_v,own_a,age),target_now=align(e.target,e.target_velocity,e.target_acceleration,age);
    const auto launch=air_lead_model::propagate_state(origin_now,own_v,own_a,delay,e.shooter_omega,e.turn);
    const vector origin=as_double(launch.point),inherited=as_double(launch.velocity);
    const auto target_at=[&](double t){return air_lead_model::propagate_state(target_now,e.target_velocity,e.target_acceleration,float(t)+delay,e.target_omega,e.turn);};
    const auto seed_delta=[&](double t){return as_double(target_at(t).point)-origin-inherited*t+vector{0,4.905*t*t,0};};
    if(length(as_double(target_at(0).point)-origin)<.01)return out;
    const auto seed_slope=[&](double t) {
        const auto d=seed_delta(t);const double n=length(d);
        const auto speed=as_double(target_at(t).velocity)-inherited+vector{0,9.81*t,0};
        return n>1e-9 ? dot(d,speed)/n-ammo.speed : -double(ammo.speed);
    };
    double lo=0,hi=0,previous_slope=seed_slope(0);
    for(int step=1;step<=32;++step) {
        const double t=step*.125,previous=t-.125;
        if(length(seed_delta(t))<=ammo.speed*t){lo=previous;hi=t;break;}
        const double slope=seed_slope(t);
        // A close, fast pass can have TWO roots between coarse samples, with
        // positive residual at both ends. Inspect the intervening minimum.
        if(previous_slope<0 && slope>0) {
            double left=previous,right=t;
            for(int i=0;i<22;++i){const double mid=(left+right)*.5;if(seed_slope(mid)<0)left=mid;else right=mid;}
            const double minimum=(left+right)*.5;
            if(length(seed_delta(minimum))<=ammo.speed*minimum){lo=previous;hi=minimum;break;}
        }
        previous_slope=slope;
    }
    if(hi==0){out.status=failure::horizon;return out;}
    for(int i=0;i<17;++i){const double mid=(lo+hi)*.5;if(length(seed_delta(mid))>ammo.speed*mid)lo=mid;else hi=mid;}
    double time=(lo+hi)*.5;vector direction=unit(seed_delta(time));
    const double k=drag_k(ammo,.5f*(origin_now.y+target_now.y));
    out.status=failure::convergence;
    for(int iteration=0;iteration<max_iterations;++iteration) {
        const auto target=target_at(time);
        const auto current=trace(direction*ammo.speed+inherited,time,k,out.steps);
        if(!current.valid){out.status=failure::budget;return out;}
        const vector residual=current.point-(as_double(target.point)-origin);
        const double error=length(residual);out.error=float(error);out.time=float(time);
        if(error<=tolerance) {
            const double distance=(std::max)(1.,length(as_double(target.point)-origin));
            const auto point=origin+direction*distance;
            out.point={float(point.x),float(point.y),float(point.z)};out.time=float(time);out.valid=true;out.status=failure::none;return out;
        }
        const vector axis=std::abs(direction.y)<.9 ? vector{0,1,0} : vector{1,0,0};
        const vector tangent1=unit(cross(direction,axis)),tangent2=unit(cross(tangent1,direction));
        constexpr double eps=1e-4;
        const auto p1=trace(unit(direction+tangent1*eps)*ammo.speed+inherited,time,k,out.steps);
        const auto p2=trace(unit(direction+tangent2*eps)*ammo.speed+inherited,time,k,out.steps);
        if(!p1.valid||!p2.valid){out.status=failure::budget;return out;}
        const vector j1=(p1.point-current.point)*(1/eps),j2=(p2.point-current.point)*(1/eps);
        const vector jt=current.velocity-as_double(target.velocity);
        const double determinant=dot(j1,cross(j2,jt));
        if(!std::isfinite(determinant)||std::abs(determinant)<1e-8*length(j1)*length(j2)*length(jt))return out;
        double a=dot(residual*(-1),cross(j2,jt))/determinant;
        double b=dot(j1,cross(residual*(-1),jt))/determinant;
        double dt=dot(j1,cross(j2,residual*(-1)))/determinant;
        double scale=1;
        const double angle=std::sqrt(a*a+b*b);if(angle>.35)scale=(std::min)(scale,.35/angle);
        if(std::abs(dt)>.75)scale=(std::min)(scale,.75/std::abs(dt));
        bool improved=false;
        for(int trial=0;trial<3;++trial,scale*=.5) {
            const double next_time=time+dt*scale;
            if(next_time<=.0001||next_time>4.)continue;
            const vector next_direction=unit(direction+(tangent1*a+tangent2*b)*scale);
            const auto next=trace(next_direction*ammo.speed+inherited,next_time,k,out.steps);
            if(!next.valid){out.status=failure::budget;return out;}
            if(length(next.point-(as_double(target_at(next_time).point)-origin))<error){direction=next_direction;time=next_time;improved=true;break;}
        }
        if(!improved)return out;
    }
    return out;
}
}
