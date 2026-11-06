#include "../src/core/startup_pointer_probe.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <unordered_map>
int main(){
    constexpr std::uintptr_t image=0x00007ff6281a0000ULL,size=0x840a000ULL;
    constexpr std::uintptr_t cached_rva=0x7a293b8, compiled_rva=0x7a4b848;
    constexpr std::uintptr_t game=0x000001aa12340000ULL,player=0x000001aa12350000ULL;
    constexpr std::uintptr_t ballistics=0x000001aa12360000ULL,camera=0x000001aa12370000ULL;
    std::unordered_map<std::uintptr_t,std::uintptr_t> memory{
        {image+cached_rva,0x100000001ULL}, // supplied log: two counters, not a pointer
        {image+compiled_rva,game},
        {image+0x7a22cb0,player},
        {game+0x3f0,ballistics},
        {game+0x660,camera}
    };
    std::size_t reads=0;
    const auto reader=[&](std::uintptr_t address,void* out,std::size_t size){
        ++reads;
        const auto value=memory.find(address);
        if(value==memory.end()||size!=sizeof(value->second))return false;
        std::memcpy(out,&value->second,size);return true;
    };
    startup_pointer_probe::layout api{cached_rva,0x7a22cb0,0x3f0,0x660};
    const auto bad=startup_pointer_probe::inspect(image,size,api,reader);
    assert(!bad.ok && bad.cgame==0x100000001ULL && reads==1);
    assert(memory.find(0x1000003f1ULL)==memory.end()); // never dereference this address
    startup_pointer_probe::layout embedded{compiled_rva,0x7a22cb0,0x3f0,0x660};
    const auto good=startup_pointer_probe::inspect(image,size,embedded,reader);
    assert(good.ok&&good.cgame==game&&good.ballistics==ballistics&&good.camera==camera);
    memory[game+0x3f0]=0;assert(!startup_pointer_probe::inspect(image,size,embedded,reader).ok);
    memory[game+0x3f0]=ballistics;
    api.cgame_rva=size;assert(!startup_pointer_probe::inspect(image,size,api,reader).ok);
    std::puts("PASS rejects 0x100000001 before 0x1000003f1 read; compiled fallback candidate valid");
}
