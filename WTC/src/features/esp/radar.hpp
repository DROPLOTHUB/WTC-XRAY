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
    // 4 lectura en vivo (sin reutilizar vistas con el mapa cerrado).
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
        bool resolve(bool executed,uintptr_t address,T& out) const {
            if(executed && prepared && bytes_read==sizeof(T)) {out=value;return true;}
            T fresh{};
            if(!TargetProcess->ReadOptional(address,&fresh,sizeof(fresh)))return false;
            out=fresh;return true;
        }
    };

    inline map_view_t read_map_view()
    {
        map_view_t view{};
        const uintptr_t image = runtime::base_address();
        uint8_t drawn=0;
        if (!image || !TargetProcess ||
            !TargetProcess->ReadOptional(image + offsets::map_drawn_this_frame_offset,&drawn,sizeof(drawn)) || drawn != 1)
        { map_status.store(1,std::memory_order_relaxed);return view; } // Puerta de 1 byte: nunca reutilizar la geometría del frame anterior.

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport || !std::isfinite(viewport->Pos.x) || !std::isfinite(viewport->Pos.y) ||
            !std::isfinite(viewport->Size.x) || !std::isfinite(viewport->Size.y) ||
            viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f)
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
        vec2_t pos{},size{};
        if (!pos_field.resolve(map_executed,image+offsets::map_minimap_pos_offset,pos) ||
            !size_field.resolve(map_executed,image+offsets::map_minimap_size_offset,size) ||
            !valid_center(pos) || !valid_size(size))
        { map_status.store(2,std::memory_order_relaxed);return view; }

        // Rectángulo del juego en píxeles relativos al viewport; no escalar ni
        // recortarlo, ya que eso alteraría la transformación mundo→mapa.
        view.screen_min = ImVec2(viewport->Pos.x + pos.x, viewport->Pos.y + pos.y);
        view.screen_max = ImVec2(view.screen_min.x + size.x, view.screen_min.y + size.y);
        const ImVec2 viewport_max(viewport->Pos.x + viewport->Size.x,
            viewport->Pos.y + viewport->Size.y);
        if (!std::isfinite(view.screen_min.x) || !std::isfinite(view.screen_min.y) ||
            !std::isfinite(view.screen_max.x) || !std::isfinite(view.screen_max.y) ||
            !std::isfinite(viewport_max.x) || !std::isfinite(viewport_max.y) ||
            size.x < 8.0f || size.y < 8.0f ||
            view.screen_min.x < viewport->Pos.x || view.screen_min.y < viewport->Pos.y ||
            view.screen_max.x > viewport_max.x || view.screen_max.y > viewport_max.y)
        { map_status.store(2,std::memory_order_relaxed);return view; }

        vec2_t full_center{},full_size{},live_center{},live_size{};
        const bool full_ok=full_center_field.resolve(map_executed,image+offsets::map_full_world_center_offset,full_center) &&
            full_size_field.resolve(map_executed,image+offsets::map_full_world_size_offset,full_size);
        const bool live_ok=live_center_field.resolve(map_executed,image+offsets::map_world_center_offset,live_center) &&
            live_size_field.resolve(map_executed,image+offsets::map_view_size_offset,live_size);
        // Missing pan is not a zero pan; missing zoom is not a full-map mode.
        if(!live_ok) {map_status.store(3,std::memory_order_relaxed);return view;}
        geometry::window world{};
        if (valid_center(live_center) && valid_size(live_size))
            world = geometry::live(live_center.x,live_center.y,live_size.x,live_size.y);
        else if (full_ok && valid_center(full_center) && valid_size(full_size))
            world = geometry::full(full_center.x,full_center.y,full_size.x,full_size.y);
        else
        { map_status.store(3,std::memory_order_relaxed);return view; }

        // Bounds por modo del mapa; si el singleton/offset falla, conservar
        // el pan/zoom leído directamente, no reemplazarlo por el mapa completo.
        uintptr_t tactical=0;
        if (tactical_field.resolve(map_executed,image+offsets::tactical_map_obj_offset,tactical) && icons::safe_pointer(tactical) &&
            tactical < static_cast<uintptr_t>(0x0000800000000000ull - 0x9000))
        {
            uint32_t mode=0;
            if (TargetProcess->ReadOptional(tactical + offsets::tactical_map_offsets::mode,&mode,sizeof(mode)) && mode <= 2)
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
                vec2_t bounds_min{},bounds_max{};float aspect=0.f;
                if(min_field.resolve(bounds_executed,tactical+offsets::tactical_map_offsets::bounds_min+shift,bounds_min) &&
                    max_field.resolve(bounds_executed,tactical+offsets::tactical_map_offsets::bounds_max+shift,bounds_max) &&
                    aspect_field.resolve(bounds_executed,tactical+offsets::tactical_map_offsets::aspect,aspect))
                    geometry::clamp_to_mode(world,bounds_min.x,bounds_min.y,bounds_max.x,bounds_max.y,aspect);
            }
        }
        view.world_min = vec2_t{world.min_x,world.min_z};
        view.world_size = vec2_t{world.extent_x,world.extent_z};

        // Visibility can change while the geometry batch is in flight.
        drawn=0;
        if(!TargetProcess->ReadOptional(image+offsets::map_drawn_this_frame_offset,&drawn,sizeof(drawn)) || drawn!=1)
        {map_status.store(1,std::memory_order_relaxed);return {};}
        view.valid = world.valid();
        map_status.store(view.valid?4:3,std::memory_order_relaxed);
        return view;
    }

    // Snapshot positions are not a guarantee of current ownership/team. Read the
    // local identity and army explicitly; failed reads must not become army zero.
    inline uintptr_t local_unit() {
        const uintptr_t player=sdk::cLocalPlayer ? sdk::cLocalPlayer->get_base() : 0;
        if(!icons::safe_pointer(player))return 0;
        for(const auto offset : {offsets::localplayer::localunit_offset,
            offsets::localplayer::ownedUnitRef_offset,offsets::localplayer::controlledUnitRef_offset}) {
            uintptr_t unit=0;
            if(TargetProcess->ReadOptional(player+offset,&unit,sizeof(unit))) {
                unit &= ~uintptr_t{1};
                if(icons::safe_pointer(unit))return unit;
            }
        }
        return 0;
    }

    struct symbol_entry_t {
        uintptr_t info=0;
        icons::kind symbol=icons::kind::unknown;
        std::chrono::steady_clock::time_point refresh{};
    };
    inline std::unordered_map<uintptr_t,symbol_entry_t> symbol_cache;
    inline uintptr_t symbol_image=0,symbol_local=0;
    inline icons::kind classify_contact(uintptr_t unit,uintptr_t image,uintptr_t local) {
        using clock=std::chrono::steady_clock;
        if(symbol_image!=image || symbol_local!=local) {
            symbol_cache.clear();symbol_image=image;symbol_local=local;
        }
        if(!TargetProcess || !icons::safe_pointer(unit) ||
            unit>static_cast<uintptr_t>(0x0000800000000000ull-0x2000))return icons::kind::unknown;
        uintptr_t info=0;
        if(!TargetProcess->ReadOptional(unit+offsets::unit_offsets::info_offset,&info,sizeof(info)) ||
            !icons::safe_pointer(info) || info>static_cast<uintptr_t>(0x0000800000000000ull-0x100)) {
            symbol_cache.erase(unit);return icons::kind::unknown;
        }
        const auto now=clock::now();
        auto it=symbol_cache.find(unit);
        if(it!=symbol_cache.end() && it->second.info==info && now<it->second.refresh)return it->second.symbol;
        // Hard cap as well as refresh expiry: churn must not grow the cache.
        if(it==symbol_cache.end() && symbol_cache.size()>=512)symbol_cache.clear();
        uintptr_t subtype=0,type=0;
        const bool sub_ok=TargetProcess->ReadOptional(info+0x30,&subtype,sizeof(subtype));
        const bool type_ok=TargetProcess->ReadOptional(info+offsets::unit_info_offsets::unit_type_offset,&type,sizeof(type));
        const auto coarse=type_ok ? icons::read_hint(type) : std::string{};
        const auto family=icons::classify({},coarse);
        // A naval "Light Cruiser" is not a light tank: positive family wins.
        const auto symbol=(family==icons::kind::naval || family==icons::kind::structure) ? family :
            icons::classify(sub_ok ? icons::read_hint(subtype) : std::string{},coarse);
        symbol_cache[unit]={info,symbol,now+std::chrono::milliseconds(symbol==icons::kind::unknown ? 250 : 1500)};
        return symbol;
    }

    struct allow_all_contacts {bool operator()(uintptr_t,const vec3_t&,icons::kind)const{return true;}};
    template<class Allocator,class Visibility=allow_all_contacts>
    inline void draw(const std::unordered_map<uintptr_t,vec3_t,std::hash<uintptr_t>,
        std::equal_to<uintptr_t>,Allocator>& enemy_positions,Visibility contact_visible={})
    {
        rendered_contacts.store(0,std::memory_order_relaxed);
        unidentified_contacts.store(0,std::memory_order_relaxed);
        total_contacts.store(static_cast<int>(enemy_positions.size()),std::memory_order_relaxed);
        const uintptr_t image=runtime::base_address();
        if(!sdk::radar_enabled || !image) {map_status.store(0,std::memory_order_relaxed);return;}
        const map_view_t map=read_map_view();
        if(!map.valid)return; // No grace/TTL: zero gate and failed reads must hide immediately.
        if (enemy_positions.empty())
            return;

        ImDrawList* list = ImGui::GetForegroundDrawList();
        if (!list)
            return;

        const uintptr_t local=local_unit();
        uint8_t local_team=0;
        if(!local || !TargetProcess->ReadOptional(local+offsets::unit_offsets::unitArmyNo_offset,
            &local_team,sizeof(local_team)) || local_team==0 || local_team==0xFF)return;
        const ImU32 enemy_color = ImGui::ColorConvertFloat4ToU32(sdk::radar_enemy_color);
        const float width = map.screen_max.x - map.screen_min.x;
        const float height = map.screen_max.y - map.screen_min.y;
        list->PushClipRect(map.screen_min, map.screen_max, true);
        int visible=0,unknown=0;
        for (const auto& [unit_base, pos] : enemy_positions)
        {
            uint8_t team=0;
            if(unit_base==local || !icons::safe_pointer(unit_base) ||
                unit_base>static_cast<uintptr_t>(0x0000800000000000ull-0x2000) ||
                !TargetProcess->ReadOptional(unit_base+offsets::unit_offsets::unitArmyNo_offset,&team,sizeof(team)) ||
                team==0 || team==0xFF || team==local_team)continue;
            if (!std::isfinite(pos.x) || !std::isfinite(pos.z))
                continue;
            float x=0.f,y=0.f;
            if(!geometry::project({map.world_min.x,map.world_min.y,map.world_size.x,map.world_size.y},
                                  pos.x,pos.z,x,y))
                continue;

            const ImVec2 point(map.screen_min.x + width * x,
                map.screen_min.y + height * y);
            const icons::kind symbol = classify_contact(unit_base, image, local);
            if (!icons::enabled(symbol) || !contact_visible(unit_base,pos,symbol)) continue;
            if(symbol==icons::kind::unknown)++unknown;
            icons::draw(list, point, symbol, enemy_color);
            ++visible;
        }
        rendered_contacts.store(visible,std::memory_order_relaxed);
        unidentified_contacts.store(unknown,std::memory_order_relaxed);
        list->PopClipRect();
    }
}
