#pragma once
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>

namespace esp::reload_display_policy {
inline constexpr std::size_t max_indicators = 16; // acotar lecturas DMA en el hilo de render
inline constexpr std::uint8_t visual_max = 16; // escala visual heredada; NO segundos
inline bool eligible(bool enabled,bool supported,float distance_sq,float range_sq) {
    return enabled && supported && std::isfinite(distance_sq) &&
        std::isfinite(range_sq) && distance_sq>=0.f && distance_sq<=range_sq;
}
inline bool supported_visual_value(std::uint8_t value) {return value<=visual_max;}
inline float ready_fraction(std::uint8_t value) {
    if (!supported_visual_value(value)) return 0.f;
    return std::clamp(1.f-static_cast<float>(value)/visual_max,0.f,1.f);
}
}
