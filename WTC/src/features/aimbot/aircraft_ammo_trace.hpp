#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <sstream>
#include <string>
#include "ballistic_validation.hpp"
namespace aimbot {
enum class ammo_pointer_state { not_read, ok, failed, zero, invalid };
inline const char* ammo_pointer_label(ammo_pointer_state state) {
    switch(state) {
    case ammo_pointer_state::ok:return "valido";
    case ammo_pointer_state::failed:return "lectura fallida";
    case ammo_pointer_state::zero:return "nulo";
    case ammo_pointer_state::invalid:return "fuera de rango";
    default:return "no consultado";
    }
}
struct AircraftAmmoDiagnostics {
    int weapon_count=-1, attempts=0, slot=-1, ammo_count=-1;
    int32_t props_id=0;
    uint32_t table_max_id=0;
    float cartridge_speed=0.f, table_speed=0.f;
    bool cartridge_read=false, table_read=false, table_pointer=false,
        table_id_in_range=false, table_entry=false;
    ammo_pointer_state array_state=ammo_pointer_state::not_read;
    ammo_pointer_state link_state=ammo_pointer_state::not_read;
    ammo_pointer_state cartridge_state=ammo_pointer_state::not_read;
};
template<class Reader> ammo_pointer_state read_ammo_pointer(Reader&& read,
    std::uintptr_t address,std::uintptr_t& out) {
    out=0;
    if(!read(address,&out,sizeof(out))) {out=0;return ammo_pointer_state::failed;}
    if(out==0)return ammo_pointer_state::zero;
    return ballistic_validation::plausible_pointer(out) ? ammo_pointer_state::ok : ammo_pointer_state::invalid;
}
// Preserve the existing layout exactly: count >= 9 adds one indirection.
// Never probe alternate layouts or guess a speed. Output contains no addresses.
template<class Reader> bool read_aircraft_cartridge(Reader&& read,
    std::uintptr_t slot_base,std::uintptr_t ammo_offset,int count,
    AircraftAmmoDiagnostics& diag,std::uintptr_t& cartridge,const char*& status) {
    cartridge=0;
    diag.array_state=diag.link_state=diag.cartridge_state=ammo_pointer_state::not_read;
    std::uintptr_t array=0;
    diag.array_state=read_ammo_pointer(read,slot_base+ammo_offset,array);
    if(diag.array_state!=ammo_pointer_state::ok) {status="Array de cartuchos no disponible";return false;}
    if(count>=9) {
        std::uintptr_t link=0;
        diag.link_state=read_ammo_pointer(read,array,link);
        if(diag.link_state!=ammo_pointer_state::ok) {status="Enlace indirecto de cartuchos no disponible";return false;}
        array=link;
    }
    diag.cartridge_state=read_ammo_pointer(read,array,cartridge);
    if(diag.cartridge_state!=ammo_pointer_state::ok) {
        cartridge=0;status="Cartucho activo no disponible";return false;
    }
    return true;
}
struct AircraftWeaponTrace {
    int index=-1;
    AircraftAmmoDiagnostics diag{};
    const char* status="Sin intento";
    bool valid=false;
};
struct AircraftAmmoTrace {
    std::array<AircraftWeaponTrace,128> rows{};
    std::size_t count=0;
    int selected=-1;
    void reset() {count=0;selected=-1;}
    void add(int index,const AircraftAmmoDiagnostics& diag,const char* status,bool valid) {
        if(count<rows.size())rows[count++]={index,diag,status,valid};
    }
    std::string report() const {
        std::ostringstream out;
        out<<"Armas consultadas: "<<count<<" | seleccionada: "<<selected<<" (indices desde 0)\n";
        for(std::size_t i=0;i<count;++i) {
            const auto& row=rows[i];const auto& d=row.diag;
            out<<"Arma "<<row.index<<" | slot "<<d.slot<<" | cartuchos "<<d.ammo_count
               <<" | cadena "<<(d.ammo_count<0 ? "sin muestra" : d.ammo_count>=9 ? "indirecta" : "directa")
               <<" | valida "<<(row.valid?"si":"no")<<" | "<<row.status<<'\n';
            out<<"  array: "<<ammo_pointer_label(d.array_state)<<" | enlace: "<<ammo_pointer_label(d.link_state)
               <<" | cartucho: "<<ammo_pointer_label(d.cartridge_state)<<" | ID "<<d.props_id<<'\n';
            out<<"  velocidad cartucho: ";if(d.cartridge_read)out<<d.cartridge_speed;else out<<"sin lectura";
            out<<" | velocidad tabla: ";if(d.table_read)out<<d.table_speed;else out<<"sin lectura";
            out<<" | tabla "<<(d.table_pointer?"si":"no")<<" | ID en rango "<<(d.table_id_in_range?"si":"no")
               <<" | entrada "<<(d.table_entry?"si":"no")<<" | max ID "<<d.table_max_id<<'\n';
        }
        return out.str();
    }
};
}
