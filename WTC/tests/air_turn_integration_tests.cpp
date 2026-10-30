#include <cmath>
struct vec3_t {float x=0,y=0,z=0;};
#include "../src/features/esp/air_intercept.hpp"
#define air_lead_model air_lead_v510_reference
#include "fixtures/air_lead_v510_reference.hpp"
#undef air_lead_model
#include <cassert>
#include <cstdio>
#include <limits>
using namespace esp::air_intercept;
static vector reference(vec3_t velocity,vec3_t acceleration,double omega,double time) {
    const vector v=as_double(velocity),a=as_double(acceleration),direction=unit(v),normal=unit(cross(v,a)),tangent=cross(normal,direction);
    const double speed=length(v),along=(std::clamp)(dot(direction,a),-50.,50.),floor=(std::min)(40.,speed);
    const int count=int(std::ceil(time/.0002));const double dt=time/count;
    vector point{};
    for(int i=0;i<count;++i){const double active=(std::min)((i+.5)*dt,.85),s=(std::max)(floor,speed+along*active);
        point=point+(direction*std::cos(omega*active)+tangent*std::sin(omega*active))*(s*dt);}
    return point;
}
int main(){
    using namespace esp::air_lead_model;
    double worst=0;int cases=0;
    for(float speed:{25.f,60.f,250.f})for(float along:{-80.f,-20.f,20.f,100.f})for(float omega:{.0002f,.1f,.5f,2.f})for(float time:{.05f,.2f,.85f,2.5f,4.f}){
        const vec3_t v{speed,0,0},a{along,20,30};
        const auto result=propagate({},v,a,time,omega,true);
        worst=(std::max)(worst,length(as_double(result)-reference(v,a,omega,time)));++cases;
    }
    assert(worst<.001);
    const vec3_t v{200,0,0},a{0,40,0};
    const auto old=esp::air_lead_v510_reference::propagate({},v,a,.8f,.2f,true);
    const auto now=propagate({},v,a,.8f,.2f,true);const auto truth=reference(v,a,.2,.8);
    const double old_error=length(as_double(old)-truth),new_error=length(as_double(now)-truth);
    assert(old_error>.5 && new_error<.001);
    TurnState turn{};assert(update_turn_state({200,0,0},{80,0,0},.03f,1,&turn)==0);
    assert(update_turn_state({20,0,0},{0,150,0},.03f,2,&turn)<=kMaxOmega && turn.omega<=kMaxOmega);
    const auto invalid=propagate({1,2,3},v,a,std::numeric_limits<float>::quiet_NaN(),.2f,true);assert(invalid.x==1);
    std::printf("PASS %d exact 3D turn cases incl. deceleration floor/coast; worst %.6f m; representative old %.6f -> new %.6f m\n",cases,worst,old_error,new_error);
}
