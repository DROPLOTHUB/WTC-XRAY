#include "../src/features/esp/radar_geometry.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main(){
    using namespace esp::radar::geometry;
    auto live_map=live(-200.f,-100.f,400.f,200.f);
    float x=0,y=0;
    assert(project(live_map,-200.f,-100.f,x,y)&&x==0.f&&y==1.f);
    assert(project(live_map,0.f,0.f,x,y)&&x==0.5f&&y==0.5f);
    assert(project(live_map,200.f,100.f,x,y)&&x==1.f&&y==0.f);
    assert(!project(live_map,201.f,0.f,x,y));
    assert(!project(live_map,NAN,0.f,x,y));
    const auto centered=full(300.f,500.f,400.f,200.f);
    assert(project(centered,300.f,500.f,x,y)&&x==0.5f&&y==0.5f);
    assert(clamp_to_mode(live_map,-100.f,-70.f,100.f,70.f,1.f));
    assert(live_map.extent_x==200.f&&live_map.extent_z==140.f);
    assert(project(live_map,0.f,0.f,x,y)&&x==0.5f&&y==0.5f);
    auto view=live(-200.f,-100.f,400.f,200.f);
    assert(!clamp_to_mode(view,0.f,0.f,0.f,0.f,1.f));
    assert(project(view,0.f,0.f,x,y)&&x==0.5f&&y==0.5f);
    assert(clamp_to_mode(view,-100.f,-70.f,100.f,70.f,1.5f));
    assert(view.extent_x==400.f&&view.min_x==-200.f);
    std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/esp/radar.hpp");assert(in);
    const std::string src{std::istreambuf_iterator<char>(in),{}};
    const auto gate=src.find("ReadOptional(image + offsets::map_drawn_this_frame_offset");
    const auto batch=src.find("auto map_scatter=TargetProcess->CreateScatterHandle()");
    assert(gate!=std::string::npos && batch!=std::string::npos && gate<batch);
    assert(src.find("bytes_read==sizeof(T)")!=std::string::npos);
    assert(src.find("ReadOptional(address,&fresh,sizeof(fresh))")!=std::string::npos);
    assert(src.find("map_executed=map_scatter")!=std::string::npos);
    assert(src.find("bounds_executed=bounds_scatter")!=std::string::npos);
    assert(src.find("last_view")==std::string::npos);
    const auto invalid_overflow=live(3.e38f,0.f,3.e38f,1.f);assert(!invalid_overflow.valid());
    auto finite=live(0.f,0.f,10.f,10.f);
    assert(!clamp_to_mode(finite,-3.e38f,0.f,3.e38f,10.f,1.f));
    assert(finite.extent_x==10.f);
    std::puts("PASS live lower bound, full-map fallback, aspect and per-mode clamp");
}
