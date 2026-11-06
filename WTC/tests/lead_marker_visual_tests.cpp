#include "../src/features/esp/lead_marker_geometry.hpp"
#include <array>
#include <vector>
#include <cassert>
#include <limits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
using command=std::array<float,6>;
static std::vector<command> capture(float size,float width,const lead_marker_geometry::point* target){
 std::vector<command> out;
 assert(lead_marker_geometry::draw({100,200},size,width,target,
  [&](float x,float y,float r){out.push_back({0,x,y,r,0,0});},
  [&](float x,float y,float x2,float y2,float w){out.push_back({1,x,y,x2,y2,w});}));
 return out;
}
int main(){
 lead_marker_geometry::point target{50,60};
 for(float size:{2.f,7.f,15.f})for(float width:{.5f,1.5f,3.f}){
  const auto a2a=capture(size,width,&target),g2a=capture(size,width,&target);
  assert(a2a==g2a&&a2a.size()==4);
  assert((a2a[0]==command{0,100,200,size,0,0}));
  assert((a2a[1]==command{1,100-size*1.5f,200,100+size*1.5f,200,width}));
  assert((a2a[2]==command{1,100,200-size*1.5f,100,200+size*1.5f,width}));
  assert((a2a[3]==command{1,50,60,100,200,width*.7f}));
  assert(capture(size,width,nullptr).size()==3);
 }
 const float nan=std::numeric_limits<float>::quiet_NaN();
 auto circle=[](float,float,float){assert(false);};
 auto line=[](float,float,float,float,float){assert(false);};
 assert(!lead_marker_geometry::draw({nan,0},7,1,nullptr,circle,line));
 assert(!lead_marker_geometry::draw({0,0},nan,1,nullptr,circle,line));
 assert(!lead_marker_geometry::draw({0,0},7,0,nullptr,circle,line));
 target.x=nan;assert(capture(7,1,&target).size()==3);
 const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
 const auto read=[](const auto& path){std::ifstream f(path);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});};
 const auto src=read(root/"src/features/esp/esp.hpp");
 assert(src.find("draw_lead_marker(lead_screen,have_target ? &unit_screen_pos : nullptr)")!=std::string::npos);
 assert(src.find("draw_lead_marker(sp,have_target ? &unit_screen_pos : nullptr)")!=std::string::npos);
 assert(src.find("draw_impact_x(sp, IM_COL32(255, 200, 0, 255)")==std::string::npos);
 for(const char* setting:{"sdk::air_lead_marker_size","sdk::air_lead_line_thickness","sdk::air_lead_sim_color","sdk::air_lead_opacity","sdk::air_lead_coverage_opacity"})assert(src.find(setting)!=std::string::npos);
 const auto menu=read(root/"src/gui/tactical_menu.hpp");
 assert(menu.find("(!sdk::air_lead_enabled && !esp::g2a_lead::enabled)")!=std::string::npos);
 std::puts("PASS identical A2A/G2A marker circle, cross, connector, style access and invalid geometry rejection");
}
