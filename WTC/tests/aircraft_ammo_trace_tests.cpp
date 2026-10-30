#include "../src/features/aimbot/aircraft_ammo_trace.hpp"
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <cstdio>
int main() {
    using namespace aimbot;
    std::unordered_map<std::uintptr_t,std::uintptr_t> memory;
    std::vector<std::uintptr_t> reads;
    auto read=[&](std::uintptr_t address,void* out,size_t bytes) {
        reads.push_back(address);assert(bytes==sizeof(std::uintptr_t));
        auto it=memory.find(address);if(it==memory.end())return false;
        std::memcpy(out,&it->second,bytes);return true;
    };
    constexpr std::uintptr_t base=0x100000, array=0x200000, link=0x300000, ammo=0x400000;
    AircraftAmmoDiagnostics d{};const char* status="";std::uintptr_t result=0;
    memory={{base+0xB0,array},{array,ammo}};
    assert(read_aircraft_cartridge(read,base,0xB0,5,d,result,status));
    assert(result==ammo && reads.size()==2 && d.link_state==ammo_pointer_state::not_read);
    reads.clear();memory={{base+0xB0,array},{array,link},{link,ammo}};
    assert(read_aircraft_cartridge(read,base,0xB0,9,d,result,status));
    assert(result==ammo && reads.size()==3 && d.link_state==ammo_pointer_state::ok);
    reads.clear();memory={{base+0xB0,array},{array,0}};
    assert(!read_aircraft_cartridge(read,base,0xB0,5,d,result,status));
    assert(result==0 && d.array_state==ammo_pointer_state::ok && d.cartridge_state==ammo_pointer_state::zero);
    AircraftAmmoTrace trace;d.ammo_count=5;d.slot=0;trace.add(0,d,status,false);
    memory.erase(array);
    assert(!read_aircraft_cartridge(read,base,0xB0,5,d,result,status));
    assert(d.cartridge_state==ammo_pointer_state::failed);
    trace.add(1,d,status,false);
    memory[array]=12;
    assert(!read_aircraft_cartridge(read,base,0xB0,5,d,result,status));
    assert(d.cartridge_state==ammo_pointer_state::invalid);
    trace.add(2,d,status,false);
    memory[array]=0;
    assert(!read_aircraft_cartridge(read,base,0xB0,9,d,result,status));
    assert(d.link_state==ammo_pointer_state::zero && d.cartridge_state==ammo_pointer_state::not_read);
    memory.clear();reads.clear();
    assert(!read_aircraft_cartridge(read,base,0xB0,5,d,result,status));
    assert(reads.size()==1 && d.array_state==ammo_pointer_state::failed);
    assert(trace.rows[0].diag.cartridge_state==ammo_pointer_state::zero);
    assert(trace.rows[1].diag.cartridge_state==ammo_pointer_state::failed);
    assert(trace.rows[2].diag.cartridge_state==ammo_pointer_state::invalid);
    for(int i=3;i<10;++i)trace.add(i,d,status,false);
    const auto report=trace.report();
    for(int i=0;i<10;++i)assert(report.find("Arma "+std::to_string(i)+" |")!=std::string::npos);
    assert(report.find("nulo")!=std::string::npos && report.find("lectura fallida")!=std::string::npos);
    assert(report.find("0x")==std::string::npos);
    for(int i=10;i<140;++i)trace.add(i,d,status,false);
    assert(trace.count==128);
    trace.selected=7;trace.reset();assert(trace.count==0 && trace.selected==-1);
    assert(trace.report().find("Arma 0 |")==std::string::npos);
    std::puts("PASS direct/indirect cartucho chain, null versus unreadable versus range, 10 retained weapons, bounded sanitized report");
}
