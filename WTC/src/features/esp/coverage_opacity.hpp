#pragma once
#include <algorithm>
#include <cmath>
#include "../../../lib/imgui/imgui.h"

// Opaque spatial coverage: the gaps remain genuinely transparent even when a
// driver/compositor mishandles partially transparent overlay pixels.
namespace lead_opacity
{
    inline bool visible(int index, float fraction)
    {
        if (!std::isfinite(fraction) || fraction <= 0.f) return false;
        if (fraction >= 1.f) return true;
        return ((index * 73) & 255) < static_cast<int>(fraction * 256.f);
    }
    inline ImU32 solid(ImU32 color)
    {
        return (color & ~IM_COL32_A_MASK) | (255u << IM_COL32_A_SHIFT);
    }
    inline void line(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 color, float thickness, float fraction)
    {
        if (!draw || fraction <= 0.f) return;
        color = solid(color);
        if (fraction >= 0.995f) { draw->AddLine(a,b,color,thickness); return; }
        const float length = std::hypot(b.x-a.x,b.y-a.y);
        const int count = std::clamp(static_cast<int>(std::ceil(length/2.5f)),1,256);
        for (int i=0;i<count;i++) {
            if (!visible(i,fraction)) continue;
            const float begin = float(i)/count, end = float(i+1)/count;
            draw->AddLine(ImVec2(a.x+(b.x-a.x)*begin,a.y+(b.y-a.y)*begin),
                ImVec2(a.x+(b.x-a.x)*end,a.y+(b.y-a.y)*end),color,thickness);
        }
    }
    inline void circle(ImDrawList* draw, ImVec2 center, float radius, ImU32 color, float fraction)
    {
        if (!draw || radius <= 0.f || fraction <= 0.f) return;
        color = solid(color);
        if (fraction >= 0.995f) { draw->AddCircle(center,radius,color,32); return; }
        constexpr float two_pi=6.283185307179586f;
        const int count=std::clamp(static_cast<int>(std::ceil(two_pi*radius/2.5f)),24,96);
        for (int i=0;i<count;i++) {
            if (!visible(i,fraction)) continue;
            const float a=two_pi*float(i)/count,b=two_pi*float(i+1)/count;
            draw->AddLine(ImVec2(center.x+radius*std::cos(a),center.y+radius*std::sin(a)),
                ImVec2(center.x+radius*std::cos(b),center.y+radius*std::sin(b)),color,1.f);
        }
    }
}
