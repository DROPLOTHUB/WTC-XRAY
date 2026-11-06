#pragma once

// Dependency-free integration surface for menu/config; opt-in by default.
namespace esp::g2a_lead {
inline bool enabled = false;
inline const char* status = "G2A desactivado";
inline float flight_time = 0, model_error = 0;
inline float sample_age_ms = -1; // -1: no current motion sample
inline const char* quality = "Sin muestra actual";
inline const char* own_source = "Sin velocidad fiable";
inline const char* target_source = "Sin velocidad fiable";
inline const char* acceleration_source = "Sin aceleracion fiable";
inline const char* ammunition_status = "Municion activa: arma no verificada (sin seleccion G2A garantizada)";
inline void reset_diagnostics() {
    flight_time=0;model_error=0;sample_age_ms=-1;
    quality="Sin muestra actual";own_source="Sin velocidad fiable";
    target_source="Sin velocidad fiable";acceleration_source="Sin aceleracion fiable";
}
}
