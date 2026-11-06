#pragma once
#include <algorithm>
#include <cmath>

namespace esp::radar::geometry {
struct window {
    float min_x=0.f,min_z=0.f,extent_x=0.f,extent_z=0.f;
    bool valid() const {return std::isfinite(min_x)&&std::isfinite(min_z)&&
        std::isfinite(extent_x)&&std::isfinite(extent_z)&&extent_x>0.001f&&extent_z>0.001f&&
        std::isfinite(min_x+extent_x)&&std::isfinite(min_z+extent_z);}
};
// The game's MapWorldCenter is the lower/pan bound, not the geometric center.
inline window live(float lower_x,float lower_z,float size_x,float size_z) {
    return {lower_x,lower_z,size_x,size_z};
}
inline window full(float center_x,float center_z,float size_x,float size_z) {
    return {center_x-size_x*0.5f,center_z-size_z*0.5f,size_x,size_z};
}
inline bool clamp_to_mode(window& view,float min_x,float min_z,float max_x,float max_z,float aspect) {
    if(!view.valid()||!std::isfinite(min_x)||!std::isfinite(min_z)||
       !std::isfinite(max_x)||!std::isfinite(max_z)||!std::isfinite(aspect)||
       max_x<=min_x||max_z<=min_z||aspect<=0.f)return false;
    const float span_x=max_x-min_x,span_z=max_z-min_z;
    if(!std::isfinite(span_x)||!std::isfinite(span_z))return false;
    if(std::fabs(aspect-1.f)<=0.0001f) {
        view.extent_x=std::min(view.extent_x,span_x);
        view.extent_z=std::min(view.extent_z,span_z);
    }
    // A zoomed-out view may be larger than the mode bounds. In that case keep
    // it centred instead of clamping against an inverted [min,max] interval.
    view.min_x=view.extent_x>=span_x ? min_x+(span_x-view.extent_x)*0.5f :
        std::clamp(view.min_x,min_x,max_x-view.extent_x);
    view.min_z=view.extent_z>=span_z ? min_z+(span_z-view.extent_z)*0.5f :
        std::clamp(view.min_z,min_z,max_z-view.extent_z);
    return true;
}
inline bool project(const window& view,float x,float z,float& u,float& v) {
    if(!view.valid()||!std::isfinite(x)||!std::isfinite(z))return false;
    u=(x-view.min_x)/view.extent_x;
    v=1.f-(z-view.min_z)/view.extent_z;
    return std::isfinite(u)&&std::isfinite(v)&&u>=0.f&&u<=1.f&&v>=0.f&&v<=1.f;
}
}
