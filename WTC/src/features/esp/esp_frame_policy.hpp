#pragma once
#include <cmath>
#include <chrono>
#include <cstdint>

namespace esp::frame_policy {
// G2G y A2A consumen el cache global solo en su propio tipo de vehiculo.
inline bool needs_global_ballistics(bool is_plane,bool ground_aimbot,
    bool air_lead,bool air_simulation) {
    return is_plane ? air_lead && air_simulation : ground_aimbot;
}
// La matriz local solo es necesaria para transformar el origen del arma.
inline bool needs_local_rotation(bool air_lead,bool air_simulation,bool air_weapon_origin) {
    return air_lead && air_simulation && air_weapon_origin;
}
// Equipo estable por unidad: refrescar al cambiar la unidad, la batalla o el TTL.
inline bool refresh_local_team(std::uintptr_t unit,std::uintptr_t cached_unit,
    std::uint64_t epoch,std::uint64_t cached_epoch,
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point next) {
    return unit!=cached_unit || epoch!=cached_epoch || now>=next;
}
// No leer detalles de unidades con posiciones invalidas o fuera del rango visual.
inline bool in_render_range(float distance_sq, float max_distance_sq) {
    return std::isfinite(distance_sq) && distance_sq >= 0.0f &&
        std::isfinite(max_distance_sq) && distance_sq <= max_distance_sq;
}
// Una caja 3D puede atravesar el plano cercano: conservar etiqueta en su centro
// aun cuando no sea seguro dibujar la caja con sus ocho esquinas.
inline float label_anchor(float center_y, float box_bottom_y, bool box_projected) {
    return box_projected && std::isfinite(box_bottom_y) && box_bottom_y > 0.0f
        ? box_bottom_y : center_y;
}
}
