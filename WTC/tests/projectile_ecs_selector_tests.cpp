#include "../src/features/esp/projectile_ecs.hpp"
#include "../src/features/esp/projectile_visibility.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <cstdio>
int main(){
    using namespace projectile_ecs;
    constexpr std::uintptr_t base=0x00007ff6281a0000ULL;
    constexpr std::uintptr_t manager=base+0x7e6d828, selector_at=base+0x7a08490;
    constexpr std::uintptr_t data=0x000001aa10000000ULL,index=0x000001aa20000000ULL;
    constexpr std::uint32_t raw=0x8f7a75; // actual supplied log: 9402997
    constexpr std::uintptr_t record_at=index+std::uintptr_t(raw)*0x40;
    constexpr std::uintptr_t storage=0x000001aa30000000ULL;
    constexpr std::uintptr_t owner=0x000001aa40000000ULL,source=0x000001aa50000000ULL;
    constexpr std::uintptr_t name_container=0x000001aa70000000ULL,name_text=0x000001aa71000000ULL;
    std::unordered_map<std::uintptr_t,std::vector<std::uint8_t>> mem;
    auto put=[&](std::uintptr_t addr,const auto& value){
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(&value);
        mem[addr]=std::vector<std::uint8_t>(bytes,bytes+sizeof(value));
    };
    put(selector_at,raw);put(manager+0x178,data);put(manager+0x570,index);
    std::uint8_t record[0x38]{};
    record[0]=2; record[2]=1; // 2 required fields; 1 sublist
    std::uint16_t id=2,offset1=8;
    std::memcpy(record+4,&id,sizeof(id));
    std::memcpy(record+0x1a,&offset1,sizeof(offset1));
    put(record_at,record);
    std::uint8_t descriptor[0x20]{};std::uint32_t count=1;
    std::memcpy(descriptor,&storage,sizeof(storage));
    std::memcpy(descriptor+8,&count,sizeof(count)); // shift=0, one row
    put(data+2*0x20,descriptor);
    put(storage,source);mem[storage+8]={1};
    put(source+0x720,name_container);put(name_container+0x870,name_text);
    std::uint8_t name[63]{};std::memcpy(name,"aim_9m",7);put(name_text,name);
    Vec3 position{100.f,200.f,300.f},velocity{900.f,0.f,0.f};
    put(source+0x2c0,position);put(source+0x2dc,velocity);put(source+0x58,owner);
    const auto reader=[&](std::uintptr_t address,void* dest,std::size_t size){
        const auto found=mem.find(address);
        if(found==mem.end()||found->second.size()<size)return false;
        std::memcpy(dest,found->second.data(),size);return true;
    };
    Layout layout{};layout.manager_rva=0x7e6d828;
    layout.rocket_selector_rva=0x7a08490;layout.aam_only=true;
    Diagnostics d{};std::vector<Candidate> out;
    read_query(reader,base,layout,false,d,out);
    assert(d.selector==raw && d.raw_selector==raw && d.record==record_at);
    assert(d.failure==Failure::none && d.accepted==1 && out.size()==1);
    assert(std::strcmp(out[0].name,"aim_9m")==0 && !out[0].is_bomb);
    assert(name_is_air_to_air("AIM-9M") && name_is_air_to_air("R-73"));
    assert(!name_is_air_to_air("AGM-65") && !name_is_air_to_air("kh-29"));
    assert(name_has_mm("100mm_9m_117m1") && name_has_mm("127MM_spike_lr2"));
    assert(!name_has_mm("aim_9m") && !name_has_mm("sws_flz_lwf_63.blk"));
    assert(name_is_air_to_air("ffa_p16^sws_flz_lwf_63.blk"));
    assert(!name_is_air_to_air("ffa_p16^sws_flz_lwf_630.blk"));
    assert(!name_is_air_to_air("ffa_p16^sws_flz_lwf_63.blk_agm"));
    std::memset(name,0,sizeof(name));std::memcpy(name,"ffa_p16^sws_flz_lwf_63.blk",sizeof("ffa_p16^sws_flz_lwf_63.blk"));put(name_text,name);
    out.clear();read_query(reader,base,layout,false,d,out);
    assert(d.failure==Failure::none && d.not_aam==0 && d.accepted==1 && out.size()==1);
    assert(std::strcmp(out[0].name,"sws_flz_lwf_63")==0);
    std::memset(name,0,sizeof(name));std::memcpy(name,"flare_aim_9",12);put(name_text,name);
    out.clear();read_query(reader,base,layout,false,d,out);
    assert(out.empty() && d.flares==1);
    std::memset(name,0,sizeof(name));std::memcpy(name,"agm_65",7);put(name_text,name);
    out.clear();read_query(reader,base,layout,false,d,out);
    assert(out.empty() && d.not_aam==1);
    layout.name_allowed=projectile_visibility::callback;
    layout.name_context=&layout.aam_only;
    projectile_visibility::set("agm_65",true);
    out.clear();read_query(reader,base,layout,false,d,out);
    assert(d.accepted==1 && out.size()==1 && d.not_aam==0);
    projectile_visibility::set("agm_65",false);
    out.clear();read_query(reader,base,layout,false,d,out);
    assert(out.empty() && d.not_aam==1);
    std::memset(name,0,sizeof(name));std::memcpy(name,"100mm_9m_117m1",sizeof("100mm_9m_117m1"));put(name_text,name);
    out.clear();read_query(reader,base,layout,false,d,out);
    assert(out.empty() && d.caliber_ignored==1 && d.not_aam==0);
    assert(!projectile_visibility::allow("100mm_9m_117m1",false));
    mem.erase(record_at);out.clear();read_query(reader,base,layout,false,d,out);
    assert(out.empty() && d.failure==Failure::read && d.record==record_at);
    std::puts("PASS 24-bit ECS selector reaches valid AAM; flares, AGMs and absent records rejected");
}
