#pragma once
#include <cstddef>
#include <cstdint>

namespace startup_pointer_probe {
struct layout {
    std::uintptr_t cgame_rva=0, localplayer_rva=0;
    std::uintptr_t ballistics_field=0, camera_field=0;
};
struct result {
    bool ok=false;
    const char* reason="Sin lectura";
    std::uintptr_t cgame=0, localplayer=0, ballistics=0, camera=0;
};
inline bool plausible_pointer(std::uintptr_t address) {
    // Reject small tagged counters such as 0x100000001, not just null.
    return address>=0x10000 && address<=0x00007FFFFFFFF000ULL &&
           (address&7)==0;
}
template<class Reader> result inspect(std::uintptr_t base,std::uintptr_t module_size,
    const layout& offsets,Reader&& reader) {
    result out{};
    if(!plausible_pointer(base)||module_size<0x1000 ||
       module_size>0x100000000ULL ||
       offsets.cgame_rva>module_size-sizeof(std::uintptr_t) ||
       offsets.localplayer_rva>module_size-sizeof(std::uintptr_t) ||
       offsets.ballistics_field==0 || offsets.ballistics_field>0x100000 ||
       offsets.camera_field==0 || offsets.camera_field>0x100000) {
        out.reason="RVA base o campos fuera de rango";return out;
    }
    if(!reader(base+offsets.cgame_rva,&out.cgame,sizeof(out.cgame)) ||
       !plausible_pointer(out.cgame)) {
        out.reason="Global.g_GameContext no es un puntero valido";return out;
    }
    if(!reader(base+offsets.localplayer_rva,&out.localplayer,sizeof(out.localplayer)) ||
       !plausible_pointer(out.localplayer)) {
        out.reason="Global.g_LocalPlayer no es un puntero valido";return out;
    }
    if(!reader(out.cgame+offsets.ballistics_field,&out.ballistics,sizeof(out.ballistics)) ||
       !plausible_pointer(out.ballistics)) {
        out.reason="Game.m_BallisticsCont no es un puntero valido";return out;
    }
    if(!reader(out.cgame+offsets.camera_field,&out.camera,sizeof(out.camera)) ||
       !plausible_pointer(out.camera)) {
        out.reason="Game.m_CameraCont no es un puntero valido";return out;
    }
    out.ok=true;out.reason="Punteros de inicio validos";
    return out;
}
}
