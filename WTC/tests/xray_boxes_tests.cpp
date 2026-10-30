#include "../src/features/esp/xray_boxes.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
struct point {float x,y,z;};
int main(){
    using namespace xray::boxes;
    using K=xray::xray_part_kind_t;
    assert(compact(K::Ammo));
    for(const auto normal:{K::GunBarrel,K::CannonBreech,K::Crew,K::Fuel,
                           K::Engine,K::Transmission,K::Body})assert(!compact(normal));
    std::vector<point> vertices{{-2.f,-1.f,-3.f},{2.f,1.f,3.f},{0.f,0.f,0.f}};
    for(int i=0;i<997;++i)vertices.push_back({999.f,999.f,999.f}); // unused mesh data
    std::vector<std::uint32_t> indices(3000);
    for(std::size_t i=0;i<indices.size();++i)indices[i]=static_cast<std::uint32_t>(i%3);
    const auto b=from_indexed_vertices(vertices,indices);
    assert(b.valid&&b.min_x==-2.f&&b.max_x==2.f);
    assert(b.min_z==-3.f&&b.max_z==3.f); // unreferenced outlier is ignored
    std::size_t projected=0;
    each_corner(b,[&](float x,float y,float z){
        assert(x==-2.f||x==2.f);assert(y==-1.f||y==1.f);assert(z==-3.f||z==3.f);
        ++projected;
    });
    assert(projected==vertices_projected_per_module&&lines_per_outline==4);
    const auto old_lines=(indices.size()/3)*3;
    const auto new_lines=lines_per_outline;
    assert(old_lines==3000&&new_lines==4&&old_lines/new_lines==750);
    const auto fallback=from_indexed_vertices(vertices,std::vector<std::uint32_t>{});
    assert(fallback.valid&&fallback.max_x==999.f);
    std::printf("PASS Ammo-only: synthetic 1000 vertices / %zu mesh lines -> %zu box corners / %zu edges per ammo module; normal kinds unchanged\n",
        old_lines,projected,new_lines);
}
