#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include "radar_symbols.hpp"

// Símbolos tácticos vectoriales: no dependen de la fuente/atlas del proceso remoto.
// El proceso DMA nunca entrega punteros útiles al ImGui del overlay.
namespace esp::radar::icons
{
    struct entry_t
    {
        kind symbol = kind::unknown;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    inline bool safe_pointer(uintptr_t p)
    {
        // Usuarios x64 canónicos, sin punteros nulos, bajos o kernel.
        return p >= 0x10000 && p < static_cast<uintptr_t>(0x0000800000000000ull);
    }

    inline std::string read_hint(uintptr_t address)
    {
        if (!safe_pointer(address) || address > static_cast<uintptr_t>(0x0000800000000000ull - 64))
            return {};
        char buf[64]{};
        // Los nombres conocidos caben en 32 bytes; solo ampliar si hace falta.
        // Optional evita falsos errores DMA al desaparecer una unidad.
        if(!TargetProcess->ReadOptional(address,buf,32))return {};
        for(size_t i=0;i<32;++i)if(buf[i]=='\0')return std::string(buf,i);
        if(!TargetProcess->ReadOptional(address+32,buf+32,32))return {};
        for(size_t i=32;i<64;++i)if(buf[i]=='\0')return std::string(buf,i);
        return {}; // no confiar en nombres sin terminador
    }

    inline kind classify_unit(uintptr_t unit_base, uintptr_t image)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, entry_t> cache;
        static uintptr_t cached_image = 0;
        static auto next_gc = clock::now();
        if (cached_image != image) { cache.clear(); cached_image = image; }
        const auto now = clock::now();
        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end();)
                if (now - it->second.last_seen > std::chrono::seconds(10)) it = cache.erase(it);
                else ++it;
            next_gc = now + std::chrono::seconds(5);
        }
        if (!TargetProcess || !safe_pointer(unit_base)) return kind::unknown;
        auto& e = cache[unit_base];
        e.last_seen = now;
        if (now < e.next_refresh) return e.symbol;
        e.next_refresh = now + std::chrono::milliseconds(1500);
        e.symbol = kind::unknown;
        if (unit_base > static_cast<uintptr_t>(0x0000800000000000ull - 0x2000)) return e.symbol;
        const uintptr_t info = TargetProcess->Read<uintptr_t>(unit_base + offsets::unit_offsets::info_offset);
        if (!safe_pointer(info) || info > static_cast<uintptr_t>(0x0000800000000000ull - 0x100)) return e.symbol;
        // UnitInfo +0x30 subtipo localizable, +0x38 familia gruesa.
        const uintptr_t sub_ptr = TargetProcess->Read<uintptr_t>(info + 0x30);
        const uintptr_t type_ptr = TargetProcess->Read<uintptr_t>(info + offsets::unit_info_offsets::unit_type_offset);
        e.symbol = classify(read_hint(sub_ptr), read_hint(type_ptr));
        return e.symbol;
    }

    inline void draw(ImDrawList* list, ImVec2 p, kind symbol, ImU32 color)
    {
        constexpr ImU32 shadow = IM_COL32(10, 10, 10, 240);
        constexpr float r = 5.5f;
        auto at=[&](float dx,float dy){return ImVec2(p.x+dx,p.y+dy);};
        auto stroke=[&](float ax,float ay,float bx,float by,float thickness=1.6f){
            list->AddLine(at(ax,ay),at(bx,by),shadow,thickness+2.0f);
            list->AddLine(at(ax,ay),at(bx,by),color,thickness);
        };
        if(symbol==kind::fighter) {
            // Sharp delta wing / single fuselage.
            list->AddTriangleFilled(at(0,-8),at(-7,5),at(7,5),shadow);
            list->AddTriangleFilled(at(0,-6),at(-5,4),at(5,4),color);
            stroke(0,-7,0,7,1.7f);
            stroke(-2,5,2,5,1.2f);
            return;
        }
        if(symbol==kind::strike) {
            // Swept wings and prominent angled attack arrow.
            stroke(0,-8,0,8,2.3f);
            stroke(0,-2,-8,5,2.6f);stroke(0,-2,8,5,2.6f);
            stroke(0,5,-3,8,1.3f);stroke(0,5,3,8,1.3f);
            return;
        }
        if(symbol==kind::bomber) {
            // Broad straight wing, two visible engine pods.
            stroke(0,-8,0,8,3.0f);
            stroke(-9,-1,9,-1,3.1f);
            stroke(-4,-2,-4,3,2.2f);stroke(4,-2,4,3,2.2f);
            stroke(-4,6,4,6,1.7f);
            return;
        }
        if(symbol==kind::helicopter) {
            // Main rotor, angular cabin and rear rotor.
            stroke(-9,-5,9,-5,2.0f);
            stroke(0,-5,0,-2,1.5f);
            list->AddTriangleFilled(at(0,-2),at(-4,3),at(4,3),shadow);
            list->AddTriangleFilled(at(0,-1),at(-3,2),at(3,2),color);
            stroke(0,3,0,8,1.7f);stroke(-3,7,3,7,1.4f);
            return;
        }
        if(symbol==kind::unknown) {
            list->AddCircleFilled(p,4.5f,shadow,12);
            list->AddCircleFilled(p,3.5f,color,12);
            return;
        }
        if(symbol==kind::naval) {
            stroke(-8,2,8,2,2.2f); stroke(-6,2,-3,6); stroke(-3,6,5,6); stroke(5,6,8,2);
            stroke(-2,2,-2,-3); stroke(-2,-3,4,-3); stroke(4,-3,4,2);
            return;
        }
        if(symbol==kind::structure) {
            list->AddRectFilled(at(-6,-5),at(6,6),shadow);
            list->AddRect(at(-5,-4),at(5,5),color,0.0f,0,2.0f);
            stroke(-6,-5,0,-9); stroke(0,-9,6,-5);
            return;
        }
        // Halo oscuro y siluetas inequívocas también a escala pequeña.
        list->AddCircleFilled(p, 7.0f, shadow, 16);
        switch (symbol)
        {
        case kind::spaa:
            list->AddCircle(p, 4.5f, color, 16, 1.5f);
            list->AddLine(ImVec2(p.x - r, p.y), ImVec2(p.x + r, p.y), color, 1.4f);
            list->AddLine(ImVec2(p.x, p.y - r), ImVec2(p.x, p.y + r), color, 1.4f);
            break;
        case kind::tank_destroyer:
            list->AddTriangleFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y + r), ImVec2(p.x - r, p.y + r), color);
            list->AddLine(ImVec2(p.x, p.y - r + 1), ImVec2(p.x, p.y + 3), shadow, 1.5f);
            break;
        case kind::heavy:
            list->AddRectFilled(ImVec2(p.x - r, p.y - 4), ImVec2(p.x + r, p.y + 4), color);
            list->AddRect(ImVec2(p.x - r + 2, p.y - 2), ImVec2(p.x + r - 2, p.y + 2), shadow, 0.0f, 0, 1.2f);
            break;
        case kind::medium:
            list->AddQuadFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y), ImVec2(p.x, p.y + r), ImVec2(p.x - r, p.y), color);
            list->AddCircleFilled(p, 1.5f, shadow, 8);
            break;
        case kind::light:
            list->AddQuad(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y), ImVec2(p.x, p.y + r), ImVec2(p.x - r, p.y), color, 2.0f);
            break;
        default: break;
        }
    }
}
