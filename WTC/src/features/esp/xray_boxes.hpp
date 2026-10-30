#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>
#include "xray_parts.hpp"

namespace xray::boxes {
inline constexpr bool compact(xray_part_kind_t kind){return kind==xray_part_kind_t::Ammo;}
struct bounds3 {
    float min_x=0,min_y=0,min_z=0,max_x=0,max_y=0,max_z=0;
    bool valid=false;
};
template<class Vertex> bounds3 from_vertices(const std::vector<Vertex>& vertices) {
    bounds3 b{};
    for(const auto& v:vertices) {
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))continue;
        if(!b.valid) {
            b.min_x=b.max_x=v.x;b.min_y=b.max_y=v.y;b.min_z=b.max_z=v.z;b.valid=true;
        }else{
            b.min_x=std::min(b.min_x,v.x);b.max_x=std::max(b.max_x,v.x);
            b.min_y=std::min(b.min_y,v.y);b.max_y=std::max(b.max_y,v.y);
            b.min_z=std::min(b.min_z,v.z);b.max_z=std::max(b.max_z,v.z);
        }
    }
    return b;
}
template<class Vertex,class Index> bounds3 from_indexed_vertices(
    const std::vector<Vertex>& vertices,const std::vector<Index>& indices) {
    if(indices.empty())return from_vertices(vertices);
    bounds3 b{};
    for(Index i:indices) {
        if(static_cast<std::size_t>(i)>=vertices.size())continue;
        const auto& v=vertices[static_cast<std::size_t>(i)];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))continue;
        if(!b.valid) {
            b.min_x=b.max_x=v.x;b.min_y=b.max_y=v.y;b.min_z=b.max_z=v.z;b.valid=true;
        } else {
            b.min_x=std::min(b.min_x,v.x);b.max_x=std::max(b.max_x,v.x);
            b.min_y=std::min(b.min_y,v.y);b.max_y=std::max(b.max_y,v.y);
            b.min_z=std::min(b.min_z,v.z);b.max_z=std::max(b.max_z,v.z);
        }
    }
    return b;
}
template<class Callback> void each_corner(const bounds3& b,Callback&& callback){
    if(!b.valid)return;
    for(int i=0;i<8;++i)
        callback((i&1)?b.max_x:b.min_x,(i&2)?b.max_y:b.min_y,(i&4)?b.max_z:b.min_z);
}
inline constexpr std::size_t vertices_projected_per_module=8;
inline constexpr std::size_t lines_per_outline=4;
}
