#pragma once
#include <array>
#include <cmath>

// G2G-only lifetime and diagnostics. No DMA, persistence or shared air settings.
namespace g2g_runtime {
using round_signature = std::array<float,6>; // speed, mass, caliber m, length, drag-area, max range
inline bool sample_valid(float age) { return std::isfinite(age) && age>=0 && age<=.25f; }
inline bool marker_valid(float solve_age,float sample_age) {
    return sample_valid(solve_age) && sample_valid(sample_age);
}
inline bool newer_sample(double interval) { return std::isfinite(interval) && interval>0; }
inline bool signature_valid(const round_signature& s) {
    for(float v:s) if(!std::isfinite(v)) return false;
    return s[0]>=50 && s[0]<=4000 && s[1]>0 && s[2]>0 && s[3]>0 && s[4]>=0 && s[5]>=0;
}
struct diagnostics {
    bool valid=false;
    float sample_age_ms=-1,solve_age_ms=-1,flight_time=0,error=0;
    int steps=0;
    const char* own_source="Sin muestra propia fiable";
    const char* target_source="Sin muestra objetivo fiable";
};
inline diagnostics current{};
inline void reset_diagnostics(){current={};}
}
