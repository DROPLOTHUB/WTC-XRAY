#include "../src/features/esp/air_ghost_filter.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static esp::air_ghost_filter::tracker history;
int main(){
 using namespace esp::air_ghost_filter;
 const point frozen{100,1000,200};
 history.begin(1,true);
 assert(history.visible(1,true,frozen,1));
 for(int i=0;i<10000;++i)assert(history.visible(1,true,frozen,1));
 for(int i=1;i<20;++i)assert(history.visible(1,true,frozen,1+i*.1));
 assert(!history.visible(1,true,frozen,3));
 assert(!history.visible(1,true,frozen,3));
 assert(history.visible(1,true,{100.04f,1000,200},3.1));
 for(int i=0;i<40;++i)assert(history.visible(2,false,frozen,1+i*.1));
 assert(history.visible(3,true,frozen,1));assert(history.visible(3,true,frozen,4));
 assert(history.visible(3,true,frozen,3.9));
 assert(history.visible(4,true,frozen,1));assert(history.visible(4,true,frozen,1.5));
 assert(history.visible(4,true,frozen,3));
 history.begin(2,true);assert(history.visible(1,true,frozen,3.2));
 for(int i=1;i<=20;++i)history.visible(1,true,frozen,3.2+i*.1);
 assert(!history.visible(1,true,frozen,5.21));
 history.begin(2,false);assert(history.visible(1,true,frozen,5.3));
 history.begin(2,true);assert(history.visible(1,true,frozen,5.4));
 history.suspend();history.begin(2,true);assert(history.visible(1,true,frozen,5.5));
 history.begin(3,true);
 for(int i=0;i<200;++i){
  const float t=i*.05f;
  assert(history.visible(5,true,{100+t*.03f,1000,200},1+t));
  assert(history.visible(6,true,{100,1000+.10f*std::sin(t*4),200},1+t));
 }
 const float nan=std::numeric_limits<float>::quiet_NaN();
 assert(history.visible(7,true,{nan,0,0},1));
 assert(history.visible(7,true,frozen,std::numeric_limits<double>::infinity()));
 const auto start=std::chrono::steady_clock::now();
 unsigned checksum=0;
 for(unsigned i=0;i<1000000;++i)checksum+=history.visible(100+(i%10000),true,{float(i),2,3},10+i*.001);
 const double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/1000000;
 static_assert(sizeof(history)<=512*1024);
 const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
 const auto read=[](const auto& path){std::ifstream f(path);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});};
 const auto policy=read(root/"src/features/esp/air_ghost_filter.hpp");
 assert(policy.find("TargetProcess")==std::string::npos&&policy.find("unordered_map")==std::string::npos);
 const auto esp=read(root/"src/features/esp/esp.hpp");
 const auto filter=esp.find("current.visible(unit_base,unit_type_info.is_plane");
 assert(filter!=std::string::npos&&filter<esp.find("bool process_air_lead"));
 assert(esp.find("air_filter_guard")!=std::string::npos);
 std::printf("PASS bounded air-only frozen-position filter; storage=%zu bytes, host mean %.2f ns/call, checksum=%u\n",sizeof(history),ns,checksum);
}
