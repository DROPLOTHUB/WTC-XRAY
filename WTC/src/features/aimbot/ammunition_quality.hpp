#pragma once
#include <algorithm>
#include <cmath>
namespace ammunition_quality {
inline bool usable_speed(float speed){return std::isfinite(speed)&&speed>=50.f&&speed<=4000.f;}
inline bool conflicting_speeds(float cartridge,float table){
    return usable_speed(cartridge)&&usable_speed(table)&&
        std::abs(cartridge-table)>std::max(20.f,std::max(cartridge,table)*.1f);
}
inline const char* freshness(double age_ms){
    if(!std::isfinite(age_ms)||age_ms<0)return "Sin muestra";
    if(age_ms>1000)return "Antigua: no usar para confirmar el arma";
    if(age_ms>200)return "Retrasada";
    return "Reciente (no garantiza impacto)";
}
}
