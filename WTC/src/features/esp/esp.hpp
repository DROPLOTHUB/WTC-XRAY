#pragma once

#define NOMINMAX
#include <Windows.h>

#include <array>
#include <algorithm>
#include <limits>
#include <cmath>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <cstdio>
#include <vector>
#include <cstring>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#define ROW_MAJOR   1

#include "game/classes/movement.hpp"
#include "xray_bvh.hpp"
#include "esp_frame_policy.hpp"
#include "../../core/render_work_policy.hpp"
#include "../../core/adjacent_value_fields.hpp"
#include "unit_type_filter.hpp"
#include "reload_display_policy.hpp"
#include "invulnerability_display_policy.hpp"
#include "../../gui/menu_preferences.hpp"
#include "air_ghost_filter.hpp"
#include "radar.hpp"
#include "air_lead_prediction.hpp"
#include "air_lead_tracking.hpp"
#include "air_intercept.hpp"
#include "g2a_lead.hpp"
#include "coverage_opacity.hpp"
#include "lead_marker_geometry.hpp"

struct vec4_t {
    float x, y, z, w;
};

#ifndef VEC3D_T_DEFINED
#define VEC3D_T_DEFINED
struct vec3d_t {
    double x, y, z;
};
#endif

namespace esp
{
    // ========================================================================
    // [SECCIÓN 1] HELPERS DE DIBUJO
    // ========================================================================
    // A2A and G2A share every visual setting and drawing primitive.
    inline void draw_lead_marker(const vec2_t& center,const vec2_t* target=nullptr)
    {
        auto* draw=g_render->draw_list();
        if(!draw)return;
        const float size=sdk::air_lead_marker_size,thickness=sdk::air_lead_line_thickness;
        ImVec4 tint=sdk::air_lead_sim_color;
        const float coverage=std::clamp(tint.w,0.f,1.f)*std::clamp(sdk::air_lead_opacity,0.f,1.f);
        if(!std::isfinite(coverage)||coverage<=0)return;
        tint.w=coverage;
        const ImU32 color=ImGui::ColorConvertFloat4ToU32(tint);
        const bool safe_alpha=sdk::air_lead_coverage_opacity;
        const lead_marker_geometry::point target_point=target ? lead_marker_geometry::point{target->x,target->y} : lead_marker_geometry::point{};
        lead_marker_geometry::draw({center.x,center.y},size,thickness,target ? &target_point : nullptr,
            [&](float x,float y,float radius){
                if(safe_alpha)lead_opacity::circle(draw,ImVec2(x,y),radius,color,coverage);
                else g_render->circle(x,y,radius,color,16);
            },
            [&](float x1,float y1,float x2,float y2,float width){
                if(safe_alpha)lead_opacity::line(draw,ImVec2(x1,y1),ImVec2(x2,y2),color,width,coverage);
                else g_render->line(x1,y1,x2,y2,color,width);
            });
    }

    inline void draw_impact_x(const vec2_t& screen_pos, ImU32 color, float size = 8.0f, float thickness = 1.5f)
    {
        g_render->line(screen_pos.x - size, screen_pos.y - size, screen_pos.x + size, screen_pos.y + size, color, thickness);
        g_render->line(screen_pos.x + size, screen_pos.y - size, screen_pos.x - size, screen_pos.y + size, color, thickness);
    }

    // ========================================================================
    // [SECCIÓN 2] CONTEXTO DE PROYECCIÓN
    // ========================================================================
    struct projection_context_t
    {
        vec3_t camera_pos{};
        double bias_clip_x = 0.0;
        double bias_clip_y = 0.0;
        double bias_clip_w = 0.0;
        bool valid = false;
    };

    // ========================================================================
    // [SECCIÓN 3] CACHÉS DE UNIDAD
    // ========================================================================
    struct unit_cached_info_t
    {
        std::string unit_type;
        std::string vehicle_name;
        uint16_t raw_type = 0;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point next_name_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    struct unit_type_info_t
    {
        ImU32       color;
        bool        worth_to_draw;
        bool        is_dummy;
        bool        is_plane;
        bool        show_reload;
        bool        draw_box;
    };

    struct unit_box_cache_t
    {
        vec3_t bbmin{};
        vec3_t bbmax{};
        rotation_matrix_t rot{};
        std::chrono::steady_clock::time_point next_bounds_refresh{};
        std::chrono::steady_clock::time_point next_rot_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    struct unit_reload_cache_t
    {
        uint8_t value = 0;
        bool valid = false;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    // ========================================================================
    // [SECCIÓN 4] CLASIFICACIÓN DE UNIDADES
    // ========================================================================
    enum class UnitClass {
        Unknown,
        Air,
        Ground,
        Naval
    };

    inline UnitClass classify_unit_type(uint16_t raw_type) {
        switch (raw_type) {
        case 0: return UnitClass::Air;
        case 3: return UnitClass::Ground;
        case 5: return UnitClass::Naval;
        default: return UnitClass::Unknown;
        }
    }

    inline unit_type_info_t get_unit_type_info(std::string_view unit_type, uint16_t raw_type)
    {
        if (unit_type_filter::noncombatant(unit_type))
            return { IM_COL32(255, 255, 255, 255), false, true, false, false, false };
        UnitClass cls = classify_unit_type(raw_type);

        if (cls == UnitClass::Air) {
            if (unit_type == "exp_helicopter")       return { ImGui::ColorConvertFloat4ToU32(sdk::color_helicopter), sdk::show_helicopter, false, true, true, false };
            if (unit_type == "exp_bomber")           return { ImGui::ColorConvertFloat4ToU32(sdk::color_bomber), sdk::show_bomber, false, true, false, false };
            if (unit_type == "exp_fighter")          return { ImGui::ColorConvertFloat4ToU32(sdk::color_fighter), sdk::show_fighter, false, true, false, false };
            if (unit_type == "exp_assault")          return { ImGui::ColorConvertFloat4ToU32(sdk::color_attacker), sdk::show_attacker, false, true, false, false };
            if (unit_type == "exp_recon_drone")      return { ImGui::ColorConvertFloat4ToU32(sdk::color_recon_drone), sdk::show_recon_drone, false, true, false, false };
            return { ImGui::ColorConvertFloat4ToU32(sdk::color_fighter), true, false, true, false, false };
        }

        if (cls == UnitClass::Ground) {
            if (unit_type == "exp_tank")             return { ImGui::ColorConvertFloat4ToU32(sdk::color_light_tank), sdk::show_light_tank, false, false, true, true };
            if (unit_type == "exp_heavy_tank")       return { ImGui::ColorConvertFloat4ToU32(sdk::color_heavy_tank), sdk::show_heavy_tank, false, false, true, true };
            if (unit_type == "exp_tank_destroyer")   return { ImGui::ColorConvertFloat4ToU32(sdk::color_medium_tank), sdk::show_medium_tank, false, false, true, true };
            if (unit_type == "exp_SPAA")             return { ImGui::ColorConvertFloat4ToU32(sdk::color_spaa), sdk::show_spaa, false, false, true, true };
            return { ImGui::ColorConvertFloat4ToU32(sdk::color_light_tank), true, false, false, true, true };
        }

        if (cls == UnitClass::Naval) {
            if (unit_type == "exp_gun_boat")         return { ImGui::ColorConvertFloat4ToU32(sdk::color_gun_boat), sdk::show_gun_boat, false, false, false, false };
            if (unit_type == "exp_destroyer")        return { ImGui::ColorConvertFloat4ToU32(sdk::color_destroyer), sdk::show_destroyer, false, false, false, false };
            if (unit_type == "exp_cruiser")          return { ImGui::ColorConvertFloat4ToU32(sdk::color_cruiser), sdk::show_cruiser, false, false, false, false };
            if (unit_type == "exp_torpedo_gun_boat") return { ImGui::ColorConvertFloat4ToU32(sdk::color_torpedo_boat), sdk::show_torpedo_boat, false, false, false, false };
            return { IM_COL32(255, 255, 255, 255), false, false, false, false, false };
        }

        return { IM_COL32(255, 255, 255, 255), false, false, false, true, false };
    }

    inline bool is_air_lead_target(std::string_view unit_type, const unit_type_info_t& info)
    {
        if (!info.is_plane) return false;
        if (unit_type == "exp_helicopter")      return sdk::air_lead_target_helicopter;
        if (unit_type == "exp_bomber")          return sdk::air_lead_target_bomber;
        if (unit_type == "exp_fighter")         return sdk::air_lead_target_fighter;
        if (unit_type == "exp_assault")         return sdk::air_lead_target_attacker;
        if (unit_type == "exp_recon_drone")     return sdk::air_lead_target_recon_drone;
        return false;
    }

    // ========================================================================
    // [SECCIÓN 5] CACHÉS
    // ========================================================================
    inline unit_cached_info_t& get_cached_unit_info(const uintptr_t unit_base, c_unit& unit)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, unit_cached_info_t> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];

        const bool refresh_type=now>=entry.next_refresh;
        bool refresh_name=sdk::esp_show_vehicle_names && now>=entry.next_name_refresh;
        if(refresh_type || refresh_name) {
            auto info=unit.getInfo();
            if(refresh_type) {
                const auto type=info.getUnitType();
                if(type!=entry.unit_type) { entry.vehicle_name.clear(); entry.next_name_refresh={}; refresh_name=sdk::esp_show_vehicle_names; }
                entry.unit_type=type;
                entry.raw_type=TargetProcess->Read<uint16_t>(
                    unit_base+offsets::unit_offsets::unitType_offset);
                entry.next_refresh=now+std::chrono::milliseconds(1500);
            }
            if(refresh_name) {
                entry.vehicle_name=info.getVehicleName();
                entry.next_name_refresh=now+std::chrono::seconds(4);
            }
        }
        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(10))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }

