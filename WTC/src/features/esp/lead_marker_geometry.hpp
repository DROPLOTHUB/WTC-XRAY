#pragma once
#include <cmath>

// Shared screen-space geometry only: no solver, histories or game reads.
namespace lead_marker_geometry {
struct point {float x=0,y=0;};
inline bool finite(point p){return std::isfinite(p.x)&&std::isfinite(p.y);}
template<class Circle,class Line>
inline bool draw(point center,float size,float thickness,const point* target,Circle circle,Line line){
    if(!finite(center)||!std::isfinite(size)||!std::isfinite(thickness)||size<=0||thickness<=0)return false;
    const float cross=size*1.5f;
    circle(center.x,center.y,size);
    line(center.x-cross,center.y,center.x+cross,center.y,thickness);
    line(center.x,center.y-cross,center.x,center.y+cross,thickness);
    if(target&&finite(*target))line(target->x,target->y,center.x,center.y,thickness*.7f);
    return true;
}
}
