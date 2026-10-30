#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include "radar_icons.hpp"
#include "radar_geometry.hpp"

// Este encabezado se incluye desde esp.hpp después de los tipos del SDK y de ImGui.
namespace esp::radar
{
    // 0 apagado; 1 gate cerrado; 2 rectángulo inválido; 3 mundo inválido;
    // 4 lectura en vivo; 5 vista confirmada en ventana anti-parpadeo.
    inline std::atomic<int> map_status{0};
    inline std::atomic<int> total_contacts{0};
    inline std::atomic<int> rendered_contacts{0};
    inline std::atomic<int> unidentified_contacts{0};
    struct map_view_t
    {
        ImVec2 screen_min{};
        ImVec2 screen_max{};
        vec2_t world_min{ 0.0f, 0.0f }; // límite inferior/pan X/Z, NO centro geométrico
        vec2_t world_size{ 0.0f, 0.0f };
        bool valid = false;
    };

    inline bool valid_size(const vec2_t& size)
    {
        return std::isfinite(size.x) && std::isfinite(size.y) &&
            size.x > 0.001f && size.y > 0.001f;
    }

    inline bool valid_center(const vec2_t& center)
    {
        return std::isfinite(center.x) && std::isfinite(center.y);
    }

    template<class T> struct map_scatter_field_t {
        T value{}; DWORD bytes_read=0; bool prepared=false;
        void prepare(VMMDLL_SCATTER_HANDLE scatter,uintptr_t address) {
            prepared=TargetProcess->AddScatterReadRequest(scatter,address,
                &value,sizeof(value),&bytes_read);
        }
        T resolve(bool executed,uintptr_t address) const {
            return executed && prepared && bytes_read==sizeof(T)
                ? value : TargetProcess->Read<T>(address);
        }
    };

    inline map_view_t read_map_view()
    {
        map_view_t view{};
        const uintptr_t image = runtime::base_address();
        if (!image || !TargetProcess ||
            TargetProcess->Read<uint8_t>(image + offsets::map_drawn_this_frame_offset) != 1)
        { map_status.store(1,std::memory_order_relaxed);return view; } // Puerta de 1 byte: nunca reutilizar la geometría del frame anterior.

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f)
        { map_status.store(2,std::memory_order_relaxed);return view; }

        // El gate de un byte anterior sigue vigente en cada frame. Una sola
        // ejecución reúne los campos de mapa independientes sin usar TTL.
        map_scatter_field_t<vec2_t> pos_field,size_field,full_center_field,full_size_field;
        map_scatter_field_t<vec2_t> live_center_field,live_size_field;
        map_scatter_field_t<uintptr_t> tactical_field;
        auto map_scatter=TargetProcess->CreateScatterHandle();
        if(map_scatter) {
            pos_field.prepare(map_scatter,image+offsets::map_minimap_pos_offset);
            size_field.prepare(map_scatter,image+offsets::map_minimap_size_offset);
            full_center_field.prepare(map_scatter,image+offsets::map_full_world_center_offset);
            full_size_field.prepare(map_scatter,image+offsets::map_full_world_size_offset);
            live_center_field.prepare(map_scatter,image+offsets::map_world_center_offset);
            live_size_field.prepare(map_scatter,image+offsets::map_view_size_offset);
            tactical_field.prepare(map_scatter,image+offsets::tactical_map_obj_offset);
        }
        const bool map_executed=map_scatter &&
            TargetProcess->ExecuteReadScatter(map_scatter,0,false);
        if(map_scatter) TargetProcess->CloseScatterHandle(map_scatter);
        const vec2_t pos=pos_field.resolve(map_executed,image+offsets::map_minimap_pos_offset);
        const vec2_t size=size_field.resolve(map_executed,image+offsets::map_minimap_size_offset);
        if (!valid_center(pos) || !valid_size(size))
        { map_status.store(2,std::memory_order_relaxed);return view; }

        // Rectángulo del juego en píxeles relativos al viewport; no escalar ni
        // recortarlo, ya que eso alteraría la transformación mundo→mapa.
        view.screen_min = ImVec2(viewport->Pos.x + pos.x, viewport->Pos.y + pos.y);
        view.screen_max = ImVec2(view.screen_min.x + size.x, view.screen_min.y + size.y);
        const ImVec2 viewport_max(viewport->Pos.x + viewport->Size.x,
            viewport->Pos.y + viewport->Size.y);
        if (size.x < 8.0f || size.y < 8.0f ||
            view.screen_min.x < viewport->Pos.x || view.screen_min.y < viewport->Pos.y ||
            view.screen_max.x > viewport_max.x || view.screen_max.y > viewport_max.y)
        { map_status.store(2,std::memory_order_relaxed);return view; }

        const vec2_t full_center=full_center_field.resolve(map_executed,image+offsets::map_full_world_center_offset);
        const vec2_t full_size=full_size_field.resolve(map_executed,image+offsets::map_full_world_size_offset);
        const vec2_t live_center=live_center_field.resolve(map_executed,image+offsets::map_world_center_offset);
        const vec2_t live_size=live_size_field.resolve(map_executed,image+offsets::map_view_size_offset);
        geometry::window world{};
        if (valid_center(live_center) && valid_size(live_size))
            world = geometry::live(live_center.x,live_center.y,live_size.x,live_size.y);
        else if (valid_center(full_center) && valid_size(full_size))
            world = geometry::full(full_center.x,full_center.y,full_size.x,full_size.y);
        else
        { map_status.store(3,std::memory_order_relaxed);return view; }

