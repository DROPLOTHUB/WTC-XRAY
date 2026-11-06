// Focused runtime regression harness: actual radar.hpp, mocked DMA/ImGui only.
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
using DWORD=uint32_t;
using VMMDLL_SCATTER_HANDLE=void*;
struct vec2_t {float x=0,y=0;};
struct vec3_t {float x=0,y=0,z=0;};
struct ImVec2 {float x=0,y=0;ImVec2()=default;ImVec2(float a,float b):x(a),y(b){}};
using ImU32=uint32_t;
#define IM_COL32(r,g,b,a) 0u
struct ImDrawList {
    int clips=0,primitives=0;
    void PushClipRect(ImVec2,ImVec2,bool){++clips;} void PopClipRect(){--clips;}
    template<class... T> void AddLine(T...){++primitives;}
    template<class... T> void AddTriangleFilled(T...){++primitives;}
    template<class... T> void AddCircleFilled(T...){++primitives;}
    template<class... T> void AddCircle(T...){++primitives;}
    template<class... T> void AddRectFilled(T...){++primitives;}
    template<class... T> void AddRect(T...){++primitives;}
    template<class... T> void AddQuadFilled(T...){++primitives;}
    template<class... T> void AddQuad(T...){++primitives;}
};
struct ImGuiViewport {ImVec2 Pos{50,30},Size{1000,800};};
namespace ImGui {
    inline ImGuiViewport viewport;inline ImDrawList list;
    inline ImGuiViewport* GetMainViewport(){return &viewport;}
    inline ImDrawList* GetForegroundDrawList(){return &list;}
    inline ImU32 ColorConvertFloat4ToU32(int){return 0;}
}
// Synthetic addresses are only a fixture, not production offset replacements.
namespace offsets {
    constexpr uintptr_t map_drawn_this_frame_offset=1,map_minimap_pos_offset=16,map_minimap_size_offset=32;
    constexpr uintptr_t map_full_world_center_offset=48,map_full_world_size_offset=64;
    constexpr uintptr_t map_world_center_offset=80,map_view_size_offset=96,tactical_map_obj_offset=112;
    namespace tactical_map_offsets {constexpr uintptr_t mode=0,mode_stride=32,bounds_min=8,bounds_max=16,aspect=128;}
    namespace unit_offsets {constexpr uintptr_t info_offset=16,unitArmyNo_offset=32;}
    namespace unit_info_offsets {constexpr uintptr_t unit_type_offset=0x38;}
    namespace localplayer {constexpr uintptr_t localunit_offset=16,ownedUnitRef_offset=24,controlledUnitRef_offset=32;}
}
namespace runtime {inline uintptr_t image=0x100000;inline uintptr_t base_address(){return image;}}
struct Player {uintptr_t get_base(){return 0x200000;}};
namespace sdk {inline bool radar_enabled=true;inline int radar_enemy_color=0;inline Player player;inline Player* cLocalPlayer=&player;}
struct Memory {
    struct Request{uintptr_t address;void* dest;size_t size;DWORD* count;};
    std::unordered_map<uintptr_t,std::vector<unsigned char>> data;
    std::vector<Request> requests;
    bool scatter=true,execute=true,prepare=true,partial=false,close_gate=false;
    uintptr_t partial_address=0;
    int fallbacks=0;
    template<class T> void put(uintptr_t a,T value){auto& b=data[a];b.resize(sizeof(T));std::memcpy(b.data(),&value,sizeof(T));}
    void text(uintptr_t a,const char* value){auto& b=data[a];b.assign(64,0);std::memcpy(b.data(),value,std::strlen(value));}
    bool copy(uintptr_t a,void* d,size_t n){auto it=data.find(a);if(it==data.end()||it->second.size()<n)return false;std::memcpy(d,it->second.data(),n);return true;}
    bool ReadOptional(uintptr_t a,void* d,size_t n){++fallbacks;return copy(a,d,n);}
    template<class T> T Read(uintptr_t a){T v{};ReadOptional(a,&v,sizeof(v));return v;}
    VMMDLL_SCATTER_HANDLE CreateScatterHandle(){requests.clear();return scatter?this:nullptr;}
    bool AddScatterReadRequest(VMMDLL_SCATTER_HANDLE,uintptr_t a,void* d,size_t n,DWORD* count){if(!prepare)return false;requests.push_back({a,d,n,count});return true;}
    bool ExecuteReadScatter(VMMDLL_SCATTER_HANDLE,int=0,bool=false){
        for(auto& r:requests)if(copy(r.address,r.dest,r.size))*r.count=(partial&&(!partial_address||partial_address==r.address))?1:static_cast<DWORD>(r.size);
        if(close_gate)put(runtime::image+offsets::map_drawn_this_frame_offset,uint8_t{0});
        return execute;
    }
    void CloseScatterHandle(VMMDLL_SCATTER_HANDLE){}
};
inline Memory memory;inline Memory* TargetProcess=&memory;
#include "../src/features/esp/radar.hpp"
#include "../src/features/esp/air_ghost_filter.hpp"
constexpr uintptr_t local_base=0x300000,enemy=0x400000,info=0x500000,tactical=0x600000;
void setup(){
    memory=Memory{};ImGui::list={};ImGui::viewport={};sdk::radar_enabled=true;runtime::image=0x100000;
    const auto b=runtime::image;
    memory.put(b+offsets::map_drawn_this_frame_offset,uint8_t{1});
    memory.put(b+offsets::map_minimap_pos_offset,vec2_t{100,100});
    memory.put(b+offsets::map_minimap_size_offset,vec2_t{400,200});
    memory.put(b+offsets::map_world_center_offset,vec2_t{-200,-100});
    memory.put(b+offsets::map_view_size_offset,vec2_t{400,200});
    memory.put(b+offsets::map_full_world_center_offset,vec2_t{0,0});
    memory.put(b+offsets::map_full_world_size_offset,vec2_t{800,400});
    memory.put(0x200000+offsets::localplayer::localunit_offset,local_base);
    memory.put(local_base+offsets::unit_offsets::unitArmyNo_offset,uint8_t{1});
    memory.put(enemy+offsets::unit_offsets::unitArmyNo_offset,uint8_t{2});
    memory.put(enemy+offsets::unit_offsets::info_offset,info);
    memory.put(info+0x30,uintptr_t{0x700000});memory.put(info+0x38,uintptr_t{0x700100});
    memory.text(0x700000,"");memory.text(0x700100,"exp_fighter");
    esp::radar::symbol_cache.clear();
}
int main(){
    using namespace esp::radar;
    setup();auto view=read_map_view();assert(view.valid&&view.screen_min.x==150&&view.world_min.x==-200);
    // Real zero position/pan is valid; a failed zero-filled read is not.
    memory.put(runtime::image+offsets::map_minimap_pos_offset,vec2_t{});
    memory.put(runtime::image+offsets::map_world_center_offset,vec2_t{});assert(read_map_view().valid);
    memory.data.erase(runtime::image+offsets::map_world_center_offset);assert(!read_map_view().valid);
    setup();memory.data.erase(runtime::image+offsets::map_minimap_pos_offset);assert(!read_map_view().valid);
    for(int failure=0;failure<4;++failure){setup();if(failure==0)memory.scatter=false;if(failure==1)memory.prepare=false;if(failure==2)memory.execute=false;if(failure==3)memory.partial=true;assert(read_map_view().valid);}
    setup();memory.close_gate=true;assert(!read_map_view().valid);
    setup();memory.put(runtime::image+offsets::map_view_size_offset,vec2_t{});view=read_map_view();assert(view.valid&&view.world_min.x==-400);
    setup();memory.put(runtime::image+offsets::map_world_center_offset,vec2_t{-50,-25});memory.put(runtime::image+offsets::map_view_size_offset,vec2_t{100,50});view=read_map_view();assert(view.valid&&view.world_min.x==-50&&view.world_size.x==100);
    setup();memory.put(runtime::image+offsets::tactical_map_obj_offset,tactical);
    memory.put(tactical+offsets::tactical_map_offsets::bounds_min,vec2_t{-10,-10});
    memory.put(tactical+offsets::tactical_map_offsets::bounds_max,vec2_t{10,10});memory.put(tactical+offsets::tactical_map_offsets::aspect,1.f);
    view=read_map_view();assert(view.valid&&view.world_size.x==400); // failed mode is not mode 0
    memory.put(tactical+offsets::tactical_map_offsets::mode,uint32_t{0});view=read_map_view();assert(view.valid&&view.world_size.x==20);
    memory.data.erase(tactical+offsets::tactical_map_offsets::bounds_min);view=read_map_view();assert(view.valid&&view.world_size.x==400);
    setup();ImGui::viewport.Size.x=INFINITY;assert(!read_map_view().valid);
    setup();memory.put(runtime::image+offsets::map_minimap_size_offset,vec2_t{1001,200});assert(!read_map_view().valid);
    std::unordered_map<uintptr_t,vec3_t> contacts{{enemy,{0,0,0}},{local_base,{0,0,0}}};
    setup();draw(contacts);assert(rendered_contacts==1&&ImGui::list.primitives>0&&ImGui::list.clips==0);
    auto primitives=ImGui::list.primitives;
    memory.put(runtime::image+offsets::map_drawn_this_frame_offset,uint8_t{0});draw(contacts);assert(rendered_contacts==0&&ImGui::list.primitives==primitives); // no grace after closing
    setup();memory.data.erase(local_base+offsets::unit_offsets::unitArmyNo_offset);draw(contacts);assert(rendered_contacts==0);
    setup();memory.put(enemy+offsets::unit_offsets::unitArmyNo_offset,uint8_t{1});draw(contacts);assert(rendered_contacts==0);
    setup();memory.data.erase(enemy+offsets::unit_offsets::unitArmyNo_offset);draw(contacts);assert(rendered_contacts==0);
    setup();contacts[enemy]={201,0,0};draw(contacts);assert(rendered_contacts==0);contacts[enemy]={0,0,0};
    setup();assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::fighter);
    memory.put(enemy+offsets::unit_offsets::info_offset,info+256);assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::unknown);
    for(uintptr_t i=0;i<1100;++i){const auto unit=0x800000+i*0x100;memory.put(unit+offsets::unit_offsets::info_offset,info);classify_contact(unit,runtime::image,local_base);assert(symbol_cache.size()<=512);}
    classify_contact(enemy,runtime::image+1,local_base);assert(symbol_cache.size()==1);
    setup();assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::fighter);symbol_cache[enemy].refresh={};memory.text(0x700100,"exp_bomber");assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::bomber);
    setup();memory.text(0x700000,"Light Cruiser");memory.text(0x700100,"exp_ship");assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::naval);
    setup();memory.data.erase(info+0x30);assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::fighter); // coarse works without subtype
    setup();memory.data.erase(info+0x38);assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::unknown);
    assert(symbol_cache[enemy].refresh-std::chrono::steady_clock::now()<=std::chrono::milliseconds(250));
    memory.put(info+0x38,uintptr_t{0x700100});symbol_cache[enemy].refresh={};assert(classify_contact(enemy,runtime::image,local_base)==icons::kind::fighter);
    setup();icons::visible[static_cast<size_t>(icons::kind::fighter)]=false;draw(contacts);assert(rendered_contacts==0);icons::visible[static_cast<size_t>(icons::kind::fighter)]=true;
    setup();memory.data.erase(0x200000+offsets::localplayer::localunit_offset);memory.put(0x200000+offsets::localplayer::controlledUnitRef_offset,local_base|uintptr_t{1});draw(contacts);assert(rendered_contacts==1);
    setup();draw(contacts,[](uintptr_t,const vec3_t&,icons::kind){return false;});assert(rendered_contacts==0);
    esp::air_ghost_filter::current.begin(77,true);
    double stamp=1;
    auto gate=[&](uintptr_t id,const vec3_t& p,icons::kind k){
        const bool air=k==icons::kind::fighter||k==icons::kind::strike||k==icons::kind::bomber||k==icons::kind::helicopter;
        return esp::air_ghost_filter::current.visible(id,air,{p.x,p.y,p.z},stamp);
    };
    setup();
    for(int i=0;i<=20;++i){stamp=1+i*.1;draw(contacts,gate);}
    assert(rendered_contacts==0);
    stamp=3.1;contacts[enemy].y=.1f;draw(contacts,gate);assert(rendered_contacts==1);
    setup();memory.text(0x700100,"exp_SPAA");
    for(int i=0;i<=30;++i){stamp=4+i*.1;draw(contacts,gate);assert(rendered_contacts==1);}
    std::puts("PASS radar checked reads, visibility, bounds/pan/zoom, teams, clipping, capped symbol refresh and shared air-only ghost gate");
}