    inline unit_box_cache_t& get_cached_box_info(const uintptr_t unit_base, c_unit& unit, const float distance_m)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, unit_box_cache_t> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];
        if (now >= entry.next_bounds_refresh)
        {
            entry.bbmin = unit.getBBMin();
            entry.bbmax = unit.getBBMax();
            entry.next_bounds_refresh = now + std::chrono::milliseconds(1000);
        }

        if (now >= entry.next_rot_refresh)
        {
            entry.rot = unit.getRotationMatrix();
            const auto rot_ttl = (distance_m <= 120.0f)
                ? std::chrono::milliseconds(16)
                : std::chrono::milliseconds(33);
            entry.next_rot_refresh = now + rot_ttl;
        }
        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(10))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }

    inline unit_reload_cache_t& get_cached_reload_info(const uintptr_t unit_base)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, unit_reload_cache_t> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];
        if (now >= entry.next_refresh)
        {
            uint8_t value=0;
            const auto address=unit_base+offsets::unit_offsets::visualReloadProgress_offset;
            entry.valid=TargetProcess->Read(address,&value,sizeof(value)); // misma ruta que Read<uint8_t> anterior
            if (entry.valid) entry.value=value;
            entry.next_refresh = now + std::chrono::milliseconds(200);
        }
        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(10))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }

    struct unit_invul_cache_t {
        bool flag_valid=false;
        bool invalid_flag=false;
        bool invulnerable=false;
        bool timer_valid=false;
        float timer=0.f;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };
    inline unit_invul_cache_t& get_cached_invulnerability(const uintptr_t unit_base) {
        using clock=std::chrono::steady_clock;
        static std::unordered_map<uintptr_t,unit_invul_cache_t> cache;
        static auto next_gc=clock::now()+std::chrono::seconds(5);
        const auto now=clock::now();
        auto& entry=cache[unit_base];
        if(now>=entry.next_refresh) {
            std::uint8_t raw=0;
            const bool read_ok=TargetProcess->ReadOptional(unit_base+
                offsets::unit_offsets::invulnerable_offset,&raw,sizeof(raw));
            entry.invalid_flag=read_ok && !invulnerability_display_policy::valid_flag(raw);
            entry.flag_valid=read_ok && !entry.invalid_flag;
            entry.invulnerable=entry.flag_valid && invulnerability_display_policy::invulnerable(raw);
            entry.timer_valid=false;
            if(entry.invulnerable && offsets::api_invul_timer_ready) {
                float timer=0.f;
                if(TargetProcess->ReadOptional(unit_base+
                    offsets::unit_offsets::invulTimer_offset,&timer,sizeof(timer)) &&
                    invulnerability_display_policy::displayable_timer(timer)) {
                    entry.timer=timer;
                    entry.timer_valid=true;
                }
            }
            entry.next_refresh=now+std::chrono::milliseconds(500);
        }
        entry.last_seen=now;
        if(now>=next_gc) {
            for(auto it=cache.begin();it!=cache.end();) {
                if(now-it->second.last_seen>std::chrono::seconds(10))it=cache.erase(it);
                else ++it;
            }
            next_gc=now+std::chrono::seconds(5);
        }
        return entry;
    }

    // ========================================================================
    // [SECCIÓN 6] GEOMETRÍA Y PROYECCIÓN
    // ========================================================================
    inline std::array<vec3_t, 8> calculate_bbox_corners_rotated(
        const vec3_t& position,
        const vec3_t& bbmin,
        const vec3_t& bbmax,
        const rotation_matrix_t& rot)
    {
        std::array<vec3_t, 8> local = { {
            { bbmin.x, bbmin.y, bbmin.z },
            { bbmax.x, bbmin.y, bbmin.z },
            { bbmin.x, bbmax.y, bbmin.z },
            { bbmax.x, bbmax.y, bbmin.z },
            { bbmin.x, bbmin.y, bbmax.z },
            { bbmax.x, bbmin.y, bbmax.z },
            { bbmin.x, bbmax.y, bbmax.z },
            { bbmax.x, bbmax.y, bbmax.z }
        } };

        const vec3_t center{
            (bbmin.x + bbmax.x) * 0.5f,
            (bbmin.y + bbmax.y) * 0.5f,
            (bbmin.z + bbmax.z) * 0.5f
        };

        std::array<vec3_t, 8> world{};

        for (size_t i = 0; i < 8; ++i)
        {
            const vec3_t offset{
                local[i].x - center.x,
                local[i].y - center.y,
                local[i].z - center.z
            };

            const vec3_t rotated = rot.transform(offset);

            world[i] = {
                position.x + center.x + rotated.x,
                position.y + center.y + rotated.y,
                position.z + center.z + rotated.z
            };
        }

        return world;
    }

    inline void draw_wireframe_box(
        const std::array<vec2_t, 8>& c,
        ImU32 color,
        float thickness)
    {
        g_render->line(c[0].x, c[0].y, c[1].x, c[1].y, color, thickness);
        g_render->line(c[1].x, c[1].y, c[3].x, c[3].y, color, thickness);
        g_render->line(c[3].x, c[3].y, c[2].x, c[2].y, color, thickness);
        g_render->line(c[2].x, c[2].y, c[0].x, c[0].y, color, thickness);

        g_render->line(c[4].x, c[4].y, c[5].x, c[5].y, color, thickness);
        g_render->line(c[5].x, c[5].y, c[7].x, c[7].y, color, thickness);
        g_render->line(c[7].x, c[7].y, c[6].x, c[6].y, color, thickness);
        g_render->line(c[6].x, c[6].y, c[4].x, c[4].y, color, thickness);

        g_render->line(c[0].x, c[0].y, c[4].x, c[4].y, color, thickness);
        g_render->line(c[1].x, c[1].y, c[5].x, c[5].y, color, thickness);
        g_render->line(c[2].x, c[2].y, c[6].x, c[6].y, color, thickness);
        g_render->line(c[3].x, c[3].y, c[7].x, c[7].y, color, thickness);
    }

    inline bool world_to_screen_counted(
        const vec3_t& point,
        vec2_t& out,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t&,
        uint32_t& w2s_counter)
    {
        ++w2s_counter;

        const ViewMatrix_t& mvp = camera_matrix;

        vec4_t clip;

#if defined(ROW_MAJOR)
        clip.x = point.x * mvp[0][0] + point.y * mvp[1][0] + point.z * mvp[2][0] + mvp[3][0];
        clip.y = point.x * mvp[0][1] + point.y * mvp[1][1] + point.z * mvp[2][1] + mvp[3][1];
        clip.z = point.x * mvp[0][2] + point.y * mvp[1][2] + point.z * mvp[2][2] + mvp[3][2];
        clip.w = point.x * mvp[0][3] + point.y * mvp[1][3] + point.z * mvp[2][3] + mvp[3][3];
#elif defined(COL_MAJOR)
        clip.x = mvp[0][0] * point.x + mvp[0][1] * point.y + mvp[0][2] * point.z + mvp[0][3];
        clip.y = mvp[1][0] * point.x + mvp[1][1] * point.y + mvp[1][2] * point.z + mvp[1][3];
        clip.z = mvp[2][0] * point.x + mvp[2][1] * point.y + mvp[2][2] * point.z + mvp[2][3];
        clip.w = mvp[3][0] * point.x + mvp[3][1] * point.y + mvp[3][2] * point.z + mvp[3][3];
#endif

        if (clip.w < 0.01f)
            return false;

        static int cached_frame = -1;
        static ImVec2 viewport_pos{};
        static ImVec2 viewport_half_size{};

        const int frame = ImGui::GetFrameCount();
        if (frame != cached_frame)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            if (!viewport)
                return false;

            viewport_pos = viewport->Pos;
            viewport_half_size = { viewport->Size.x * 0.5f, viewport->Size.y * 0.5f };
            cached_frame = frame;
        }

        if (viewport_half_size.x <= 0.0f || viewport_half_size.y <= 0.0f)
            return false;

        const float inv_w = 1.0f / clip.w;
        const float ndc_x = clip.x * inv_w;
        const float ndc_y = clip.y * inv_w;

        out.x = viewport_pos.x + (ndc_x + 1.0f) * viewport_half_size.x;
        out.y = viewport_pos.y + (1.0f - ndc_y) * viewport_half_size.y;

        return true;
    }

    inline bool is_point_in_clip_frustum(
        const vec3_t& point,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        const float margin = 0.2f)
    {
        const float legacy_clip_x =
            (point.x * camera_matrix[0][0]) +
            (point.y * camera_matrix[1][0]) +
            (point.z * camera_matrix[2][0]) +
            camera_matrix[3][0];
        const float legacy_clip_y =
            (point.x * camera_matrix[0][1]) +
            (point.y * camera_matrix[1][1]) +
            (point.z * camera_matrix[2][1]) +
            camera_matrix[3][1];
        const float legacy_clip_w =
            (point.x * camera_matrix[0][3]) +
            (point.y * camera_matrix[1][3]) +
            (point.z * camera_matrix[2][3]) +
            camera_matrix[3][3];

        const float legacy_limit = 1.0f + margin;
        const bool legacy_visible = legacy_clip_w > 0.001f &&
            std::fabs(legacy_clip_x / legacy_clip_w) <= legacy_limit &&
            std::fabs(legacy_clip_y / legacy_clip_w) <= legacy_limit;

        if (!projection_ctx.valid)
            return legacy_visible;

        const double local_x = static_cast<double>(point.x) - static_cast<double>(projection_ctx.camera_pos.x);
        const double local_y = static_cast<double>(point.y) - static_cast<double>(projection_ctx.camera_pos.y);
        const double local_z = static_cast<double>(point.z) - static_cast<double>(projection_ctx.camera_pos.z);

        const double clip_x =
            local_x * static_cast<double>(camera_matrix[0][0]) +
            local_y * static_cast<double>(camera_matrix[1][0]) +
            local_z * static_cast<double>(camera_matrix[2][0]) +
            projection_ctx.bias_clip_x;

        const double clip_y =
            local_x * static_cast<double>(camera_matrix[0][1]) +
            local_y * static_cast<double>(camera_matrix[1][1]) +
            local_z * static_cast<double>(camera_matrix[2][1]) +
            projection_ctx.bias_clip_y;

        const double clip_w =
            local_x * static_cast<double>(camera_matrix[0][3]) +
            local_y * static_cast<double>(camera_matrix[1][3]) +
            local_z * static_cast<double>(camera_matrix[2][3]) +
            projection_ctx.bias_clip_w;

        if (clip_w <= 0.001f)
            return legacy_visible;

        const double inv_w = 1.0 / clip_w;
        const double ndc_x = clip_x * inv_w;
        const double ndc_y = clip_y * inv_w;
        const double limit = 1.0 + static_cast<double>(margin);
        const bool precise_visible = std::fabs(ndc_x) <= limit && std::fabs(ndc_y) <= limit;
        return precise_visible || legacy_visible;
    }

    inline bool draw_cheap_2d_box(
        const vec3_t& unit_pos,
        const vec3_t& bbmin,
        const vec3_t& bbmax,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        uint32_t& w2s_counter,
        ImU32 color,
        float* out_max_y = nullptr)
    {
        vec2_t top_sp{};
        vec2_t bottom_sp{};
        vec2_t side_sp{};
        vec2_t center_sp{};

        const float center_y = (bbmin.y + bbmax.y) * 0.5f;
        const float half_width_world = std::max(std::fabs(bbmin.x), std::fabs(bbmax.x));

        const vec3_t top_world{ unit_pos.x, unit_pos.y + bbmax.y, unit_pos.z };
        const vec3_t bottom_world{ unit_pos.x, unit_pos.y + bbmin.y, unit_pos.z };
        const vec3_t side_world{ unit_pos.x + half_width_world, unit_pos.y + center_y, unit_pos.z };
        const vec3_t center_world{ unit_pos.x, unit_pos.y + center_y, unit_pos.z };

        if (!world_to_screen_counted(top_world, top_sp, camera_matrix, projection_ctx, w2s_counter) ||
            !world_to_screen_counted(bottom_world, bottom_sp, camera_matrix, projection_ctx, w2s_counter) ||
            !world_to_screen_counted(side_world, side_sp, camera_matrix, projection_ctx, w2s_counter) ||
            !world_to_screen_counted(center_world, center_sp, camera_matrix, projection_ctx, w2s_counter))
        {
            return false;
        }

        const float height = std::fabs(bottom_sp.y - top_sp.y);
        if (height < 2.0f)
            return false;

        float half_width = std::fabs(side_sp.x - center_sp.x);
        if (half_width < 1.0f)
            half_width = std::max(2.0f, height * 0.28f);

        const float min_x = center_sp.x - half_width;
        const float max_x = center_sp.x + half_width;
        const float min_y = std::min(top_sp.y, bottom_sp.y);
        const float max_y = std::max(top_sp.y, bottom_sp.y);

        g_render->rect(min_x, min_y, max_x - min_x, max_y - min_y, color, 0.0f);
        if (out_max_y)
            *out_max_y = max_y;
        return true;
    }

    struct projected_box_bounds_t
    {
        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        bool valid = false;
    };

    inline auto projected_bounds_from_screen_corners(const std::array<vec2_t,8>& corners)
        -> projected_box_bounds_t
    {
        projected_box_bounds_t bounds{};
        float min_x=corners[0].x,max_x=corners[0].x;
        float min_y=corners[0].y,max_y=corners[0].y;
        for (size_t i=1;i<corners.size();++i) {
            min_x=std::min(min_x,corners[i].x);max_x=std::max(max_x,corners[i].x);
            min_y=std::min(min_y,corners[i].y);max_y=std::max(max_y,corners[i].y);
        }
        if (max_x<=min_x || max_y<=min_y) return bounds;
        if (const auto* viewport=ImGui::GetMainViewport()) {
            min_x=std::clamp(min_x,viewport->Pos.x,viewport->Pos.x+viewport->Size.x);
            max_x=std::clamp(max_x,viewport->Pos.x,viewport->Pos.x+viewport->Size.x);
            min_y=std::clamp(min_y,viewport->Pos.y,viewport->Pos.y+viewport->Size.y);
            max_y=std::clamp(max_y,viewport->Pos.y,viewport->Pos.y+viewport->Size.y);
        }
        return {min_x,min_y,max_x,max_y,true};
    }

    inline auto get_projected_box_bounds(
        c_unit& unit,
        const vec3_t& unit_pos,
        const float distance_f,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        uint32_t& w2s_counter,
        uint32_t& obb_counter) -> projected_box_bounds_t
    {
        projected_box_bounds_t bounds{};
        auto& box_info = get_cached_box_info(unit.get_base(), unit, distance_f);
        ++obb_counter;
        const auto world_corners = calculate_bbox_corners_rotated(unit_pos, box_info.bbmin, box_info.bbmax, box_info.rot);

        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();

        bool any_visible = false;
        for (const auto& corner : world_corners)
        {
            vec2_t sp;
            if (!world_to_screen_counted(corner, sp, camera_matrix, projection_ctx, w2s_counter))
                continue;

            any_visible = true;

            min_x = std::min(min_x, sp.x);
            min_y = std::min(min_y, sp.y);
            max_x = std::max(max_x, sp.x);
            max_y = std::max(max_y, sp.y);
        }

        if (!any_visible || max_x <= min_x || max_y <= min_y)
            return bounds;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport)
        {
            const float view_min_x = viewport->Pos.x;
            const float view_min_y = viewport->Pos.y;
            const float view_max_x = viewport->Pos.x + viewport->Size.x;
            const float view_max_y = viewport->Pos.y + viewport->Size.y;

            min_x = std::clamp(min_x, view_min_x, view_max_x);
            max_x = std::clamp(max_x, view_min_x, view_max_x);
            min_y = std::clamp(min_y, view_min_y, view_max_y);
            max_y = std::clamp(max_y, view_min_y, view_max_y);
        }

        bounds.min_x = min_x;
        bounds.min_y = min_y;
        bounds.max_x = max_x;
        bounds.max_y = max_y;
        bounds.valid = true;
        return bounds;
    }

    inline bool draw_projected_2d_box_from_obb(
        c_unit& unit,
        const vec3_t& unit_pos,
        const float distance_f,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        uint32_t& w2s_counter,
        uint32_t& obb_counter,
        ImU32 color,
        float* out_max_y = nullptr,
        projected_box_bounds_t* out_bounds = nullptr)
    {
        const auto bounds = get_projected_box_bounds(unit, unit_pos, distance_f, camera_matrix, projection_ctx, w2s_counter, obb_counter);
        if (!bounds.valid)
            return false;

        g_render->rect(bounds.min_x, bounds.min_y, bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y, color, 0.0f);
        if (out_max_y)
            *out_max_y = bounds.max_y;
        if (out_bounds)
            *out_bounds = bounds;
        return true;
    }

    // ========================================================================
    // [SECCIÓN 7] X-RAY — PACKED BVH: TRANSFORM + AXIS CONVERSION
    // ========================================================================
    enum xform_layout_t
    {
        XF_RM = 0,
        XF_CM,
        XF_CM3,
        XF_RM3,
        XF_COUNT
    };

    struct part_xform_t
    {
        float          f[16] = {};
        xform_layout_t layout = XF_RM3;
        bool           valid = false;
    };

    struct xform_scheme_t
    {
        uintptr_t      stride = 0x30;
        xform_layout_t layout = XF_RM3;
        int            clean = -1;
        int            tested = 0;
    };

    inline const char* xform_layout_name(xform_layout_t l)
    {
        switch (l)
        {
        case XF_RM:  return "4x4-row";
        case XF_CM:  return "4x4-col";
        case XF_CM3: return "3x4-col(TMatrix)";
        case XF_RM3: return "3x4-row(TMatrix)";
        default:     return "?";
        }
    }

    inline float orthonorm_score(const vec3_t& b0, const vec3_t& b1, const vec3_t& b2)
    {
        return std::fabs(b0.length() - 1.0f) + std::fabs(b1.length() - 1.0f) + std::fabs(b2.length() - 1.0f)
            + std::fabs(b0.dot(b1)) + std::fabs(b1.dot(b2)) + std::fabs(b2.dot(b0));
    }

    inline void basis_from(const float* f, xform_layout_t layout, vec3_t& b0, vec3_t& b1, vec3_t& b2)
    {
        switch (layout)
        {
        case XF_RM:
            b0 = vec3_t(f[0], f[1], f[2]);
            b1 = vec3_t(f[4], f[5], f[6]);
            b2 = vec3_t(f[8], f[9], f[10]);
            break;
        case XF_CM:
            b0 = vec3_t(f[0], f[4], f[8]);
            b1 = vec3_t(f[1], f[5], f[9]);
            b2 = vec3_t(f[2], f[6], f[10]);
            break;
        case XF_CM3:
        case XF_RM3:
        default:
            b0 = vec3_t(f[0], f[1], f[2]);
            b1 = vec3_t(f[3], f[4], f[5]);
            b2 = vec3_t(f[6], f[7], f[8]);
            break;
        }
    }

    inline vec3_t apply_xform(const float* f, xform_layout_t layout, const vec3_t& v)
    {
        switch (layout)
        {
        case XF_RM:
            return vec3_t{
                v.x * f[0] + v.y * f[1] + v.z * f[2] + f[3],
                v.x * f[4] + v.y * f[5] + v.z * f[6] + f[7],
                v.x * f[8] + v.y * f[9] + v.z * f[10] + f[11] };
        case XF_CM:
            return vec3_t{
                v.x * f[0] + v.y * f[4] + v.z * f[8] + f[12],
                v.x * f[1] + v.y * f[5] + v.z * f[9] + f[13],
                v.x * f[2] + v.y * f[6] + v.z * f[10] + f[14] };
        case XF_CM3:
            return vec3_t{
                v.x * f[0] + v.y * f[3] + v.z * f[6] + f[9],
                v.x * f[1] + v.y * f[4] + v.z * f[7] + f[10],
                v.x * f[2] + v.y * f[5] + v.z * f[8] + f[11] };
        case XF_RM3:
        default:
            return vec3_t{
                v.x * f[0] + v.y * f[1] + v.z * f[2] + f[9],
                v.x * f[3] + v.y * f[4] + v.z * f[5] + f[10],
                v.x * f[6] + v.y * f[7] + v.z * f[8] + f[11] };
        }
    }

    inline vec3_t bvh_vertex_to_local_game(const vec3_t& v_bvh, const part_xform_t* xf)
    {
        float vx = v_bvh.x, vy = v_bvh.y, vz = v_bvh.z;

        switch (sdk::xray_axis_fix_mode)
        {
        case 0:  vx = -v_bvh.y; vy = v_bvh.x; vz = v_bvh.z; break;
        case 1:  vx = v_bvh.x; vy = v_bvh.z; vz = -v_bvh.y; break;
        case 2:  vx = v_bvh.y; vy = -v_bvh.x; vz = v_bvh.z; break;
        default: break;
        }

        if (xf && xf->valid)
            return apply_xform(xf->f, xf->layout, vec3_t{ vx, vy, vz });

        return vec3_t{ vx, vy, vz };
    }

    inline bool read_part_transform(uintptr_t matrix_array_base, uint16_t transform_index, part_xform_t& out)
    {
        out.valid = false;
        if (!matrix_array_base) return false;
        if (transform_index == 0xFFFFu || transform_index >= 4096u) return false;

        const uintptr_t addr = matrix_array_base + static_cast<uintptr_t>(transform_index) * 0x30;
        if (!TargetProcess->Read(addr, out.f, 12 * sizeof(float))) return false;

        out.layout = XF_CM3;
        out.valid = true;
        return true;
    }

    // ========================================================================
    // [SECCIÓN 7c] X-RAY — SKELETON TREE (animado)
    // ========================================================================
    struct unit_skeleton_ctx_t
    {
        uintptr_t dm = 0;
        uint8_t   valid_flag = 0;
        uintptr_t tree = 0;
        uintptr_t packed = 0;
        uint16_t  tree_count = 0;
        bool      animated_ok = false;
    };

    inline unit_skeleton_ctx_t read_unit_skeleton_ctx(uintptr_t unit_base, uintptr_t dm_root)
    {
        unit_skeleton_ctx_t ctx{};
        if (!unit_base) return ctx;

        // get_xray_cache acaba de leer esta raíz en el mismo render.
        ctx.dm = dm_root;
        if (!ctx.dm) return ctx;

        ctx.valid_flag = TargetProcess->Read<uint8_t>(
            ctx.dm + offsets::damage_model::m_ValidFlag);
        if (ctx.valid_flag != 1) return ctx;

        ctx.tree = TargetProcess->Read<uintptr_t>(
            unit_base + offsets::unit_offsets::modelTransform_offset);
        if (!ctx.tree) return ctx;

        ctx.packed = TargetProcess->Read<uintptr_t>(
            ctx.tree + offsets::damage_model::geom_node_tree_base);
        ctx.tree_count = TargetProcess->Read<uint16_t>(
            ctx.tree + offsets::damage_model::geom_node_tree_count);

        ctx.animated_ok = (ctx.packed != 0 && ctx.tree_count > 0);
        return ctx;
    }

    inline bool read_animated_matrix(const unit_skeleton_ctx_t& ctx,
        uint16_t tree_index,
        part_xform_t& out)
    {
        out.valid = false;
        if (!ctx.animated_ok) return false;
        if (tree_index == 0xFFFF) return false;
        if (tree_index >= ctx.tree_count) return false;

        const uintptr_t addr = ctx.packed + static_cast<uintptr_t>(tree_index) * 0x40;
        if (!TargetProcess->Read(addr, out.f, 16 * sizeof(float))) return false;

        out.layout = XF_CM;
        out.valid = true;

        return true;
    }

    // ========================================================================
    // [SECCIÓN 7d] CLASIFICACIÓN CACHEADA DE PARTS
    // ========================================================================
    using xray_part_kind_t = xray::xray_part_kind_t;

    // ========================================================================
    // [SECCIÓN 8] X-RAY — MALLA ORIGINAL Y CAJAS EXCLUSIVAS PARA AMMO
    // ========================================================================

    // Wireframe usa AddLine de ImGui: no hay lotes de malla rellena.
    inline size_t project_part_vertices_to(const xray::XrayPart& part,
        const part_xform_t* transform,const rotation_matrix_t& unit_rot,
        const vec3_t& unit_pos,const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,uint32_t& w2s_counter,
        std::vector<vec2_t>& projected,std::vector<uint8_t>& visible) {
        const size_t n=part.vertices.size();
        projected.resize(n);visible.assign(n,0);
        size_t count=0;
        for(size_t i=0;i<n;++i) {
            const vec3_t local=bvh_vertex_to_local_game(part.vertices[i],transform);
            const vec3_t world=unit_rot.transform(local)+unit_pos;
            if(world_to_screen_counted(world,projected[i],camera_matrix,proj_ctx,w2s_counter)) {
                visible[i]=1;++count;
            }
        }
        return count;
    }
    inline void draw_lines_from_projected(const std::vector<uint32_t>& indices,
        const std::vector<vec2_t>& projected,const std::vector<uint8_t>& visible,
        ImDrawList* draw_list,ImU32 color,float thickness) {
        if(!draw_list||projected.empty())return;
        const size_t n=projected.size();
        for(size_t t=0;t<indices.size()/3;++t) {
            const uint32_t a=indices[t*3],b=indices[t*3+1],c=indices[t*3+2];
            if(a>=n||b>=n||c>=n||!visible[a]||!visible[b]||!visible[c])continue;
            draw_list->AddLine(ImVec2(projected[a].x,projected[a].y),ImVec2(projected[b].x,projected[b].y),color,thickness);
            draw_list->AddLine(ImVec2(projected[b].x,projected[b].y),ImVec2(projected[c].x,projected[c].y),color,thickness);
            draw_list->AddLine(ImVec2(projected[c].x,projected[c].y),ImVec2(projected[a].x,projected[a].y),color,thickness);
        }
    }
    inline void draw_module_lines_world(const xray::XrayPart& part,
        const part_xform_t* transform,const rotation_matrix_t& unit_rot,
        const vec3_t& unit_pos,const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,uint32_t& w2s_counter,
        ImU32 color,float thickness) {
        ImDrawList* list=ImGui::GetBackgroundDrawList();
        if(!list)list=ImGui::GetForegroundDrawList();
        if(!list)return;
        thread_local std::vector<vec2_t> projected;
        thread_local std::vector<uint8_t> visible;
        project_part_vertices_to(part,transform,unit_rot,unit_pos,camera_matrix,
            proj_ctx,w2s_counter,projected,visible);
        draw_lines_from_projected(part.indices,projected,visible,list,color,thickness);
    }

    // Sólo ocho esquinas por módulo Ammo, no para el resto.
    inline bool project_xray_rect(
        const xray::XrayPart& part,
        const part_xform_t* part_transform,
        const rotation_matrix_t& unit_rot,
        const vec3_t& unit_pos,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,
        uint32_t& w2s_counter,
        ImVec2& out_min, ImVec2& out_max)
    {
        if(!part.local_bounds.valid)return false;
        float min_x=std::numeric_limits<float>::max(),min_y=min_x;
        float max_x=std::numeric_limits<float>::lowest(),max_y=max_x;
        int count=0;
        xray::boxes::each_corner(part.local_bounds,[&](float x,float y,float z){
            vec3_t vertex{};vertex.x=x;vertex.y=y;vertex.z=z;
            const vec3_t local=bvh_vertex_to_local_game(vertex,part_transform);
            const vec3_t world=unit_rot.transform(local)+unit_pos;
            vec2_t screen{};
            if(world_to_screen_counted(world,screen,camera_matrix,proj_ctx,w2s_counter) &&
               std::isfinite(screen.x) && std::isfinite(screen.y)) {
                min_x=std::min(min_x,screen.x);min_y=std::min(min_y,screen.y);
                max_x=std::max(max_x,screen.x);max_y=std::max(max_y,screen.y);
                ++count;
            }
        });
        if(count<2)return false;
        // Los elementos diminutos conservan un contorno visible sin reproyectar.
        const float pad_x=std::max(0.f,(3.f-(max_x-min_x))*0.5f);
        const float pad_y=std::max(0.f,(3.f-(max_y-min_y))*0.5f);
        out_min=ImVec2(min_x-pad_x,min_y-pad_y);
        out_max=ImVec2(max_x+pad_x,max_y+pad_y);
        return true;
    }

    // [SECCIÓN 8b] X-Ray por cajas: ningún triángulo de la malla se dibuja.
    inline void draw_xray_modules(
        c_unit& unit,
        const vec3_t& unit_pos,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,
        uint32_t& w2s_counter,
        float distance_f)
    {
        const uintptr_t unit_base = unit.get_base();
        auto& cache = xray::get_xray_cache(unit_base);
        if (cache.parts.empty())
            return;
        rotation_matrix_t unit_rot = unit.getRotationMatrix();

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        if (!draw_list) draw_list = ImGui::GetForegroundDrawList();
        if (!draw_list) return;

        // ====================================================================
        // CONTEXTO LEÍDO UNA SOLA VEZ POR UNIT/FRAME
        // ====================================================================
        const unit_skeleton_ctx_t skel = read_unit_skeleton_ctx(unit_base,cache.dm_root);

        // ====================================================================
        // VIEWPORT + MARGEN PARA CULLING 2D
        // ====================================================================
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float vp_w = vp ? vp->Size.x : 1920.0f;
        const float vp_h = vp ? vp->Size.y : 1080.0f;
        const float vp_margin = 200.0f;


        // Cache de transforms por índice dentro de la unit/frame.
        // - animated_cache por tree_index (skeleton tree)
        // - bind_cache por transform_index (m_TransformArray)
        // Referencias estables: unordered_map garantiza no invalidar
        // punteros a valores en inserts posteriores.
        thread_local std::unordered_map<uint16_t, part_xform_t> s_animated_cache;
        thread_local std::unordered_map<uint16_t, part_xform_t> s_bind_cache;
        s_animated_cache.clear();
        s_bind_cache.clear();

        for (const auto& part : cache.parts)
        {
            const xray_part_kind_t kind = part.kind;

            bool  is_small = false;
            ImU32 color = IM_COL32(255, 255, 255, 30);

            switch (kind)
            {
            case xray_part_kind_t::GunBarrel:
                if (!sdk::bShowModuleGunBarrel) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_gun_barrel);
                is_small = true;
                break;
            case xray_part_kind_t::CannonBreech:
                if (!sdk::bShowModuleCannonBreech) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_cannon_breech);
                is_small = true;
                break;
            case xray_part_kind_t::Crew:
                if (!sdk::bShowModuleCrew) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_crew);
                is_small = true;
                break;
            case xray_part_kind_t::Fuel:
                if (!sdk::bShowModuleFuelTank) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_fuel_tank);
                break;
            case xray_part_kind_t::Ammo:
                if (!sdk::bShowModuleAmmoRack) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_ammo_rack);
                break;
            case xray_part_kind_t::Engine:
                if (!sdk::bShowModuleEngine) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_engine);
                break;
            case xray_part_kind_t::Transmission:
                if (!sdk::bShowModuleTransmission) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_transmission);
                break;
            case xray_part_kind_t::Body:
            default:
                continue; // Body/partes no clasificadas: nunca proyectar ni dibujar.
            }

            if (is_small && distance_f > sdk::xray_small_module_distance)
                continue;

            if (xray::boxes::compact(kind) ? !part.local_bounds.valid :
                (part.vertices.empty() || part.indices.size() < 3))
                continue;

            // ---- Transform: skeleton tree primero, bind pose de fallback ----
            // Cacheado por índice dentro de este draw. Múltiples parts con el
            // mismo transform_index NO repiten lectura DMA.
            const part_xform_t* p_transform = nullptr;

            // animated
            if (skel.animated_ok && part.tree_index != 0xFFFF)
            {
                auto it = s_animated_cache.find(part.tree_index);
                if (it == s_animated_cache.end())
                {
                    part_xform_t tmp{};
                    if (read_animated_matrix(skel, part.tree_index, tmp))
                        it = s_animated_cache.emplace(part.tree_index, tmp).first;
                }
                if (it != s_animated_cache.end())
                    p_transform = &it->second;
            }

            // bind pose fallback
            if (!p_transform)
            {
                auto it = s_bind_cache.find(part.transform_index);
                if (it == s_bind_cache.end())
                {
                    part_xform_t tmp{};
                    if (read_part_transform(cache.matrix_array_base, part.transform_index, tmp))
                        it = s_bind_cache.emplace(part.transform_index, tmp).first;
                }
                if (it != s_bind_cache.end())
                    p_transform = &it->second;
            }

            // ---- Alpha ----
            if (sdk::xray_module_alpha < 1.0f)
            {
                int alpha = static_cast<int>((color >> 24) & 0xFF);
                alpha = static_cast<int>(static_cast<float>(alpha) * sdk::xray_module_alpha);
                alpha = std::clamp(alpha, 0, 255);
                color = (color & 0x00FFFFFF) | (alpha << 24);
            }

            if (sdk::xray_alpha_falloff)
            {
                const float distance_ratio = std::clamp(distance_f / sdk::xray_render_distance, 0.0f, 1.0f);
                int alpha = static_cast<int>((color >> 24) & 0xFF);
                alpha = static_cast<int>(static_cast<float>(alpha) * (1.0f - distance_ratio));
                alpha = std::clamp(alpha, 10, 255);
                color = (color & 0x00FFFFFF) | (alpha << 24);
            }

            if (xray::boxes::compact(kind)) {
                // Ammo únicamente: caja en pantalla desde ocho esquinas cacheadas.
                ImVec2 rect_min{},rect_max{};
                if(!project_xray_rect(part,p_transform,unit_rot,unit_pos,camera_matrix,
                                      proj_ctx,w2s_counter,rect_min,rect_max))
                    continue;
                const float vp_x=vp?vp->Pos.x:0.f, vp_y=vp?vp->Pos.y:0.f;
                if(rect_max.x<vp_x-vp_margin || rect_max.y<vp_y-vp_margin ||
                   rect_min.x>vp_x+vp_w+vp_margin || rect_min.y>vp_y+vp_h+vp_margin)
                    continue;
                // Recortar cajas parcialmente fuera de pantalla previene vértices
                // enormes cuando el módulo cruza el plano cercano de la cámara.
                const ImVec2 clipped_min(std::max(rect_min.x,vp_x-vp_margin),
                                         std::max(rect_min.y,vp_y-vp_margin));
                const ImVec2 clipped_max(std::min(rect_max.x,vp_x+vp_w+vp_margin),
                                         std::min(rect_max.y,vp_y+vp_h+vp_margin));
                draw_list->AddRect(clipped_min,clipped_max,color,0.f,0,
                                   std::max(.5f,sdk::xray_line_thickness));
    
                if(sdk::bShowModuleNames && distance_f<=sdk::xray_names_distance) {
                    g_render->text({(clipped_min.x+clipped_max.x)*.5f,
                                    (clipped_min.y+clipped_max.y)*.5f},
                        IM_COL32(255,255,255,255),0,part.name,
                        g_render->fonts().m_esp,sdk::esp_font_size_module);
                }
                continue; // ninguna triangulación para Ammo
            }

            // Todos los demás módulos conservan las líneas originales de malla.
            draw_module_lines_world(part,p_transform,unit_rot,unit_pos,
                camera_matrix,proj_ctx,w2s_counter,color,sdk::xray_line_thickness);
        }
    }

    // ========================================================================
    // [SECCIÓN 9] RUN PRINCIPAL
    // ========================================================================
    inline void run(const std::shared_ptr<const misc::frame_snapshot_t>& snapshot)
    {
        const auto esp_start = std::chrono::steady_clock::now();
        misc::prof_esp_radar_us.store(0,std::memory_order_relaxed);
        misc::prof_esp_units_us.store(0,std::memory_order_relaxed);
        misc::prof_esp_xray_us.store(0,std::memory_order_relaxed);
        misc::prof_esp_reload_us.store(0,std::memory_order_relaxed);
        misc::prof_esp_invul_us.store(0,std::memory_order_relaxed);
        uint64_t aimbot_us_total = 0;
        uint64_t aimbot_calls = 0;
        uint32_t w2s_calls = 0;
        uint32_t obb_calls = 0;
        uint64_t cull_distance=0, cull_type=0, cull_frustum=0, cull_projection=0, partial_boxes=0;
        misc::prof_esp_cull_distance.store(0,std::memory_order_relaxed);
        misc::prof_esp_cull_type.store(0,std::memory_order_relaxed);
        misc::prof_esp_cull_frustum.store(0,std::memory_order_relaxed);
        misc::prof_esp_cull_projection.store(0,std::memory_order_relaxed);
        misc::prof_esp_partial_boxes.store(0,std::memory_order_relaxed);
        misc::prof_reload_candidates.store(0,std::memory_order_relaxed);
        misc::prof_reload_drawn.store(0,std::memory_order_relaxed);
        misc::prof_reload_failed.store(0,std::memory_order_relaxed);
        misc::prof_reload_unexpected.store(0,std::memory_order_relaxed);
        misc::prof_reload_sample.store(-1,std::memory_order_relaxed);
        misc::prof_invul_candidates.store(0,std::memory_order_relaxed);
        misc::prof_invul_shown.store(0,std::memory_order_relaxed);
        misc::prof_invul_failed.store(0,std::memory_order_relaxed);
        misc::prof_invul_invalid.store(0,std::memory_order_relaxed);
        misc::prof_invul_timer_shown.store(0,std::memory_order_relaxed);

        struct air_filter_guard_t {
            bool ready=false;
            ~air_filter_guard_t(){if(!ready)air_ghost_filter::current.suspend();}
        } air_filter_guard;
        g2g_runtime::reset_diagnostics();
        sdk::ground_prediction_status = sdk::aimbot_enabled ? "Esperando snapshot valido" : "Desactivado";
        sdk::air_lead_status=sdk::air_lead_enabled ? "Esperando objetivo A2A" : "A2A desactivado";
        g2a_lead::status=g2a_lead::enabled ? "G2A esperando snapshot valido / ESP" : "G2A desactivado";
        g2a_lead::reset_diagnostics();
        struct g2a_cache_t {
            uintptr_t owner=0,target=0,container=0,own_container=0;uint64_t epoch=0;
            std::chrono::steady_clock::time_point next{},sample{},latest{};
            air_intercept::solution solution{};
            ballistic_validation::round_data ammunition{};
            g2a_lead::motion_track own{},enemy{};
            g2a_lead::motion_sample own_sample{},enemy_sample{};
        };
        static g2a_cache_t g2a_cache;
        // Early returns (disabled ESP, bad snapshot/lifecycle) must not retain history.
        struct g2a_guard_t {
            g2a_cache_t& cache;bool reached=false;
            ~g2a_guard_t(){if(!reached)cache={};}
        } g2a_guard{g2a_cache};
        sdk::ground_prediction_screen_known = false;
        if (!sdk::esp_enabled) {
            sdk::ground_prediction_status = "ESP desactivado: G2G depende del render ESP";
            sdk::air_lead_status="ESP desactivado: A2A depende del render ESP";
            return;
        }

        if (!snapshot)
            return;

        const auto& curr_snapshot = *snapshot;
        const auto& frame = curr_snapshot.frame;
        if (!frame.valid)
            return;

        const ViewMatrix_t camera_matrix = frame.camera_matrix;
        const vec3_t camera_pos = frame.camera_pos;

        projection_context_t projection_ctx{};
        projection_ctx.camera_pos = camera_pos;
        projection_ctx.bias_clip_x =
            static_cast<double>(camera_pos.x) * static_cast<double>(camera_matrix[0][0]) +
            static_cast<double>(camera_pos.y) * static_cast<double>(camera_matrix[1][0]) +
            static_cast<double>(camera_pos.z) * static_cast<double>(camera_matrix[2][0]) +
            static_cast<double>(camera_matrix[3][0]);
        projection_ctx.bias_clip_y =
            static_cast<double>(camera_pos.x) * static_cast<double>(camera_matrix[0][1]) +
            static_cast<double>(camera_pos.y) * static_cast<double>(camera_matrix[1][1]) +
            static_cast<double>(camera_pos.z) * static_cast<double>(camera_matrix[2][1]) +
            static_cast<double>(camera_matrix[3][1]);
        projection_ctx.bias_clip_w =
            static_cast<double>(camera_pos.x) * static_cast<double>(camera_matrix[0][3]) +
            static_cast<double>(camera_pos.y) * static_cast<double>(camera_matrix[1][3]) +
            static_cast<double>(camera_pos.z) * static_cast<double>(camera_matrix[2][3]) +
            static_cast<double>(camera_matrix[3][3]);
        projection_ctx.valid = false;

        if (frame.gui_state != GuiState::ALIVE && frame.gui_state != GuiState::SPEC)
            return;

        const auto current_epoch=misc::unit_identity_epoch.load(std::memory_order_acquire);
        const double air_filter_sample_time=std::chrono::duration<double>(curr_snapshot.captured_at.time_since_epoch()).count();
        const double air_filter_age=std::chrono::duration<double>(std::chrono::steady_clock::now()-curr_snapshot.captured_at).count();
        air_ghost_filter::current.begin(current_epoch,menu_preferences::current.filter_static_air &&
            std::isfinite(air_filter_age) && air_filter_age>=0 && air_filter_age<=.25);
        air_filter_guard.ready=true;

        const float esp_render_distance = std::clamp(sdk::esp_render_distance, 1000.0f, 55000.0f);
        const float esp_render_distance_sq = esp_render_distance * esp_render_distance;
        const float esp_box_distance = std::clamp(sdk::esp_box_distance, 500.0f, 3500.0f);
        const float esp_box_distance_sq = esp_box_distance * esp_box_distance;
        constexpr float far_simplified_distance = 10000.0f;
        constexpr float far_simplified_distance_sq = far_simplified_distance * far_simplified_distance;
        ImFont* const esp_font = g_render->fonts().m_esp;

        if (frame.local_is_plane && sdk::esp_show_bomb_impact && !frame.bomb_impact.empty())
        {
            vec2_t sp;
            if (g_render->world_to_screen(frame.bomb_impact, sp, camera_matrix))
            {
                g_render->circle(sp.x, sp.y, 6.0f, IM_COL32(255, 0, 200, 255), 16);
                draw_impact_x(sp, IM_COL32(255, 255, 255, 255));
            }
        }

        const auto radar_start=std::chrono::steady_clock::now();
        radar::draw(curr_snapshot.positions,[&](uintptr_t base,const vec3_t& position,radar::icons::kind symbol){
            const bool aerial=symbol==radar::icons::kind::fighter || symbol==radar::icons::kind::strike ||
                symbol==radar::icons::kind::bomber || symbol==radar::icons::kind::helicopter;
            return air_ghost_filter::current.visible(base,aerial,{position.x,position.y,position.z},air_filter_sample_time);
        });
        misc::prof_esp_radar_us.store(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-radar_start).count()),std::memory_order_relaxed);

        thread_local std::vector<c_unit> units;
        thread_local uint64_t seen_units_revision=UINT64_MAX;
        misc::snapshot_units_into(units,seen_units_revision);
        const vec3_t& local_pos = frame.local_pos;

        int32_t local_team = -1;
        uintptr_t local_unit_base = frame.local_unit_base;
        render_work_policy::frame_sample<rotation_matrix_t> local_rotation;
        if (!local_unit_base) {
            // Spectate/fallback: el snapshot no siempre contiene la unidad controlada.
            const uintptr_t local_player = sdk::cLocalPlayer ? sdk::cLocalPlayer->get_base() : 0;
            if (local_player) {
                uintptr_t candidate = TargetProcess->Read<uintptr_t>(
                    local_player + offsets::localplayer::localunit_offset);
                if (!candidate) candidate = TargetProcess->Read<uintptr_t>(
                    local_player + offsets::localplayer::ownedUnitRef_offset);
                if (!candidate) candidate = TargetProcess->Read<uintptr_t>(
                    local_player + offsets::localplayer::controlledUnitRef_offset);
                candidate &= ~uintptr_t{1};
                if (startup_pointer_probe::plausible_pointer(candidate))
                    local_unit_base = candidate;
            }
        }
        if (local_unit_base) {
            using clock=std::chrono::steady_clock;
            static uintptr_t team_owner=0;
            static int32_t cached_team=-1;
            static uint64_t team_epoch=0;
            static clock::time_point next_team_read{};
            const auto now=clock::now();
            const auto epoch=misc::unit_identity_epoch.load(std::memory_order_acquire);
            if (frame_policy::refresh_local_team(local_unit_base,team_owner,epoch,
                team_epoch,now,next_team_read)) {
                uint8_t team_byte=0;
                cached_team=TargetProcess->Read(local_unit_base+
                    offsets::unit_offsets::unitArmyNo_offset,&team_byte,sizeof(team_byte))
                    ? static_cast<int32_t>(team_byte) : -1;
                team_owner=local_unit_base;
                team_epoch=epoch;
                next_team_read=now+std::chrono::milliseconds(250);
            }
            local_team=cached_team;
            // Rotation is sampled lazily below only for an actual A2A solve.
        }

        // Positive ground classification: non-air alone also includes naval/unknown.
        uint16_t g2a_local_type=0xFFFF;
        const bool process_g2a=g2a_lead::enabled && !frame.local_is_plane &&
            frame.gui_state==GuiState::ALIVE && local_unit_base==frame.local_unit_base && local_unit_base &&
            TargetProcess->Read(local_unit_base+offsets::unit_offsets::unitType_offset,
                &g2a_local_type,sizeof(g2a_local_type)) && classify_unit_type(g2a_local_type)==UnitClass::Ground;
        if(g2a_lead::enabled) g2a_lead::status=process_g2a ?
            "G2A sin objetivo aereo enemigo visible" : "G2A requiere unidad terrestre local activa";
        uintptr_t best_g2a_base=0;
        vec3_t best_g2a_pos{};
        float best_g2a_score=std::numeric_limits<float>::max();
        constexpr float ballistic_render_distance = 3500.0f;
        constexpr float ballistic_render_distance_sq = ballistic_render_distance * ballistic_render_distance;
        if (frame_policy::needs_global_ballistics(frame.local_is_plane,
            sdk::aimbot_enabled,sdk::air_lead_enabled,sdk::air_lead_use_simulation) || process_g2a)
            aimbot::BeginFrame(frame.local_is_plane,local_unit_base,
                misc::unit_identity_epoch.load(std::memory_order_acquire));

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        const ImVec2 viewport_center = main_viewport
            ? ImVec2(main_viewport->Pos.x + main_viewport->Size.x * 0.5f, main_viewport->Pos.y + main_viewport->Size.y * 0.5f)
            : ImVec2(0.0f, 0.0f);
        const float aim_fov_radius = main_viewport
            ? (std::min(main_viewport->Size.x, main_viewport->Size.y) * 0.50f)
            : 0.0f;
        const float aim_fov_radius_sq = aim_fov_radius * aim_fov_radius;


        c_unit best_aimbot_unit{};
        vec3_t best_aimbot_pos{};
        float best_aimbot_score = std::numeric_limits<float>::max();
        bool has_best_aimbot_target = false;
        struct reload_candidate_t {uintptr_t base;vec2_t screen;float distance_sq;c_unit unit;vec3_t position;float distance_m;projected_box_bounds_t cached_box;};
        static thread_local std::vector<reload_candidate_t> reload_candidates;
        reload_candidates.clear();
        if (sdk::esp_show_reload && reload_candidates.capacity()<32) reload_candidates.reserve(32);
        struct invul_candidate_t {uintptr_t base;vec2_t screen;float distance_sq;};
        static thread_local std::vector<invul_candidate_t> invul_candidates;
        invul_candidates.clear();
        if (sdk::esp_show_invulnerable && offsets::api_invulnerable_ready &&
            invul_candidates.capacity()<invulnerability_display_policy::max_indicators)
            invul_candidates.reserve(invulnerability_display_policy::max_indicators);

        c_unit best_xray_unit{};
        vec3_t best_xray_pos{};
        float best_xray_distance_f = 0.0f;
        float best_xray_score = std::numeric_limits<float>::max();
        bool has_xray_target = false;

        struct TeamCacheEntry {
            uint8_t team = 0xFF;
            std::chrono::steady_clock::time_point next_update{};
        };
        struct AirContainerCacheEntry {
            uintptr_t container = 0;
            std::chrono::steady_clock::time_point next_update{};
        };
        static std::unordered_map<uintptr_t, TeamCacheEntry> team_cache;
        static uint64_t team_cache_epoch=0;

        if(team_cache_epoch!=current_epoch) {team_cache.clear();team_cache_epoch=current_epoch;}
        static std::unordered_map<uintptr_t, AirContainerCacheEntry> air_container_cache;
        static uint64_t air_container_epoch=0;
        if (air_container_epoch!=current_epoch) {
            air_container_cache.clear();air_container_epoch=current_epoch;
        }

        auto get_unit_team = [&](uintptr_t unit_base) -> uint8_t {
            const auto now = std::chrono::steady_clock::now();
            auto& entry = team_cache[unit_base];
            if (now >= entry.next_update) {
                uint8_t team=0xFF;
                entry.team=TargetProcess->Read(unit_base+offsets::unit_offsets::unitArmyNo_offset,
                    &team,sizeof(team)) ? team : 0xFF;
                entry.next_update = now + std::chrono::milliseconds(500);
            }
            return entry.team;
            };

        auto get_air_container = [&](uintptr_t unit_base, const std::chrono::steady_clock::time_point& now) -> uintptr_t {
            if(!offsets::api_air_velocity_ready)return 0;
            auto& entry = air_container_cache[unit_base];
            if (now >= entry.next_update) {
                entry.container = TargetProcess->Read<uintptr_t>(unit_base + offsets::unit_offsets::airmovement_offset);
                entry.next_update = now + std::chrono::milliseconds(200);
            }
            return entry.container;
            };

        c_unit best_air_lead_unit{};
        vec3_t best_air_lead_pos{};
        float best_air_lead_score = std::numeric_limits<float>::max();
        bool has_best_air_lead_target = false;

        struct AirLeadCacheEntry {
            vec3_t raw_lead_point{};
            vec3_t smoothed_lead_point{};
            air_lead_tracking::motion_state motion{};
            vec3_t prediction_velocity{};
            vec3_t prediction_acceleration{};
            uintptr_t last_container = 0;
            bool initialized = false;
            std::chrono::steady_clock::time_point last_sample{},latest{};
            std::chrono::steady_clock::time_point last_update{};
            std::chrono::steady_clock::time_point next_sample{};
            std::chrono::steady_clock::time_point last_smooth{};
            vec3_t solve_origin{};
            uintptr_t shooter_base=0;uint64_t shooter_generation=0;
            air_lead_model::TurnState turn_state{};
            float turn_omega = 0.0f;
            float flight_time=0.f,model_error=0.f;
            air_lead_tracking::source own_source=air_lead_tracking::source::none,target_source=air_lead_tracking::source::none;
        };
        static std::unordered_map<uintptr_t, AirLeadCacheEntry> air_lead_cache;
        static uint64_t air_lead_epoch=0;
        if (air_lead_epoch!=current_epoch) {air_lead_cache.clear();air_lead_epoch=current_epoch;}
        const auto& ammo=aimbot::ballisticsData;
        const air_lead_tracking::solve_context context{ammo.velocity,ammo.mass,ammo.caliber,ammo.drag,sdk::air_lead_extra_time,
            sdk::air_lead_weapon_selection,aimbot::aircraft_weapon_trace.selected,ammo.caliber_in_mm,sdk::air_lead_use_heaviest_weapon,
            sdk::air_lead_use_weapon_position,sdk::air_lead_use_turn_prediction,aimbot::aircraft_global_fallback,
            offsets::api_air_velocity_ready,offsets::api_air_acceleration_ready};
        static air_lead_tracking::solve_context previous_context{};
        if(!(previous_context==context)){air_lead_cache.clear();previous_context=context;}
        struct native_motion_t {vec3_t velocity{},acceleration{};bool have_velocity=false,have_acceleration=false;};
        const auto read_native_motion=[&](uintptr_t container) {
            native_motion_t sample;
            if(!container || !offsets::api_air_velocity_ready)return sample;
            vec3d_t v{},a{};bool velocity_ok=false,acceleration_ok=false;
            if(offsets::api_air_acceleration_ready) {
                const auto pair=adjacent_value_fields::read_pair<vec3d_t>(container,
                    offsets::unit_offsets::airmovement_offsets::velocity_offset,
                    offsets::unit_offsets::airmovement_offsets::acceleration_offset,
                    [](uintptr_t address,void* data,size_t size){return TargetProcess->ReadOptional(address,data,size);});
                v=pair.first;a=pair.second;velocity_ok=pair.first_valid;acceleration_ok=pair.second_valid;
            } else velocity_ok=TargetProcess->ReadOptional(container+
                offsets::unit_offsets::airmovement_offsets::velocity_offset,&v,sizeof(v));
            sample.velocity={float(v.x),float(v.y),float(v.z)};
            sample.acceleration={float(a.x),float(a.y),float(a.z)};
            sample.have_velocity=velocity_ok && air_lead_tracking::bounded(sample.velocity,950.f);
            sample.have_acceleration=acceleration_ok && air_lead_tracking::bounded(sample.acceleration,160.f);
            return sample;
        };
        struct shooter_motion_t {
            uintptr_t owner=0,container=0;uint64_t epoch=0,generation=0;
            air_lead_tracking::motion_state history{};air_lead_tracking::estimate sample{};
            air_lead_model::TurnState turn{};float omega=0;
            std::chrono::steady_clock::time_point next{},sampled_at{};
        };
        static shooter_motion_t shooter;
        const auto sample_shooter=[&](const std::chrono::steady_clock::time_point now) {
            if(shooter.owner!=local_unit_base || shooter.epoch!=current_epoch){shooter={};shooter.owner=local_unit_base;shooter.epoch=current_epoch;}
            const double interval=std::chrono::duration<double>(curr_snapshot.captured_at-shooter.sampled_at).count();
            if(now>=shooter.next && interval>=.025) {
                if(interval>.2){shooter.history={};shooter.turn={};}
                shooter.sampled_at=curr_snapshot.captured_at;
                shooter.next=now+std::chrono::milliseconds(30);
                const auto container=get_air_container(local_unit_base,now);
                if(shooter.container!=container){shooter.history={};shooter.turn={};}
                shooter.container=container;
                const auto native=read_native_motion(container);
                const double timestamp=std::chrono::duration<double>(curr_snapshot.captured_at.time_since_epoch()).count();
                shooter.sample=air_lead_tracking::observe(shooter.history,frame.local_pos,timestamp,
                    native.have_velocity?&native.velocity:nullptr,native.have_acceleration?&native.acceleration:nullptr);
                if(shooter.sample.reset){shooter.turn={};++shooter.generation;}
                shooter.omega=shooter.sample.valid ? air_lead_model::update_turn_state(shooter.sample.velocity,
                    shooter.sample.acceleration,shooter.sample.dt,local_unit_base,&shooter.turn) : 0.f;
            }
        };
        bool drew_air_lead=false;
        sdk::air_lead_own_motion_status="Sin muestra dibujada";sdk::air_lead_target_motion_status="Sin muestra dibujada";
        sdk::air_lead_flight_time=0.f;sdk::air_lead_model_error=0.f;
        if(sdk::air_lead_enabled && !sdk::air_lead_use_simulation)
            sdk::air_lead_status="Use Simulation desactivado";
        else if(sdk::air_lead_enabled && !frame.local_is_plane)
            sdk::air_lead_status="A2A requiere avion local";
        else if(sdk::air_lead_enabled)
            sdk::air_lead_status="Sin objetivo aereo enemigo visible";

        auto run_air_lead = [&](c_unit& unit, const vec3_t& unit_pos) {
            const uintptr_t unit_base = unit.get_base();
            const auto now = std::chrono::steady_clock::now();
            auto& cache = air_lead_cache[unit_base];
            if(cache.shooter_base!=local_unit_base) {
                cache=AirLeadCacheEntry{};cache.shooter_base=local_unit_base;
            }
            // Snapshot high-water mark is independent of the last solved sample.
            const float snapshot_age=std::chrono::duration<float>(now-curr_snapshot.captured_at).count();
            if(!std::isfinite(snapshot_age) || snapshot_age<0 || snapshot_age>.2f ||
                curr_snapshot.captured_at<cache.latest) {
                cache.initialized=false;cache.motion={};cache.turn_state={};
                if(!drew_air_lead)sdk::air_lead_status="Snapshot A2A fuera de tiempo";
                return;
            }
            cache.latest=curr_snapshot.captured_at;
            const double interval=std::chrono::duration<double>(curr_snapshot.captured_at-cache.last_sample).count();
            if(interval>.2) {
                cache.initialized=false;cache.motion={};cache.turn_state={};cache.next_sample={};
            }
            const float held_age=std::chrono::duration<float>(now-cache.last_sample).count();
            if(!std::isfinite(held_age) || held_age<0 || held_age>.2f)cache.initialized=false;
            // DMA + filtering + solve share a cadence; rendering stays per-frame.
            // Duplicates cannot refresh motion or renew an old solution.
            // Failed solutions are throttled too, rather than retried at render FPS.
            if (now >= cache.next_sample && interval>=.025) {
                cache.last_sample=curr_snapshot.captured_at;
                cache.next_sample = now + std::chrono::milliseconds(30);
                vec3_t air_velocity{ 0.0f, 0.0f, 0.0f };
                vec3_t air_acceleration{ 0.0f, 0.0f, 0.0f };

                // Launch from the aircraft, not a third-person camera behind it.
                vec3_t origin = frame.local_pos;
                sample_shooter(now);
                if(cache.shooter_generation!=shooter.generation){cache.initialized=false;cache.shooter_generation=shooter.generation;}
                if (sdk::air_lead_use_weapon_position && local_unit_base) {
                    // Weapon-local geometry is shared across all air targets.
                    static uintptr_t weapon_owner = 0;
                    static uint64_t weapon_epoch=0;
                    static int weapon_selection=-2;
                    static vec3_t weapon_local{};
                    static bool weapon_valid = false;
                    static std::chrono::steady_clock::time_point weapon_next{};
                    if (weapon_owner != local_unit_base || weapon_epoch!=current_epoch ||
                        weapon_selection!=aimbot::aircraft_weapon_trace.selected || now >= weapon_next) {
                        weapon_owner = local_unit_base;weapon_epoch=current_epoch;
                        weapon_selection=aimbot::aircraft_weapon_trace.selected;
                        // The position array is only known for the first gun: no invented index mapping.
                        weapon_valid = !aimbot::aircraft_global_fallback && weapon_selection==0 &&
                            aimbot::GetLocalWeaponLocalPosition(weapon_local,local_unit_base);
                        weapon_next = now + std::chrono::milliseconds(100);
                    }
                    if (weapon_valid) {
                        const bool has_local_rot=frame_policy::needs_local_rotation(sdk::air_lead_enabled,
                            sdk::air_lead_use_simulation,sdk::air_lead_use_weapon_position) &&
                            local_rotation.get([&](rotation_matrix_t& value) {
                                return TargetProcess->Read(local_unit_base+
                                    offsets::unit_offsets::rotation_matrix_offset,&value,sizeof(value));
                            });
                        if (has_local_rot) {
                            origin = frame.local_pos + local_rotation.value.transform(weapon_local);
                        }
                        // Failed rotation: retain aircraft origin, never treat local offsets as world coordinates.
                    }
                }

                const uintptr_t air_container=get_air_container(unit_base,now);
                if(cache.last_container!=air_container) {
                    cache.motion={};cache.turn_state={};cache.initialized=false;
                }
                cache.last_container=air_container;
                const auto native=read_native_motion(air_container);
                // Native reads occur now, not at snapshot capture: include their latency.
                const auto read_complete=std::chrono::steady_clock::now();
                const float read_age=std::chrono::duration<float>(read_complete-curr_snapshot.captured_at).count();
                const float own_age=std::chrono::duration<float>(read_complete-shooter.sampled_at).count();
                if(!std::isfinite(read_age) || read_age<0 || read_age>.2f ||
                    !std::isfinite(own_age) || own_age<0 || own_age>.2f) {
                    cache.initialized=false;cache.motion={};cache.turn_state={};
                    if(!drew_air_lead)sdk::air_lead_status="Lectura A2A fuera de tiempo";
                    return;
                }
                const auto motion=air_lead_tracking::observe(cache.motion,unit_pos,
                    std::chrono::duration<double>(curr_snapshot.captured_at.time_since_epoch()).count(),
                    native.have_velocity?&native.velocity:nullptr,native.have_acceleration?&native.acceleration:nullptr);
                if(motion.reset){cache.turn_state={};cache.initialized=false;}
                cache.solve_origin=origin;
                if(!motion.valid) {
                    cache.initialized=false;
                    if(!drew_air_lead)sdk::air_lead_status="Esperando movimiento fiable del objetivo";
                    return;
                }
                air_velocity=motion.velocity;air_acceleration=motion.acceleration;
                cache.turn_omega=air_lead_model::update_turn_state(air_velocity,air_acceleration,
                    motion.dt,unit_base,&cache.turn_state);
                cache.prediction_velocity = air_velocity;
                cache.prediction_acceleration = air_acceleration;
                { // already paced by next_sample above
                    const aimbot::BallisticsData local_bal = aimbot::ballisticsData;
                    const float sample_age = read_age;
                    const float delay = std::clamp(sdk::air_lead_extra_time, 0.0f, 0.5f);
                    // Sin datos de munición reales, 300 m/s fabricados desplazaban
                    // el marcador cientos de metros: ocultarlo en vez de mentir.
                    if (!std::isfinite(local_bal.velocity) || local_bal.velocity<50.f ||
                        local_bal.velocity>4000.f) {
                        cache.initialized=false;
                        if(!drew_air_lead) sdk::air_lead_status="Municion A2A sin velocidad fiable";
                        return;
                    }
                    const air_intercept::encounter encounter{origin,unit_pos,shooter.sample.velocity,
                        shooter.sample.acceleration,air_velocity,air_acceleration,shooter.omega,
                        cache.turn_omega,sample_age,delay,sdk::air_lead_use_turn_prediction,shooter.sample.valid};
                    const auto solution=air_intercept::solve(encounter,{local_bal.velocity,local_bal.mass,
                        aimbot::aircraft_ballistics_policy::caliber_for_air_solver(
                            local_bal.caliber,local_bal.caliber_in_mm),local_bal.drag});
                    if (!solution.valid)
                    {
                        cache.initialized = false;
                        if(!drew_air_lead) sdk::air_lead_status=air_intercept::reason(solution.status);
                        return;
                    }
                    if(cache.initialized)cache.prediction_velocity=air_lead_tracking::lead_velocity(
                        cache.raw_lead_point,solution.point,std::chrono::duration<float>(read_complete-cache.last_update).count(),air_velocity);
                    // The aim-point derivative is not the target acceleration.
                    cache.prediction_acceleration={};
                    cache.raw_lead_point = solution.point;
                    cache.flight_time=solution.time;cache.model_error=solution.error;
                    cache.own_source=shooter.sample.provenance;cache.target_source=motion.provenance;

                    if (!cache.initialized) {
                        cache.smoothed_lead_point = cache.raw_lead_point;
                        cache.last_smooth = read_complete;
                        cache.initialized = true;
                    }
                    cache.last_update = read_complete;
                }
            } // sampled work; only projection/smoothing below runs at render FPS

            if (cache.initialized)
            {
                const auto render_now=std::chrono::steady_clock::now();
                const float render_sample_age=std::chrono::duration<float>(render_now-cache.last_sample).count();
                if(!std::isfinite(render_sample_age) || render_sample_age<0 || render_sample_age>.2f) {
                    cache.initialized=false;
                    if(!drew_air_lead)sdk::air_lead_status="Solucion A2A caducada";
                    return;
                }
                float smooth_alpha = 0.5f;
                if (sdk::air_lead_smooth_adaptive) {
                    const float min_a = std::clamp(sdk::air_lead_smooth_min_alpha, 0.05f, 0.95f);
                    const float max_a = std::clamp(sdk::air_lead_smooth_max_alpha, min_a, 0.95f);
                    const float distance_to_target = (unit_pos - cache.solve_origin).length();
                    const float reactivity = std::clamp((2500.0f - distance_to_target) / 2500.0f, 0.0f, 1.0f);
                    smooth_alpha = min_a + (max_a - min_a) * reactivity;
                }

                const float dt_render = std::clamp(
                    std::chrono::duration<float>(render_now - cache.last_smooth).count(), 0.0f, 0.10f);
                cache.last_smooth = render_now;
                const float tau = -1.0f / (60.0f * std::log1p(-std::clamp(smooth_alpha, 0.05f, 0.95f)));
                const float alpha_time = 1.0f - std::exp(-dt_render / tau);
                const float raw_age = std::clamp(
                    std::chrono::duration<float>(render_now - cache.last_update).count(), 0.0f, 0.10f);
                const vec3_t live_raw = cache.raw_lead_point + cache.prediction_velocity * raw_age
                    + cache.prediction_acceleration * (0.5f * raw_age * raw_age);
                cache.smoothed_lead_point += cache.prediction_velocity * dt_render
                    + cache.prediction_acceleration * (0.5f * dt_render * dt_render);
                cache.smoothed_lead_point += (live_raw - cache.smoothed_lead_point) * alpha_time;

                if (std::isfinite(cache.smoothed_lead_point.x) &&
                    std::isfinite(cache.smoothed_lead_point.y) &&
                    std::isfinite(cache.smoothed_lead_point.z))
                {
                    vec2_t lead_screen;
                    if (world_to_screen_counted(cache.smoothed_lead_point, lead_screen, camera_matrix, projection_ctx, w2s_calls))
                    {
                        vec2_t unit_screen_pos{};
                        const bool have_target=world_to_screen_counted(unit_pos,unit_screen_pos,camera_matrix,projection_ctx,w2s_calls);
                        draw_lead_marker(lead_screen,have_target ? &unit_screen_pos : nullptr);
                        drew_air_lead=true;
                        sdk::air_lead_own_motion_status=air_lead_tracking::label(cache.own_source);
                        sdk::air_lead_target_motion_status=air_lead_tracking::label(cache.target_source);
                        sdk::air_lead_flight_time=cache.flight_time;sdk::air_lead_model_error=cache.model_error;
                        sdk::air_lead_status=cache.own_source!=air_lead_tracking::source::none ? "Dibujando A2A (herencia de velocidad propia)" :
                            "Dibujando A2A sin compensacion propia (sin muestra fiable)";
                    } else if(!drew_air_lead)
                        sdk::air_lead_status="Prediccion A2A fuera de pantalla";
                }
            }
            };

        if (sdk::aimbot_enabled && frame.local_is_plane)
            sdk::ground_prediction_status="G2G requiere unidad terrestre local";
        else if (sdk::aimbot_enabled) sdk::ground_prediction_status = sdk::esp_only_show_planes
            ? "Only Show Planes oculta los objetivos terrestres"
            : "Sin objetivo terrestre visible (filtros/FOV/distancia)";

        const auto units_start=std::chrono::steady_clock::now();
        for (c_unit& unit : units)
        {
            vec3_t unit_pos;
            const auto unit_base = unit.get_base();
            const auto curr_pos_it = curr_snapshot.positions.find(unit_base);
            if (curr_pos_it == curr_snapshot.positions.end())
                continue;

            unit_pos = curr_pos_it->second;

            if (unit_pos.empty())
                continue;

            const float dx = unit_pos.x - local_pos.x;
            const float dy = unit_pos.y - local_pos.y;
            const float dz = unit_pos.z - local_pos.z;
            const float distance_sq = dx * dx + dy * dy + dz * dz;
            if (!frame_policy::in_render_range(distance_sq,esp_render_distance_sq)) {
                ++cull_distance;
                continue;
            }

            auto& cached_info = get_cached_unit_info(unit_base, unit);
            const std::string& unit_type = cached_info.unit_type;
            if (unit_type.empty()) {
                ++cull_type;
                continue;
            }

            const uint16_t raw_type = cached_info.raw_type;
            const auto unit_type_info = get_unit_type_info(unit_type, raw_type);
            if(!air_ghost_filter::current.visible(unit_base,unit_type_info.is_plane,
                {unit_pos.x,unit_pos.y,unit_pos.z},air_filter_sample_time)) {
                ++cull_type;continue;
            }

            if (sdk::esp_only_show_planes && !unit_type_info.is_plane) {
                ++cull_type;
                continue;
            }

            vec2_t candidate_screen{};
            bool candidate_projected=false;
            bool process_air_lead = sdk::air_lead_enabled && sdk::air_lead_use_simulation &&
                frame.local_is_plane && local_unit_base && unit_type_info.is_plane && local_team != -1 &&
                is_air_lead_target(unit_type, unit_type_info);

            if (process_air_lead)
            {
                uint8_t team_byte = get_unit_team(unit_base);
                if (team_byte!=0xFF && static_cast<int32_t>(team_byte) != local_team)
                {
                    if (sdk::air_lead_only_target)
                    {
                        vec2_t sp;
                        if (world_to_screen_counted(unit_pos, sp, camera_matrix, projection_ctx, w2s_calls))
                        {
                            candidate_screen=sp;
                            candidate_projected=true;
                            const float dx_center = sp.x - viewport_center.x;
                            const float dy_center = sp.y - viewport_center.y;
                            const float screen_dist_sq = dx_center * dx_center + dy_center * dy_center;
                            const float score = screen_dist_sq + distance_sq * 0.05f;
                            if (!has_best_air_lead_target || score < best_air_lead_score)
                            {
                                best_air_lead_unit = unit;
                                best_air_lead_pos = unit_pos;
                                best_air_lead_score = score;
                                has_best_air_lead_target = true;
                            }
                        }
                    }
                    else
                    {
                        run_air_lead(unit, unit_pos);
                    }
                }
            }

            // Independent from G2G selection and A2A enable/simulation settings.
            if(process_g2a && unit_base!=local_unit_base && unit_type_info.is_plane &&
                !unit_type_info.is_dummy && unit_type_info.worth_to_draw && local_team!=-1 &&
                distance_sq<=ballistic_render_distance_sq) {
                const uint8_t team=get_unit_team(unit_base);
                vec2_t sp{};
                if(team!=0xFF && static_cast<int32_t>(team)!=local_team &&
                    world_to_screen_counted(unit_pos,sp,camera_matrix,projection_ctx,w2s_calls)) {
                    const float x=sp.x-viewport_center.x,y=sp.y-viewport_center.y;
                    const float center_sq=x*x+y*y;
                    const float score=center_sq+distance_sq*.05f;
                    if(aim_fov_radius_sq>0 && center_sq<=aim_fov_radius_sq && score<best_g2a_score) {
                        best_g2a_base=unit_base;best_g2a_pos=unit_pos;best_g2a_score=score;
                    }
                }
            }

            if (unit_type_info.is_dummy || !unit_type_info.worth_to_draw) {
                ++cull_type;
                continue;
            }

            if (!is_point_in_clip_frustum(unit_pos, camera_matrix, projection_ctx)) {
                ++cull_frustum;
                continue;
            }

            vec2_t screen_pos=candidate_screen;
            if (!candidate_projected && !world_to_screen_counted(unit_pos, screen_pos, camera_matrix, projection_ctx, w2s_calls)) {
                ++cull_projection;
                continue;
            }

            const float distance_f = std::sqrt(distance_sq);
            const std::string& vehicle_name = cached_info.vehicle_name;
            const float dx_center = screen_pos.x - viewport_center.x;
            const float dy_center = screen_pos.y - viewport_center.y;
            const float screen_dist_sq = dx_center * dx_center + dy_center * dy_center;
            const bool is_inside_fov = (aim_fov_radius_sq > 0.0f) && (screen_dist_sq <= aim_fov_radius_sq);

            if(!unit_type_info.worth_to_draw || distance_sq>esp_render_distance_sq) {
                ++cull_type; continue;
            }
            bool show_name_for_unit = true;
            if (distance_sq > far_simplified_distance_sq)
                show_name_for_unit = is_inside_fov;

            float label_base_y = screen_pos.y; // centro valido aunque la OBB se recorte
            projected_box_bounds_t projected_box{};
            const bool needs_reload_box=reload_display_policy::eligible(sdk::esp_show_reload,
                unit_type_info.show_reload,distance_sq,esp_render_distance_sq);

            const bool should_draw_any_box = sdk::esp_show_boxes || sdk::esp_use_3d_boxes;
            if (unit_type_info.draw_box && should_draw_any_box)
            {
                const float active_box_distance_sq = esp_box_distance_sq;
                if (distance_sq > active_box_distance_sq)
                {
                    label_base_y = screen_pos.y;
                }
                else
                {
                    if (sdk::esp_use_3d_boxes)
                    {
                        auto& box_info = get_cached_box_info(unit_base, unit, distance_f);
                        ++obb_calls;
                        const auto world_corners =
                            calculate_bbox_corners_rotated(unit_pos, box_info.bbmin, box_info.bbmax, box_info.rot);

                        std::array<vec2_t, 8> sc;
                        bool                  visible = true;

                        for (size_t i = 0; i < 8; ++i)
                        {
                            if (!world_to_screen_counted(world_corners[i], sc[i], camera_matrix, projection_ctx, w2s_calls))
                            {
                                visible = false;
                                break;
                            }
                        }

                        if (visible)
                        {
                            float box_bottom_y = sc[0].y;
                            for (size_t i = 1; i < 8; ++i)
                                box_bottom_y = std::max(box_bottom_y, sc[i].y);

                            label_base_y = frame_policy::label_anchor(screen_pos.y,box_bottom_y,true);
                            draw_wireframe_box(sc, unit_type_info.color, 1.f);
                            if (needs_reload_box)
                                projected_box=projected_bounds_from_screen_corners(sc);
                        }
                        else {
                            ++partial_boxes; // la caja incompleta no se dibuja, la etiqueta si
                        }
                    }
                    else if (sdk::esp_show_boxes)
                    {
                        float projected_max_y = 0.0f;
                        if (draw_projected_2d_box_from_obb(unit, unit_pos, distance_f, camera_matrix, projection_ctx, w2s_calls, obb_calls, unit_type_info.color, &projected_max_y,
                            needs_reload_box ? &projected_box : nullptr))
                            label_base_y = projected_max_y;
                    }
                }
            }
            else
            {
                label_base_y = screen_pos.y;
            }

            if (label_base_y > 0.f)
            {
                float text_y = label_base_y + 5.0f;

                if (sdk::esp_show_vehicle_names && show_name_for_unit && !vehicle_name.empty())
                {
                    g_render->text(
                        { screen_pos.x, text_y },
                        unit_type_info.color,
                        0,
                        vehicle_name,
                        esp_font,
                        sdk::esp_font_size_name
                    );
                    text_y += 15.0f;
                }

                if (sdk::esp_show_distance) {
                    char  buf[16];
                    std::snprintf(buf, sizeof(buf), "%.0fm", distance_f);

                    g_render->text(
                        { screen_pos.x, text_y },
                        unit_type_info.color,
                        0,
                        buf,
                        esp_font,
                        sdk::esp_font_size_distance
                    );
                    text_y += 15.0f;
                }
            }

            if(sdk::esp_show_invulnerable && offsets::api_invulnerable_ready &&
                invulnerability_display_policy::ground_vehicle_type(unit_type,raw_type) &&
                distance_sq<=sdk::esp_invul_distance*sdk::esp_invul_distance &&
                unit_base!=local_unit_base && main_viewport &&
                screen_pos.x>=main_viewport->Pos.x+6.f && screen_pos.y>=main_viewport->Pos.y+6.f &&
                screen_pos.x<main_viewport->Pos.x+main_viewport->Size.x-6.f &&
                screen_pos.y<main_viewport->Pos.y+main_viewport->Size.y-6.f)
                invul_candidates.push_back({unit_base,screen_pos,distance_sq});

            if (needs_reload_box)
                reload_candidates.push_back({unit_base,screen_pos,distance_sq,unit,unit_pos,distance_f,projected_box});

            const bool allow_aimbot_for_unit = !frame.local_is_plane && unit_type_info.draw_box && !unit_type_info.is_plane;
            if (sdk::aimbot_enabled && allow_aimbot_for_unit && distance_sq <= ballistic_render_distance_sq)
            {
                if (aim_fov_radius_sq > 0.0f && screen_dist_sq <= aim_fov_radius_sq)
                {
                    const float score = screen_dist_sq + (distance_sq * 0.05f);
                    if (!has_best_aimbot_target || score < best_aimbot_score)
                    {
                        best_aimbot_unit = unit;
                        best_aimbot_pos = unit_pos;
                        best_aimbot_score = score;
                        has_best_aimbot_target = true;
                    }
                }
            }

            if (sdk::bShowDamageModel && local_team != -1 && distance_f <= sdk::xray_render_distance)
            {
                if (unit_type_info.draw_box && !unit_type_info.is_plane)
                {
                    uint8_t team_byte = get_unit_team(unit_base);
                    int32_t team = static_cast<int32_t>(team_byte);
                    if (team_byte!=0xFF && team != local_team)
                    {
                        float score = screen_dist_sq + distance_sq * 0.05f;
                        if (!has_xray_target || score < best_xray_score)
                        {
                            best_xray_unit = unit;
                            best_xray_pos = unit_pos;
                            best_xray_distance_f = distance_f;
                            best_xray_score = score;
                            has_xray_target = true;
                        }
                    }
                }
            }
        }

        misc::prof_esp_units_us.store(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-units_start).count()),std::memory_order_relaxed);
        // Single prioritized G2A target; only fresh snapshots feed motion histories.
        g2a_guard.reached=true;
        if(!process_g2a || !best_g2a_base) {
            g2a_cache = {};
            g2a_lead::sample_age_ms = -1;
            g2a_lead::own_source = "Sin velocidad fiable";
            g2a_lead::target_source = "Sin velocidad fiable";
            g2a_lead::acceleration_source = "Sin aceleracion fiable";
            g2a_lead::quality = "G2A requiere unidad terrestre local activa";
        } else {
            const auto now = std::chrono::steady_clock::now();
            const float age = std::chrono::duration<float>(now - curr_snapshot.captured_at).count();
            const auto& b = aimbot::ballisticsData;
            const ballistic_validation::round_data round{b.velocity, b.mass,
                b.caliber * (b.caliber_in_mm ? .001f : 1.f), b.length, b.max_dist};
            const auto& previous = g2a_cache.ammunition;
            if (g2a_cache.owner != local_unit_base || g2a_cache.target != best_g2a_base || g2a_cache.epoch != current_epoch ||
                previous.speed != round.speed || previous.mass != round.mass || previous.caliber_m != round.caliber_m ||
                previous.length_m != round.length_m || previous.max_distance != round.max_distance) {
                g2a_cache = {};
                g2a_cache.owner = local_unit_base;
                g2a_cache.target = best_g2a_base;
                g2a_cache.epoch = current_epoch;
                g2a_cache.ammunition = round;
            }
            air_intercept::ammunition checked{};
            bool invalidate_solution = false;
            if (!std::isfinite(age) || age < 0 || age > .2f || !g2a_lead::ammunition(round, checked)) {
                invalidate_solution = true;
            }
            if (curr_snapshot.captured_at < g2a_cache.latest) {
                invalidate_solution = true;
                g2a_cache.own = {}; g2a_cache.enemy = {};
            } else {
                g2a_cache.latest = curr_snapshot.captured_at;
            }
            // sample belongs to the last observation, not the latest rendered frame.
            const double interval = std::chrono::duration<double>(curr_snapshot.captured_at - g2a_cache.sample).count();
            if (interval > .2) {
                g2a_cache.own = {}; g2a_cache.enemy = {}; g2a_cache.next = {};
                g2a_cache.solution = {};
            }
            const float held_age = std::chrono::duration<float>(now - g2a_cache.sample).count();
            if (!std::isfinite(held_age) || held_age < 0 || held_age > .2f)
                g2a_cache.solution = {};
            if (g2a_cache.owner != local_unit_base || g2a_cache.target != best_g2a_base || g2a_cache.epoch != current_epoch) {
                invalidate_solution = true;
            }
            if (invalidate_solution) {
                g2a_cache.solution = {};
            }
            if (now >= g2a_cache.next && interval >= .025 && !invalidate_solution) {
                g2a_cache.sample = curr_snapshot.captured_at;
                g2a_cache.next = now + std::chrono::milliseconds(30);
                vec3_t own_velocity{}; uintptr_t source = 0;
                const bool own_known = aimbot::ReadGroundVelocity(local_unit_base, own_velocity, source);
                const auto container = get_air_container(best_g2a_base, now);
                if (container != g2a_cache.container || source != g2a_cache.own_container) {
                    g2a_cache.own = {}; g2a_cache.enemy = {};
                    g2a_cache.container = container; g2a_cache.own_container = source;
                }
                const auto native = read_native_motion(container);
                const auto read_complete = std::chrono::steady_clock::now();
                const float read_age = std::chrono::duration<float>(read_complete - curr_snapshot.captured_at).count();
                if (!std::isfinite(read_age) || read_age < 0 || read_age > .2f) {
                    invalidate_solution = true;
                    g2a_cache.own = {}; g2a_cache.enemy = {}; g2a_cache.solution = {};
                } else {
                    const double stamp = std::chrono::duration<double>(curr_snapshot.captured_at.time_since_epoch()).count();
                    g2a_cache.own_sample = g2a_lead::observe(g2a_cache.own, frame.local_pos, stamp,
                        own_known ? &own_velocity : nullptr, nullptr, 150);
                    g2a_cache.enemy_sample = g2a_lead::observe(g2a_cache.enemy, best_g2a_pos, stamp,
                        native.have_velocity ? &native.velocity : nullptr, native.have_acceleration ? &native.acceleration : nullptr, 950);
                    g2a_cache.solution = g2a_lead::solve_motion(frame.local_pos, best_g2a_pos,
                        g2a_cache.own_sample, g2a_cache.enemy_sample, read_age, round);
                }
            }
            // Recheck at publication: a long solve cannot publish an expired sample.
            const float publication_age=std::chrono::duration<float>(std::chrono::steady_clock::now()-g2a_cache.sample).count();
            if(!std::isfinite(publication_age) || publication_age<0 || publication_age>.2f) {
                g2a_cache.solution={};invalidate_solution=true;
            }
            if (!invalidate_solution && g2a_cache.sample != std::chrono::steady_clock::time_point{}) {
                g2a_lead::sample_age_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g2a_cache.sample).count();
                g2a_lead::own_source = air_lead_tracking::label(g2a_cache.own_sample.motion.provenance);
                g2a_lead::target_source = air_lead_tracking::label(g2a_cache.enemy_sample.motion.provenance);
                g2a_lead::acceleration_source = g2a_cache.enemy_sample.acceleration;
                g2a_lead::quality = g2a_cache.solution.valid ? "Modelo acotado: giro/aceleracion 0.85 s; intercepcion 4 s" :
                    "Sin solucion actual fiable (historial/lectura/limites)";
            }
            const auto& result = g2a_cache.solution;
            g2a_lead::status = g2a_lead::reason(result.status);
            if (result.valid) {
                vec2_t sp{};
                if (main_viewport && world_to_screen_counted(result.point, sp, camera_matrix, projection_ctx, w2s_calls) &&
                    ground_prediction::screen_visible(sp.x, sp.y, main_viewport->Pos.x, main_viewport->Pos.y,
                        main_viewport->Size.x, main_viewport->Size.y)) {
                    vec2_t unit_screen_pos{};
                    const bool have_target=world_to_screen_counted(best_g2a_pos,unit_screen_pos,camera_matrix,projection_ctx,w2s_calls);
                    draw_lead_marker(sp,have_target ? &unit_screen_pos : nullptr);
                    g2a_lead::flight_time = result.time; g2a_lead::model_error = result.error;
                } else {
                    g2a_lead::status = "G2A solucion fuera de pantalla";
                }
            }
        }

        if (sdk::air_lead_only_target && has_best_air_lead_target)
        {
            run_air_lead(best_air_lead_unit, best_air_lead_pos);
        }

        {
            const auto now = std::chrono::steady_clock::now();
            static auto air_lead_gc = std::chrono::steady_clock::now();
            if (now - air_lead_gc > std::chrono::seconds(10)) {
                for (auto it = air_lead_cache.begin(); it != air_lead_cache.end();) {
                    if (now - it->second.last_update > std::chrono::seconds(10))
                        it = air_lead_cache.erase(it);
                    else
                        ++it;
                }
                for (auto it = air_container_cache.begin(); it != air_container_cache.end();) {
                    if (now > it->second.next_update + std::chrono::seconds(10))
                        it = air_container_cache.erase(it);
                    else
                        ++it;
                }
                air_lead_gc = now;
            }
        }


        const auto xray_start=std::chrono::steady_clock::now();
        if (has_xray_target)
            draw_xray_modules(best_xray_unit, best_xray_pos, camera_matrix, projection_ctx, w2s_calls, best_xray_distance_f);
        misc::prof_esp_xray_us.store(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-xray_start).count()),std::memory_order_relaxed);

        // Independiente de G2G/FOV/caja 3D: una lectura visual por enemigo
        // cercano, limitada para no multiplicar DMA en cada frame.
        const auto reload_start=std::chrono::steady_clock::now();
        misc::prof_reload_candidates.store(reload_candidates.size(),std::memory_order_relaxed);
        if (sdk::esp_show_reload && !reload_candidates.empty()) {
            const auto count=(std::min)(reload_candidates.size(),reload_display_policy::max_indicators);
            std::partial_sort(reload_candidates.begin(),reload_candidates.begin()+count,
                reload_candidates.end(),[](const auto& a,const auto& b) {
                    return a.distance_sq<b.distance_sq;
                });
            uint64_t drawn=0,failed=0,unexpected=0;
            for (std::size_t i=0;i<count;++i) {
                auto& target=reload_candidates[i];
                const auto& visual=get_cached_reload_info(target.base);
                if (!visual.valid) {++failed;continue;}
                if (misc::prof_reload_sample.load(std::memory_order_relaxed)<0)
                    misc::prof_reload_sample.store(visual.value,std::memory_order_relaxed);
                if (!reload_display_policy::supported_visual_value(visual.value)) {
                    ++unexpected; // exponer lectura incoherente sin inventar tiempo
                    if (sdk::esp_show_reload_seconds) {
                        char raw_label[24];
                        std::snprintf(raw_label,sizeof(raw_label),runtime_language::text("Raw %u").c_str(),unsigned(visual.value));
                        g_render->text({target.screen.x+18.f,target.screen.y-24.f},
                            IM_COL32(255,190,95,255),0,raw_label,esp_font,sdk::esp_font_size_reload);
                    }
                    continue;
                }
                // Ubicación clásica: barra fina junto a la caja proyectada, no una
                // columna fija de 34 px flotando junto al centro del vehículo.
                // La caja ya proyectada en este frame evita repetir 8 esquinas.
                const auto box=target.cached_box.valid ? target.cached_box :
                    get_projected_box_bounds(target.unit,target.position,
                        target.distance_m,camera_matrix,projection_ctx,w2s_calls,obb_calls);
                if (!box.valid) {++failed;continue;}
                const float height=box.max_y-box.min_y;
                if (!std::isfinite(height) || height<3.f) {++failed;continue;}
                float bar_x=box.max_x+4.f;
                if (main_viewport && bar_x>main_viewport->Pos.x+main_viewport->Size.x-5.f)
                    bar_x=box.min_x-5.f;
                const float fraction=reload_display_policy::ready_fraction(visual.value);
                g_render->line(bar_x,box.min_y,bar_x,box.max_y,IM_COL32(0,0,0,210),4.f);
                g_render->line(bar_x,box.max_y,bar_x,box.max_y-height*fraction,
                    IM_COL32(0,215,45,255),2.f);
                if (sdk::esp_show_reload_seconds) {
                    char label[24];
                    std::snprintf(label,sizeof(label),"%.1fs",(10.f/16.f)*visual.value);
                    g_render->text({bar_x+5.f,box.min_y-14.f},IM_COL32(255,255,255,255),
                        0,label,esp_font,sdk::esp_font_size_reload);
                }
                ++drawn;
            }
            misc::prof_reload_drawn.store(drawn,std::memory_order_relaxed);
            misc::prof_reload_failed.store(failed,std::memory_order_relaxed);
            misc::prof_reload_unexpected.store(unexpected,std::memory_order_relaxed);
        }

        misc::prof_esp_reload_us.store(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-reload_start).count()),std::memory_order_relaxed);
        const auto invul_start=std::chrono::steady_clock::now();
        misc::prof_invul_candidates.store(invul_candidates.size(),std::memory_order_relaxed);
        if (!invul_candidates.empty()) {
            const auto count=(std::min)(invul_candidates.size(),
                invulnerability_display_policy::max_indicators);
            std::partial_sort(invul_candidates.begin(),invul_candidates.begin()+count,
                invul_candidates.end(),[](const auto& a,const auto& b){return a.distance_sq<b.distance_sq;});
            uint64_t shown=0,failed=0,invalid=0,timers=0;
            for(std::size_t i=0;i<count;++i) {
                const auto& target=invul_candidates[i];
                const auto& state=get_cached_invulnerability(target.base);
                if(state.invalid_flag) {++invalid;continue;}
                if(!state.flag_valid) {++failed;continue;}
                if(!state.invulnerable) continue; // el timer no decide el estado
                char label[64];
                if(sdk::esp_invul_show_timer && state.timer_valid) {
                    std::snprintf(label,sizeof(label),"INVUL %.1f",state.timer);
                    ++timers;
                } else {
                    std::snprintf(label,sizeof(label),"INVUL");
                }
                g_render->text({target.screen.x+7.f,target.screen.y-19.f},
                    ImGui::ColorConvertFloat4ToU32(sdk::color_invulnerable),
                    0,label,esp_font,std::clamp(sdk::esp_font_size_invul,8.f,18.f));
                ++shown;
            }
            misc::prof_invul_shown.store(shown,std::memory_order_relaxed);
            misc::prof_invul_failed.store(failed,std::memory_order_relaxed);
            misc::prof_invul_invalid.store(invalid,std::memory_order_relaxed);
            misc::prof_invul_timer_shown.store(timers,std::memory_order_relaxed);
        }

        misc::prof_esp_invul_us.store(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-invul_start).count()),std::memory_order_relaxed);
        if (sdk::aimbot_enabled && has_best_aimbot_target)
        {
            const auto aimbot_start = std::chrono::steady_clock::now();
            aimbot::run(best_aimbot_unit, best_aimbot_pos, local_pos, camera_matrix, curr_snapshot.captured_at, local_unit_base, current_epoch);
            const auto aimbot_end = std::chrono::steady_clock::now();
            aimbot_us_total += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(aimbot_end - aimbot_start).count()
                );
            ++aimbot_calls;
        }

        if (sdk::esp_show_fps && g_render->draw_list() && esp_font)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 anchor = viewport
                ? ImVec2(viewport->Pos.x + 15.0f, viewport->Pos.y + 15.0f)
                : ImVec2(15.0f, 15.0f);

            char fps_buf[32];
            std::snprintf(fps_buf, sizeof(fps_buf), "FPS: %.0f", ImGui::GetIO().Framerate);
            g_render->draw_list()->AddText(esp_font, esp_font->LegacySize, anchor, IM_COL32(255, 255, 255, 255), fps_buf);
        }

        const auto esp_end = std::chrono::steady_clock::now();
        misc::prof_esp_us.store(
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(esp_end - esp_start).count()),
            std::memory_order_relaxed
        );
        misc::prof_esp_count.fetch_add(1, std::memory_order_relaxed);
        misc::prof_aimbot_us.store(aimbot_us_total, std::memory_order_relaxed);
        misc::prof_aimbot_calls.store(aimbot_calls, std::memory_order_relaxed);
        misc::prof_esp_w2s_calls.store(static_cast<uint64_t>(w2s_calls), std::memory_order_relaxed);
        misc::prof_esp_obb_calls.store(static_cast<uint64_t>(obb_calls), std::memory_order_relaxed);
        misc::prof_esp_cull_distance.store(cull_distance,std::memory_order_relaxed);
        misc::prof_esp_cull_type.store(cull_type,std::memory_order_relaxed);
        misc::prof_esp_cull_frustum.store(cull_frustum,std::memory_order_relaxed);
        misc::prof_esp_cull_projection.store(cull_projection,std::memory_order_relaxed);
        misc::prof_esp_partial_boxes.store(partial_boxes,std::memory_order_relaxed);
    }
}   // namespace esp