        // Bounds por modo del mapa; si el singleton/offset falla, conservar
        // el pan/zoom leído directamente, no reemplazarlo por el mapa completo.
        const uintptr_t tactical=tactical_field.resolve(map_executed,image+offsets::tactical_map_obj_offset);
        if (icons::safe_pointer(tactical) &&
            tactical < static_cast<uintptr_t>(0x0000800000000000ull - 0x9000))
        {
            const uint32_t mode = TargetProcess->Read<uint32_t>(tactical + offsets::tactical_map_offsets::mode);
            if (mode <= 2)
            {
                const uintptr_t shift = static_cast<uintptr_t>(mode) * offsets::tactical_map_offsets::mode_stride;
                map_scatter_field_t<vec2_t> min_field,max_field;
                map_scatter_field_t<float> aspect_field;
                auto bounds_scatter=TargetProcess->CreateScatterHandle();
                if(bounds_scatter) {
                    min_field.prepare(bounds_scatter,tactical+offsets::tactical_map_offsets::bounds_min+shift);
                    max_field.prepare(bounds_scatter,tactical+offsets::tactical_map_offsets::bounds_max+shift);
                    aspect_field.prepare(bounds_scatter,tactical+offsets::tactical_map_offsets::aspect);
                }
                const bool bounds_executed=bounds_scatter &&
                    TargetProcess->ExecuteReadScatter(bounds_scatter,0,false);
                if(bounds_scatter) TargetProcess->CloseScatterHandle(bounds_scatter);
                const vec2_t bounds_min=min_field.resolve(bounds_executed,
                    tactical+offsets::tactical_map_offsets::bounds_min+shift);
                const vec2_t bounds_max=max_field.resolve(bounds_executed,
                    tactical+offsets::tactical_map_offsets::bounds_max+shift);
                const float aspect=aspect_field.resolve(bounds_executed,
                    tactical+offsets::tactical_map_offsets::aspect);
                geometry::clamp_to_mode(world,bounds_min.x,bounds_min.y,bounds_max.x,bounds_max.y,aspect);
            }
        }
        view.world_min = vec2_t{world.min_x,world.min_z};
        view.world_size = vec2_t{world.extent_x,world.extent_z};

        view.valid = world.valid();
        map_status.store(view.valid?4:3,std::memory_order_relaxed);
        return view;
    }

    template<class Allocator>
    inline void draw(const std::unordered_map<uintptr_t,vec3_t,std::hash<uintptr_t>,
        std::equal_to<uintptr_t>,Allocator>& enemy_positions)
    {
        // El latch del juego se pone a 1 y vuelve a 0 dentro del frame del
        // juego. El render del FUSER/DMA NO está sincronizado con ese frame:
        // leerlo una sola vez por frame del overlay provoca parpadeo.
        // Retener la ÚLTIMA vista confirmada durante una ventana breve evita
        // perder los frames intermedios, sin usar indefinidamente globals viejos.
        static map_view_t last_view{};
        static std::chrono::steady_clock::time_point last_seen{};
        static uintptr_t last_image = 0;
        const uintptr_t image = runtime::base_address();
        rendered_contacts.store(0,std::memory_order_relaxed);
        unidentified_contacts.store(0,std::memory_order_relaxed);
        total_contacts.store(static_cast<int>(enemy_positions.size()),std::memory_order_relaxed);
        if (!sdk::radar_enabled || !image || image != last_image)
        {
            last_view = {};
            last_seen = {};
            last_image = image;
            if (!sdk::radar_enabled || !image)
            { map_status.store(0,std::memory_order_relaxed);return; }
        }

        const auto now = std::chrono::steady_clock::now();
        map_view_t map = read_map_view(); // El latch se lee cada frame; nunca se cachea su valor.
        if (map.valid)
        {
            last_view = map;
            last_seen = now;
        }
        else
        {
            constexpr auto kMapGrace = std::chrono::milliseconds(250);
            if (!last_view.valid || now - last_seen > kMapGrace)
            {
                last_view = {};
                return;
            }
            map = last_view;
            map_status.store(5,std::memory_order_relaxed);
        }
        if (enemy_positions.empty())
            return;

        ImDrawList* list = ImGui::GetForegroundDrawList();
        if (!list)
            return;

        const ImU32 enemy_color = ImGui::ColorConvertFloat4ToU32(sdk::radar_enemy_color);
        const float width = map.screen_max.x - map.screen_min.x;
        const float height = map.screen_max.y - map.screen_min.y;
        list->PushClipRect(map.screen_min, map.screen_max, true);
        int visible=0,unknown=0;
        for (const auto& [unit_base, pos] : enemy_positions)
        {
            if (!std::isfinite(pos.x) || !std::isfinite(pos.z))
                continue;
            float x=0.f,y=0.f;
            if(!geometry::project({map.world_min.x,map.world_min.y,map.world_size.x,map.world_size.y},
                                  pos.x,pos.z,x,y))
                continue;

            const ImVec2 point(map.screen_min.x + width * x,
                map.screen_min.y + height * y);
            const icons::kind symbol = icons::classify_unit(unit_base, image);
            if (!icons::enabled(symbol)) continue;
            if(symbol==icons::kind::unknown)++unknown;
            icons::draw(list, point, symbol, enemy_color);
            ++visible;
        }
        rendered_contacts.store(visible,std::memory_order_relaxed);
        unidentified_contacts.store(unknown,std::memory_order_relaxed);
        list->PopClipRect();
    }
}
