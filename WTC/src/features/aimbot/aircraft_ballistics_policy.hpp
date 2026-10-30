#pragma once
#include <cmath>
namespace aimbot::aircraft_ballistics_policy {
inline float caliber_for_air_solver(float caliber,bool in_mm) {
    return caliber*(in_mm ? 1.f : 1000.f);
}
inline bool real_speed(float speed) {
    return std::isfinite(speed) && speed>=50.f && speed<=4000.f;
}
inline bool usable_drag_geometry(float mass,float caliber_m) {
    return std::isfinite(mass) && mass>0.f &&
        std::isfinite(caliber_m) && caliber_m>0.f && caliber_m<=2.f;
}
}
