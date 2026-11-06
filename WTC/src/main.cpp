#include "gui/runtime_language.hpp"
#include "includes.hpp"
#include "core/core.hpp"
#include "core/app_version.hpp"
#include "features/esp/g2a_settings.hpp"
#include "core/config.hpp"
#include "core/hotkey_validation.hpp"
#include "core/missile_registry_storage.hpp"
#include "gui/overlay_profile_position.hpp"
#include "gui/menu_preferences.hpp"
#include "features/aimbot/ammunition_quality.hpp"
#include "core/quality_diagnostics.hpp"
#include <commdlg.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#pragma comment(lib, "dcomp.lib")

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain1* g_pSwapChain = nullptr;
static IDCompositionDevice* g_compositionDevice = nullptr;
static IDCompositionTarget* g_compositionTarget = nullptr;
static IDCompositionVisual* g_compositionVisual = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HMONITOR                 g_CurrentMonitor = nullptr;
static bool                     g_F1DownLastFrame = false;
static bool                     g_F2DownLastFrame = false;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void UpdateCurrentMonitor(HWND hWnd);
void ApplyFullscreenToMonitor(HWND hWnd, int index);
int clamp_monitor_index(int value, int count);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void ApplyMenuStyle(int style)
{
    ImGuiStyle& st = ImGui::GetStyle();
    ImVec4* colors = st.Colors;
    ImGui::StyleColorsDark();

    switch (style)
    {
    case 0: // Dark
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.22f, 0.60f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.95f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.38f, 0.43f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.20f, 0.20f, 0.80f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        st.WindowRounding = 6.0f;
        break;
    case 1: // Classic
        ImGui::StyleColorsClassic();
        st.WindowRounding = 4.0f;
        break;
    case 2: // Crimson
        ImGui::StyleColorsDark();
        colors[ImGuiCol_Button] = ImVec4(0.40f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.15f, 0.15f, 0.90f);
        st.WindowRounding = 8.0f;
        break;
    case 3: // Midnight
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.15f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.18f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.15f, 0.45f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.20f, 0.80f, 0.80f);
        st.WindowRounding = 10.0f;
        break;
    case 4: // Minimal Light
        ImGui::StyleColorsLight();
        colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.97f, 0.97f, 0.99f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.75f, 0.78f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.60f, 0.20f, 1.00f);
        st.WindowRounding = 5.0f;
        st.FrameRounding = 3.0f;
        break;
    case 5: // Forest
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.12f, 0.08f, 0.94f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.15f, 0.10f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.30f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.40f, 0.25f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.90f, 0.30f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 0.30f, 0.80f);
        st.WindowRounding = 7.0f;
        break;
    case 6: // Ocean
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.10f, 0.16f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.03f, 0.12f, 0.19f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.10f, 0.25f, 0.35f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.35f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.55f, 0.70f, 0.80f);
        st.WindowRounding = 9.0f;
        break;
    }

    st.FrameRounding = 4.0f;
    st.GrabRounding = 4.0f;
    st.ScrollbarRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.WindowPadding = ImVec2(10.0f, 10.0f);
    st.FramePadding = ImVec2(6.0f, 4.0f);
    st.ItemSpacing = ImVec2(8.0f, 6.0f);
    st.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    st.Alpha = 1.0f;
    st.AntiAliasedLines = true;
    st.AntiAliasedFill = true;
}

namespace GUI
{
    struct menu_preset_t
    {
        // ESP
        bool  esp_enabled = true;
        bool  radar_enabled = true;
        bool  aimbot_enabled = false;
        float esp_render_distance = 2000.0f;
        float esp_box_distance = 2000.0f;
        bool  esp_show_boxes = true;
        bool  esp_use_3d_boxes = true;
        bool  esp_show_vehicle_names = true;
        bool  esp_show_distance = true;
        bool  esp_show_reload = false;
        bool  esp_show_invulnerable = true;
        bool  esp_invul_show_timer = true;
        float esp_invul_distance = 2000.f;
        float esp_font_size_invul = 9.f;
        ImVec4 color_invulnerable = ImVec4(.98f,.72f,.28f,1.f);
        bool  esp_show_reload_seconds = false;
        bool  esp_show_fps = true;
        bool  esp_show_bomb_impact = true;
        bool  esp_only_show_planes = false;

        // Projectile settings participate in manual saving and every profile.
        bool projectile_esp_enabled = false;
        bool projectile_debug = false;
        bool projectile_incoming_only = false;
        bool projectile_incoming_use_localplayer = true;
        bool projectile_aam_only = true;
        std::map<std::string,bool> missile_overrides;
        menu_preferences::settings ui;
        std::map<std::string,projectile_visibility::category> missile_categories;
        projectile_visibility::category_policy_map missile_policy{};
        missile_alerts::Settings alerts;
        std::array<bool,esp::radar::icons::visible.size()> radar_kinds = [] {
            std::array<bool,esp::radar::icons::visible.size()> result{}; result.fill(true); return result; }();
        float projectile_incoming_distance = 3000.0f;
        float projectile_incoming_angle = 30.0f;
        int projectile_scan_budget = 4096;

        // Vehicle filters
        bool show_helicopter = true;
        bool show_bomber = true;
        bool show_fighter = true;
        bool show_attacker = true;
        bool show_light_tank = true;
        bool show_heavy_tank = true;
        bool show_medium_tank = true;
        bool show_spaa = true;
        bool show_gun_boat = true;
        bool show_destroyer = true;
        bool show_cruiser = true;
        bool show_torpedo_boat = true;
        bool show_recon_drone = true;

        int death_state_threshold = 1;

        // Vehicle colors
        ImVec4 color_helicopter = ImVec4(247.f / 255.f, 163.f / 255.f, 159.f / 255.f, 1.f);
        ImVec4 color_bomber = ImVec4(155.f / 255.f, 165.f / 255.f, 237.f / 255.f, 1.f);
        ImVec4 color_fighter = ImVec4(241.f / 255.f, 158.f / 255.f, 112.f / 255.f, 1.f);
        ImVec4 color_attacker = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_light_tank = ImVec4(246.f / 255.f, 234.f / 255.f, 237.f / 255.f, 1.f);
        ImVec4 color_heavy_tank = ImVec4(246.f / 255.f, 95.f / 255.f, 99.f / 255.f, 1.f);
        ImVec4 color_medium_tank = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_spaa = ImVec4(184.f / 255.f, 146.f / 255.f, 238.f / 255.f, 1.f);
        ImVec4 color_gun_boat = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_destroyer = ImVec4(246.f / 255.f, 95.f / 255.f, 99.f / 255.f, 1.f);
        ImVec4 color_cruiser = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_torpedo_boat = ImVec4(184.f / 255.f, 146.f / 255.f, 238.f / 255.f, 1.f);
        ImVec4 color_recon_drone = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
        ImVec4 radar_enemy_color = ImVec4(1.0f, 0.255f, 0.255f, 0.96f);

        // X-Ray
        bool  bShowDamageModel = false;
        bool  bShowModuleNames = false;
        bool  bShowModuleGunBarrel = true;
        bool  bShowModuleCannonBreech = true;
        bool  bShowModuleCrew = true;
        bool  bShowModuleFuelTank = true;
        bool  bShowModuleAmmoRack = true;
        bool  bShowModuleEngine = true;
        bool  bShowModuleTransmission = true;
        float xray_small_module_distance = 150.0f;
        float xray_line_thickness = 0.8f;
        float xray_module_alpha = 0.9f;
        float xray_render_distance = 1500.0f;
        float xray_names_distance = 800.0f;
        ImVec4 color_mod_gun_barrel = ImVec4(1.0f, 0.6f, 0.2f, 0.7f);
        ImVec4 color_mod_cannon_breech = ImVec4(0.8f, 0.2f, 0.2f, 0.7f);
        ImVec4 color_mod_crew = ImVec4(0.2f, 0.8f, 0.2f, 0.7f);
        ImVec4 color_mod_fuel_tank = ImVec4(0.2f, 0.5f, 1.0f, 0.7f);
        ImVec4 color_mod_ammo_rack = ImVec4(1.0f, 0.8f, 0.0f, 0.7f);
        ImVec4 color_mod_engine = ImVec4(1.0f, 0.55f, 0.1f, 0.7f);
        ImVec4 color_mod_transmission = ImVec4(0.55f, 0.65f, 1.0f, 0.7f);
        bool  xray_alpha_falloff = true;

        // Air Lead
        bool  air_lead_enabled = false;
        bool  g2a_lead_enabled = false;
        bool  air_lead_use_simulation = true;
        ImVec4 air_lead_sim_color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
        float air_lead_opacity = 0.60f;
        bool air_lead_coverage_opacity = true;
        float menu_ui_scale = 1.0f;
        float air_lead_marker_size = 5.0f;
        float air_lead_line_thickness = 1.2f;
        bool  air_lead_use_weapon_position = false;
        bool  air_lead_smooth_adaptive = true;
        float air_lead_smooth_min_alpha = 0.30f;
        float air_lead_smooth_max_alpha = 0.85f;
        bool  air_lead_use_turn_prediction = true;
        bool  air_lead_target_helicopter = true;
        bool  air_lead_target_recon_drone = true;
        bool  air_lead_target_fighter = true;
        bool  air_lead_target_bomber = true;
        bool  air_lead_target_attacker = true;
        bool  air_lead_only_target = true;
        float air_lead_extra_time = 0.0f;
        int   air_lead_weapon_selection = -1;
        bool  air_lead_use_heaviest_weapon = false;

        // Settings
        int   menu_style = 0;
        bool  esp_vsync_enabled = false;
        int   esp_vsync_fps = 144;
        bool  auto_fullscreen_on_start = false;

        // Font sizes
        float esp_font_size_name = 14.0f;
        float esp_font_size_distance = 13.0f;
        float esp_font_size_reload = 12.0f;
        float esp_font_size_module = 11.0f;

        // Keybinds
        int   xray_toggle_key = 0x58;
        int   key_esp_enable = 0;
        int   key_aimbot_enable = 0;
        int   key_air_lead_enable = 0;
        int   key_3d_box = 0;
        int   key_2d_box = 0;
        int   key_vehicle_name = 0;
        int   key_distance = 0;
        int   key_show_crew = 0;
        int   key_show_barrel = 0;
        int   key_show_ammorack = 0;
        int   key_show_fueltank = 0;

        // Monitor
        int   monitor_index = 0;
        std::string name = "Preset";
    };

    bool menu_open = true;
    int active_preset = 0;
    std::array<menu_preset_t, 5> presets = {};
    std::string configuration_warning;

    struct bool_binding_t { bool menu_preset_t::* preset; bool* runtime; };
    struct float_binding_t { float menu_preset_t::* preset; float* runtime; };
    struct color_binding_t { ImVec4 menu_preset_t::* preset; ImVec4* runtime; };
    struct int_binding_t { int menu_preset_t::* preset; int* runtime; };

    #include "gui/preset_bindings.hpp"

    menu_preset_t capture_current_settings()
    {
        menu_preset_t preset = {};
        for (const auto& b : bool_bindings)   preset.*(b.preset) = *b.runtime;
        for (const auto& f : float_bindings)  preset.*(f.preset) = *f.runtime;
        for (const auto& c : color_bindings)  preset.*(c.preset) = *c.runtime;
        for (const auto& i : int_bindings)    preset.*(i.preset) = *i.runtime;
        preset.monitor_index = sdk::monitor_index;
        preset.missile_overrides = projectile_visibility::capture();
        preset.ui = menu_preferences::current;
        preset.missile_categories=projectile_visibility::capture_category_overrides();
        preset.missile_policy=projectile_visibility::capture_category_policy();
        preset.alerts=missile_alerts::capture_settings();
        preset.radar_kinds = esp::radar::icons::visible;
        preset.name = presets[std::clamp(active_preset,0,static_cast<int>(presets.size())-1)].name;
        return preset;
    }

    void apply_preset(const menu_preset_t& preset)
    {
        for (const auto& b : bool_bindings)   *b.runtime = preset.*(b.preset);
        for (const auto& f : float_bindings)  *f.runtime = preset.*(f.preset);
        for (const auto& c : color_bindings)  *c.runtime = preset.*(c.preset);
        for (const auto& i : int_bindings)    *i.runtime = preset.*(i.preset);
        sdk::monitor_index = clamp_monitor_index(preset.monitor_index, sdk::monitor_count);
        projectile_visibility::apply(preset.missile_overrides);
        menu_preferences::current = preset.ui;
        runtime_language::set_language(menu_preferences::current.language);
        projectile_visibility::apply_category_overrides(preset.missile_categories);
        projectile_visibility::apply_category_policy(preset.missile_policy);
        missile_alerts::apply_settings(preset.alerts);
        esp::radar::icons::visible = preset.radar_kinds;
    }

    void initialize_presets()
    {
        presets[0] = capture_current_settings();
        presets[0].name = "Default";
        for (int i = 1; i < 5; ++i)
        {
            presets[i] = presets[0];
            presets[i].name = "Preset " + std::to_string(i + 1);
        }
        presets[1].aimbot_enabled = true;
        presets[1].name = "Aimbot";
        presets[2].esp_show_boxes = false;
        presets[2].esp_use_3d_boxes = false;
        presets[2].name = "Clean";
        active_preset = 0;
    }

    void sync_shared_settings_to_all_presets()
    {
        const int safe_monitor_index = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
        for (auto& preset : presets)
        {
            preset.monitor_index = safe_monitor_index;
            preset.auto_fullscreen_on_start = sdk::auto_fullscreen_on_start;
        }
    }

    void cycle_preset()
    {
        const std::string active_name = presets[active_preset].name;
        presets[active_preset] = capture_current_settings();
        presets[active_preset].name = active_name;
        sync_shared_settings_to_all_presets();
        active_preset = (active_preset + 1) % static_cast<int>(presets.size());
        apply_preset(presets[active_preset]);
    }

    void load_presets_from_map(const std::unordered_map<std::string, std::string>& values)
    {
        configuration_warning.clear();
        // Restaurar nombres observados de la configuracion guardada manualmente.
        if (auto it=values.find("missile_catalog_count"); it!=values.end()) {
            const int count=std::clamp(config::parse_int(it->second,0),0,
                static_cast<int>(projectile_visibility::max_names));
            std::vector<std::string> names; names.reserve(count);
            for (int j=0;j<count;++j) {
                auto item=values.find("missile_catalog_"+std::to_string(j));
                if (item==values.end()) continue;
                const auto name=projectile_visibility::hex_decode(item->second);
                if (!name.empty()) names.push_back(name);
            }
            projectile_visibility::restore_observed(names);
        }
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            menu_preset_t loaded = presets[i];
            const std::string prefix = "preset" + std::to_string(i + 1) + "_";

            if (auto it = values.find(prefix + "name"); it != values.end())
                loaded.name = it->second.substr(0,63);

            auto read_bool = [&](const char* key, bool& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) target = config::parse_bool(it->second, target);
                };
            auto read_float = [&](const char* key, float& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) target = std::clamp(config::parse_float(it->second, target), 0.0f, 100000.0f);
                };
            auto read_int = [&](const char* key, int& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) target = config::parse_int(it->second, target);
                };
            auto read_color = [&](const char* key, ImVec4& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) config::apply_color(target, it->second);
                };

            read_bool("projectile_esp_enabled", loaded.projectile_esp_enabled);
            read_bool("projectile_debug", loaded.projectile_debug);
            read_bool("projectile_incoming_only", loaded.projectile_incoming_only);
            read_bool("projectile_incoming_use_localplayer", loaded.projectile_incoming_use_localplayer);
            read_bool("projectile_aam_only", loaded.projectile_aam_only);
            loaded.missile_overrides.clear();
            for (std::size_t j=0;j<loaded.radar_kinds.size();++j) {
                loaded.radar_kinds[j]=true;
                if (auto it=values.find(prefix+"radar_kind_"+std::to_string(j)); it!=values.end())
                    loaded.radar_kinds[j]=config::parse_bool(it->second,true);
            }
            if (auto it=values.find(prefix+"missile_count"); it!=values.end()) {
                const int count=std::clamp(config::parse_int(it->second,0),0,
                    static_cast<int>(projectile_visibility::max_names*2));
                for (int j=0;j<count;++j) {
                    auto item=values.find(prefix+"missile_"+std::to_string(j));
                    if (item==values.end()) continue;
                    const auto& encoded=item->second;
                    const auto colon=encoded.find(':');
                    if (colon==std::string::npos || colon+2!=encoded.size() ||
                        (encoded.back()!='0' && encoded.back()!='1')) continue;
                    const auto name=projectile_visibility::hex_decode(
                        std::string_view(encoded).substr(0,colon));
                    if (!name.empty() && !projectile_ecs::name_has_flare(name.c_str()))
                        loaded.missile_overrides[name]=encoded.back()=='1';
                }
            }
            read_float("projectile_incoming_distance", loaded.projectile_incoming_distance);
            read_float("projectile_incoming_angle", loaded.projectile_incoming_angle);
            read_int("projectile_scan_budget", loaded.projectile_scan_budget);

            // ESP
            read_bool("esp_enabled", loaded.esp_enabled);
            read_bool("radar_enabled", loaded.radar_enabled);
            read_bool("aimbot_enabled", loaded.aimbot_enabled);
            read_float("esp_render_distance", loaded.esp_render_distance);
            read_float("esp_box_distance", loaded.esp_box_distance);
            read_bool("esp_show_boxes", loaded.esp_show_boxes);
            read_bool("esp_use_3d_boxes", loaded.esp_use_3d_boxes);
            read_bool("esp_show_vehicle_names", loaded.esp_show_vehicle_names);
            read_bool("esp_show_distance", loaded.esp_show_distance);
            read_bool("esp_show_reload", loaded.esp_show_reload);
            read_bool("esp_show_invulnerable", loaded.esp_show_invulnerable);
            read_bool("esp_invul_show_timer", loaded.esp_invul_show_timer);
            read_float("esp_invul_distance", loaded.esp_invul_distance);
            read_bool("esp_show_reload_seconds", loaded.esp_show_reload_seconds);
            read_bool("esp_show_fps", loaded.esp_show_fps);
            read_bool("esp_show_bomb_impact", loaded.esp_show_bomb_impact);
            read_bool("esp_only_show_planes", loaded.esp_only_show_planes);

            // Vehicle filters
            read_bool("show_helicopter", loaded.show_helicopter);
            read_bool("show_bomber", loaded.show_bomber);
            read_bool("show_fighter", loaded.show_fighter);
            read_bool("show_attacker", loaded.show_attacker);
            read_bool("show_light_tank", loaded.show_light_tank);
            read_bool("show_heavy_tank", loaded.show_heavy_tank);
            read_bool("show_medium_tank", loaded.show_medium_tank);
            read_bool("show_spaa", loaded.show_spaa);
            read_bool("show_gun_boat", loaded.show_gun_boat);
            read_bool("show_destroyer", loaded.show_destroyer);
            read_bool("show_cruiser", loaded.show_cruiser);
            read_bool("show_torpedo_boat", loaded.show_torpedo_boat);
            read_bool("show_recon_drone", loaded.show_recon_drone);

            read_int("death_state_threshold", loaded.death_state_threshold);

            // Vehicle colors
            read_color("color_helicopter", loaded.color_helicopter);
            read_color("color_bomber", loaded.color_bomber);
            read_color("color_fighter", loaded.color_fighter);
            read_color("color_attacker", loaded.color_attacker);
            read_color("color_light_tank", loaded.color_light_tank);
            read_color("color_heavy_tank", loaded.color_heavy_tank);
            read_color("color_medium_tank", loaded.color_medium_tank);
            read_color("color_spaa", loaded.color_spaa);
            read_color("color_gun_boat", loaded.color_gun_boat);
            read_color("color_destroyer", loaded.color_destroyer);
            read_color("color_cruiser", loaded.color_cruiser);
            read_color("color_torpedo_boat", loaded.color_torpedo_boat);
            read_color("color_recon_drone", loaded.color_recon_drone);
            read_color("radar_enemy_color", loaded.radar_enemy_color);
            read_color("color_invulnerable", loaded.color_invulnerable);

            // X-Ray
            read_bool("bShowDamageModel", loaded.bShowDamageModel);
            read_bool("bShowModuleNames", loaded.bShowModuleNames);
            read_bool("bShowModuleGunBarrel", loaded.bShowModuleGunBarrel);
            read_bool("bShowModuleCannonBreech", loaded.bShowModuleCannonBreech);
            read_bool("bShowModuleCrew", loaded.bShowModuleCrew);
            read_bool("bShowModuleFuelTank", loaded.bShowModuleFuelTank);
            read_bool("bShowModuleAmmoRack", loaded.bShowModuleAmmoRack);
            read_bool("bShowModuleEngine", loaded.bShowModuleEngine);
            read_bool("bShowModuleTransmission", loaded.bShowModuleTransmission);
            read_float("xray_small_module_distance", loaded.xray_small_module_distance);
            read_float("xray_line_thickness", loaded.xray_line_thickness);
            read_float("xray_module_alpha", loaded.xray_module_alpha);
            read_float("xray_render_distance", loaded.xray_render_distance);
            read_float("xray_names_distance", loaded.xray_names_distance);
            read_color("color_mod_gun_barrel", loaded.color_mod_gun_barrel);
            read_color("color_mod_cannon_breech", loaded.color_mod_cannon_breech);
            read_color("color_mod_crew", loaded.color_mod_crew);
            read_color("color_mod_fuel_tank", loaded.color_mod_fuel_tank);
            read_color("color_mod_ammo_rack", loaded.color_mod_ammo_rack);
            read_color("color_mod_engine", loaded.color_mod_engine);
            read_color("color_mod_transmission", loaded.color_mod_transmission);
            read_bool("xray_alpha_falloff", loaded.xray_alpha_falloff);

            // Air Lead
            read_bool("air_lead_enabled", loaded.air_lead_enabled);
            read_bool("g2a_lead_enabled", loaded.g2a_lead_enabled);
            read_bool("air_lead_use_simulation", loaded.air_lead_use_simulation);
            read_color("air_lead_sim_color", loaded.air_lead_sim_color);
            read_float("air_lead_opacity", loaded.air_lead_opacity);
            read_bool("air_lead_coverage_opacity", loaded.air_lead_coverage_opacity);
            read_float("menu_ui_scale", loaded.menu_ui_scale);
            read_float("air_lead_marker_size", loaded.air_lead_marker_size);
            read_float("air_lead_line_thickness", loaded.air_lead_line_thickness);
            read_bool("air_lead_use_weapon_position", loaded.air_lead_use_weapon_position);
            read_bool("air_lead_smooth_adaptive", loaded.air_lead_smooth_adaptive);
            read_float("air_lead_smooth_min_alpha", loaded.air_lead_smooth_min_alpha);
            read_float("air_lead_smooth_max_alpha", loaded.air_lead_smooth_max_alpha);
            read_bool("air_lead_use_turn_prediction", loaded.air_lead_use_turn_prediction);
            read_bool("air_lead_target_helicopter", loaded.air_lead_target_helicopter);
            read_bool("air_lead_target_recon_drone", loaded.air_lead_target_recon_drone);
            read_bool("air_lead_target_fighter", loaded.air_lead_target_fighter);
            read_bool("air_lead_target_bomber", loaded.air_lead_target_bomber);
            read_bool("air_lead_target_attacker", loaded.air_lead_target_attacker);
            read_bool("air_lead_only_target", loaded.air_lead_only_target);
            read_float("air_lead_extra_time", loaded.air_lead_extra_time);
            read_int("air_lead_weapon_selection", loaded.air_lead_weapon_selection);
            read_bool("air_lead_use_heaviest_weapon", loaded.air_lead_use_heaviest_weapon);

            // Settings
            read_int("menu_style", loaded.menu_style);
            read_bool("esp_vsync_enabled", loaded.esp_vsync_enabled);
            read_int("esp_vsync_fps", loaded.esp_vsync_fps);

            // Font sizes
            read_float("esp_font_size_name", loaded.esp_font_size_name);
            read_float("esp_font_size_distance", loaded.esp_font_size_distance);
            read_float("esp_font_size_reload", loaded.esp_font_size_reload);
            read_float("esp_font_size_invul", loaded.esp_font_size_invul);
            read_float("esp_font_size_module", loaded.esp_font_size_module);

            loaded.missile_categories.clear();
            int category_count=0;read_int("missile_category_count",category_count);
            for(int j=0;j<std::clamp(category_count,0,16384);++j) {
                const auto it=values.find(prefix+"missile_category_"+std::to_string(j));
                if(it==values.end())continue;
                const auto colon=it->second.find(':');
                if(colon==std::string::npos || colon+2!=it->second.size())continue;
                const int value=it->second.back()-'0';
                const auto name=projectile_visibility::hex_decode(std::string_view(it->second).substr(0,colon));
                if(value>=0 && value<4 && projectile_visibility::category_name_allowed(name))
                    loaded.missile_categories[name]=static_cast<projectile_visibility::category>(value);
            }
            for(std::size_t j=0;j<loaded.missile_policy.size();++j) {
                const auto it=values.find(prefix+"missile_policy_"+std::to_string(j));
                int value=it==values.end()?0:config::parse_int(it->second,0);
                loaded.missile_policy[j]=static_cast<projectile_visibility::category_policy>(value>=0&&value<=2?value:0);
            }
            read_bool("missile_alerts",loaded.alerts.enabled);read_bool("missile_audio",loaded.alerts.audio_enabled);
            const auto alert_number=[&](const char* name,double& value){
                const auto it=values.find(prefix+name);if(it!=values.end())value=config::parse_float(it->second,static_cast<float>(value));
            };
            alert_number("alert_range",loaded.alerts.max_range_m);alert_number("alert_closure",loaded.alerts.min_closure_mps);
            alert_number("alert_time",loaded.alerts.max_time_s);alert_number("alert_miss",loaded.alerts.max_miss_distance_m);
            alert_number("alert_interval",loaded.alerts.audio_interval_s);loaded.alerts=missile_alerts::sanitize(loaded.alerts);
            read_bool("menu_advanced",loaded.ui.advanced);
            loaded.ui.language=1; // Public release default for older profiles.
            loaded.ui.filter_static_air=true; // Older profiles retain the default filter.
            read_int("language",loaded.ui.language);
            loaded.ui.language=menu_preferences::valid_language(loaded.ui.language);
            read_bool("filter_static_air",loaded.ui.filter_static_air);
            read_int("key_g2a",loaded.ui.g2a_key);
            loaded.ui.favorites.clear();
            int favorite_count=0;read_int("favorite_count",favorite_count);
            for(int j=0;j<std::clamp(favorite_count,0,128);++j) {
                const auto it=values.find(prefix+"favorite_"+std::to_string(j));
                if(it==values.end())continue;
                const auto name=projectile_visibility::hex_decode(it->second);
                if(menu_preferences::valid_favorite(name))loaded.ui.favorites.insert(name);
            }
            // Keybinds
            read_int("xray_toggle_key", loaded.xray_toggle_key);
            read_int("key_esp_enable", loaded.key_esp_enable);
            read_int("key_aimbot_enable", loaded.key_aimbot_enable);
            read_int("key_air_lead_enable", loaded.key_air_lead_enable);
            read_int("key_3d_box", loaded.key_3d_box);
            read_int("key_2d_box", loaded.key_2d_box);
            read_int("key_vehicle_name", loaded.key_vehicle_name);
            read_int("key_distance", loaded.key_distance);
            read_int("key_show_crew", loaded.key_show_crew);
            read_int("key_show_barrel", loaded.key_show_barrel);
            read_int("key_show_ammorack", loaded.key_show_ammorack);
            read_int("key_show_fueltank", loaded.key_show_fueltank);
            std::array<int*,13> bindings = {&loaded.ui.g2a_key,&loaded.xray_toggle_key,&loaded.key_esp_enable,
                &loaded.key_aimbot_enable,&loaded.key_air_lead_enable,&loaded.key_3d_box,&loaded.key_2d_box,
                &loaded.key_vehicle_name,&loaded.key_distance,&loaded.key_show_crew,&loaded.key_show_barrel,
                &loaded.key_show_ammorack,&loaded.key_show_fueltank};
            std::array<int,13> keys{};
            for(size_t j=0;j<keys.size();++j) keys[j]=*bindings[j];
            const int repaired=hotkey_validation::sanitize(keys);
            for(size_t j=0;j<keys.size();++j) *bindings[j]=keys[j];
            if(repaired) {
                configuration_warning="Atajos inválidos/duplicados reparados al cargar (F1/F2 reservadas).";
                LOG("[CONFIG] Preset %d: %d atajos invalidos o duplicados desasignados.\n",i+1,repaired);
            }

            // Monitor
            if (const auto it = values.find(prefix + "monitor_index"); it != values.end())
                loaded.monitor_index = config::parse_int(it->second, loaded.monitor_index);

            loaded.esp_render_distance = std::clamp(loaded.esp_render_distance, 1000.0f, 55000.0f);
            loaded.esp_box_distance = std::clamp(loaded.esp_box_distance, 500.0f, 3500.0f);
            loaded.esp_invul_distance = std::clamp(loaded.esp_invul_distance, 100.f, 3500.f);
            loaded.menu_style = std::clamp(loaded.menu_style, 0, 6);
            loaded.esp_vsync_fps = std::clamp(loaded.esp_vsync_fps, 30, 500);
            loaded.death_state_threshold = std::clamp(loaded.death_state_threshold, 1, 2);
            loaded.air_lead_weapon_selection = std::clamp(loaded.air_lead_weapon_selection, -1, 20);
            loaded.projectile_incoming_distance = std::clamp(loaded.projectile_incoming_distance, 500.0f, 8000.0f);
            loaded.projectile_incoming_angle = std::clamp(loaded.projectile_incoming_angle, 5.0f, 60.0f);
            loaded.projectile_scan_budget = std::clamp(loaded.projectile_scan_budget, 256, 4096);
            loaded.air_lead_opacity = std::clamp(loaded.air_lead_opacity, 0.0f, 1.0f);
            loaded.air_lead_marker_size=std::clamp(loaded.air_lead_marker_size,2.f,15.f);
            loaded.air_lead_line_thickness=std::clamp(loaded.air_lead_line_thickness,.5f,3.f);
            loaded.air_lead_smooth_min_alpha=std::clamp(loaded.air_lead_smooth_min_alpha,.05f,.95f);
            loaded.air_lead_smooth_max_alpha=std::clamp(loaded.air_lead_smooth_max_alpha,.05f,.95f);
            if(loaded.air_lead_smooth_min_alpha>loaded.air_lead_smooth_max_alpha)
                std::swap(loaded.air_lead_smooth_min_alpha,loaded.air_lead_smooth_max_alpha);
            loaded.air_lead_extra_time=std::clamp(loaded.air_lead_extra_time,0.f,.5f);
            loaded.xray_small_module_distance=std::clamp(loaded.xray_small_module_distance,50.f,2000.f);
            loaded.xray_line_thickness=std::clamp(loaded.xray_line_thickness,.2f,3.f);
            loaded.xray_module_alpha=std::clamp(loaded.xray_module_alpha,0.f,1.f);
            loaded.xray_render_distance=std::clamp(loaded.xray_render_distance,100.f,5000.f);
            loaded.xray_names_distance=std::clamp(loaded.xray_names_distance,100.f,5000.f);
            loaded.esp_font_size_name=std::clamp(loaded.esp_font_size_name,8.f,36.f);
            loaded.esp_font_size_distance=std::clamp(loaded.esp_font_size_distance,8.f,36.f);
            loaded.esp_font_size_reload=std::clamp(loaded.esp_font_size_reload,8.f,36.f);
            loaded.esp_font_size_invul=std::clamp(loaded.esp_font_size_invul,8.f,18.f);
            loaded.esp_font_size_module=std::clamp(loaded.esp_font_size_module,8.f,36.f);
            loaded.menu_ui_scale = std::clamp(loaded.menu_ui_scale, 0.8f, 1.5f);
            presets[i] = loaded;
        }

        if (const auto it = values.find("active_preset"); it != values.end())
            active_preset = std::clamp(config::parse_int(it->second, active_preset), 0, static_cast<int>(presets.size()) - 1);

        apply_preset(presets[active_preset]);
    }

    void sync_active_preset_from_runtime()
    {
        active_preset = std::clamp(active_preset, 0, static_cast<int>(presets.size()) - 1);
        const std::string name = presets[active_preset].name;
        presets[active_preset] = capture_current_settings();
        presets[active_preset].name = name;
        sync_shared_settings_to_all_presets();
    }

    void append_presets_to_stream(std::ostream& output)
    {
        output << std::setprecision(std::numeric_limits<float>::max_digits10);
        // Serialize the live active profile without modifying the UI/profile state.
        auto saved_presets = presets;
        const int selected = std::clamp(active_preset, 0, static_cast<int>(saved_presets.size()) - 1);
        const std::string active_name = saved_presets[selected].name;
        saved_presets[selected] = capture_current_settings();
        saved_presets[selected].name = active_name;
        const int safe_monitor_index = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
        for (auto& preset : saved_presets)
        {
            preset.monitor_index = safe_monitor_index;
            preset.auto_fullscreen_on_start = sdk::auto_fullscreen_on_start;
        }
        output << "active_preset=" << selected << '\n';
        // Catalogo observado: archivo automatico independiente junto al EXE.
        // Conservamos lectura de configs antiguas para migrarlo sin perder nombres.

        for (int i = 0; i < static_cast<int>(saved_presets.size()); ++i)
        {
            const menu_preset_t& p = saved_presets[i];
            const std::string prefix = "preset" + std::to_string(i + 1) + "_";

            output << prefix << "name=" << p.name << '\n';
            output << prefix << "projectile_esp_enabled=" << (p.projectile_esp_enabled ? 1 : 0) << '\n';
            output << prefix << "projectile_debug=" << (p.projectile_debug ? 1 : 0) << '\n';
            output << prefix << "projectile_incoming_only=" << (p.projectile_incoming_only ? 1 : 0) << '\n';
            output << prefix << "projectile_incoming_use_localplayer=" << (p.projectile_incoming_use_localplayer ? 1 : 0) << '\n';
            output << prefix << "projectile_aam_only=" << (p.projectile_aam_only ? 1 : 0) << '\n';
            for (std::size_t j=0;j<p.radar_kinds.size();++j)
                output << prefix << "radar_kind_" << j << '=' << (p.radar_kinds[j]?1:0) << '\n';
            output << prefix << "missile_count=" << p.missile_overrides.size() << '\n';
            std::size_t missile_index=0;
            for (const auto& [name,enabled] : p.missile_overrides)
                output << prefix << "missile_" << missile_index++ << '='
                    << projectile_visibility::hex_encode(name) << ':' << (enabled?1:0) << '\n';
            output << prefix << "projectile_incoming_distance=" << p.projectile_incoming_distance << '\n';
            output << prefix << "projectile_incoming_angle=" << p.projectile_incoming_angle << '\n';
            output << prefix << "projectile_scan_budget=" << p.projectile_scan_budget << char(10);
            output << prefix << "missile_category_count=" << p.missile_categories.size() << char(10);
            std::size_t category_index=0;
            for(const auto& [name,value]:p.missile_categories)output << prefix << "missile_category_" << category_index++
                << '=' << projectile_visibility::hex_encode(name) << ':' << int(value) << char(10);
            for(std::size_t j=0;j<p.missile_policy.size();++j)output << prefix << "missile_policy_" << j << '=' << int(p.missile_policy[j]) << char(10);
            output << prefix << "missile_alerts=" << int(p.alerts.enabled) << char(10);
            output << prefix << "missile_audio=" << int(p.alerts.audio_enabled) << char(10);
            output << prefix << "alert_range=" << p.alerts.max_range_m << char(10);
            output << prefix << "alert_closure=" << p.alerts.min_closure_mps << char(10);
            output << prefix << "alert_time=" << p.alerts.max_time_s << char(10);
            output << prefix << "alert_miss=" << p.alerts.max_miss_distance_m << char(10);
            output << prefix << "alert_interval=" << p.alerts.audio_interval_s << char(10);
            output << prefix << "menu_advanced=" << int(p.ui.advanced) << char(10);
            output << prefix << "language=" << menu_preferences::valid_language(p.ui.language) << char(10);
            output << prefix << "filter_static_air=" << int(p.ui.filter_static_air) << char(10);
            output << prefix << "key_g2a=" << p.ui.g2a_key << char(10);
            output << prefix << "favorite_count=" << p.ui.favorites.size() << char(10);
            std::size_t favorite_index=0;
            for(const auto& name:p.ui.favorites)output << prefix << "favorite_" << favorite_index++ << '='
                << projectile_visibility::hex_encode(name) << char(10);

            output << prefix << "esp_enabled=" << (p.esp_enabled ? 1 : 0) << '\n';
            output << prefix << "radar_enabled=" << (p.radar_enabled ? 1 : 0) << '\n';
            output << prefix << "aimbot_enabled=" << (p.aimbot_enabled ? 1 : 0) << '\n';
            output << prefix << "esp_render_distance=" << p.esp_render_distance << '\n';
            output << prefix << "esp_box_distance=" << p.esp_box_distance << '\n';
            output << prefix << "esp_show_boxes=" << (p.esp_show_boxes ? 1 : 0) << '\n';
            output << prefix << "esp_use_3d_boxes=" << (p.esp_use_3d_boxes ? 1 : 0) << '\n';
            output << prefix << "esp_show_vehicle_names=" << (p.esp_show_vehicle_names ? 1 : 0) << '\n';
            output << prefix << "esp_show_distance=" << (p.esp_show_distance ? 1 : 0) << '\n';
            output << prefix << "esp_show_reload=" << (p.esp_show_reload ? 1 : 0) << '\n';
            output << prefix << "esp_show_invulnerable=" << (p.esp_show_invulnerable ? 1 : 0) << '\n';
            output << prefix << "esp_invul_show_timer=" << (p.esp_invul_show_timer ? 1 : 0) << '\n';
            output << prefix << "esp_invul_distance=" << p.esp_invul_distance << '\n';
            output << prefix << "esp_show_reload_seconds=" << (p.esp_show_reload_seconds ? 1 : 0) << '\n';
            output << prefix << "esp_show_fps=" << (p.esp_show_fps ? 1 : 0) << '\n';
            output << prefix << "esp_show_bomb_impact=" << (p.esp_show_bomb_impact ? 1 : 0) << '\n';
            output << prefix << "esp_only_show_planes=" << (p.esp_only_show_planes ? 1 : 0) << '\n';

            output << prefix << "show_helicopter=" << (p.show_helicopter ? 1 : 0) << '\n';
            output << prefix << "show_bomber=" << (p.show_bomber ? 1 : 0) << '\n';
            output << prefix << "show_fighter=" << (p.show_fighter ? 1 : 0) << '\n';
            output << prefix << "show_attacker=" << (p.show_attacker ? 1 : 0) << '\n';
            output << prefix << "show_light_tank=" << (p.show_light_tank ? 1 : 0) << '\n';
            output << prefix << "show_heavy_tank=" << (p.show_heavy_tank ? 1 : 0) << '\n';
            output << prefix << "show_medium_tank=" << (p.show_medium_tank ? 1 : 0) << '\n';
            output << prefix << "show_spaa=" << (p.show_spaa ? 1 : 0) << '\n';
            output << prefix << "show_gun_boat=" << (p.show_gun_boat ? 1 : 0) << '\n';
            output << prefix << "show_destroyer=" << (p.show_destroyer ? 1 : 0) << '\n';
            output << prefix << "show_cruiser=" << (p.show_cruiser ? 1 : 0) << '\n';
            output << prefix << "show_torpedo_boat=" << (p.show_torpedo_boat ? 1 : 0) << '\n';
            output << prefix << "show_recon_drone=" << (p.show_recon_drone ? 1 : 0) << '\n';

            output << prefix << "death_state_threshold=" << p.death_state_threshold << '\n';

            auto write_color = [&](const char* key, const ImVec4& col) {
                output << prefix << key << "=" << config::color_to_string(col) << '\n';
                };
            write_color("color_helicopter", p.color_helicopter);
            write_color("color_bomber", p.color_bomber);
            write_color("color_fighter", p.color_fighter);
            write_color("color_attacker", p.color_attacker);
            write_color("color_light_tank", p.color_light_tank);
            write_color("color_heavy_tank", p.color_heavy_tank);
            write_color("color_medium_tank", p.color_medium_tank);
            write_color("color_spaa", p.color_spaa);
            write_color("color_gun_boat", p.color_gun_boat);
            write_color("color_destroyer", p.color_destroyer);
            write_color("color_cruiser", p.color_cruiser);
            write_color("color_torpedo_boat", p.color_torpedo_boat);
            write_color("color_recon_drone", p.color_recon_drone);
            write_color("radar_enemy_color", p.radar_enemy_color);
            write_color("color_invulnerable", p.color_invulnerable);

            output << prefix << "bShowDamageModel=" << (p.bShowDamageModel ? 1 : 0) << '\n';
            output << prefix << "bShowModuleNames=" << (p.bShowModuleNames ? 1 : 0) << '\n';
            output << prefix << "bShowModuleGunBarrel=" << (p.bShowModuleGunBarrel ? 1 : 0) << '\n';
            output << prefix << "bShowModuleCannonBreech=" << (p.bShowModuleCannonBreech ? 1 : 0) << '\n';
            output << prefix << "bShowModuleCrew=" << (p.bShowModuleCrew ? 1 : 0) << '\n';
            output << prefix << "bShowModuleFuelTank=" << (p.bShowModuleFuelTank ? 1 : 0) << '\n';
            output << prefix << "bShowModuleAmmoRack=" << (p.bShowModuleAmmoRack ? 1 : 0) << '\n';
            output << prefix << "bShowModuleEngine=" << (p.bShowModuleEngine ? 1 : 0) << '\n';
            output << prefix << "bShowModuleTransmission=" << (p.bShowModuleTransmission ? 1 : 0) << '\n';
            output << prefix << "xray_small_module_distance=" << p.xray_small_module_distance << '\n';
            output << prefix << "xray_line_thickness=" << p.xray_line_thickness << '\n';
            output << prefix << "xray_module_alpha=" << p.xray_module_alpha << '\n';
            output << prefix << "xray_render_distance=" << p.xray_render_distance << '\n';
            output << prefix << "xray_names_distance=" << p.xray_names_distance << '\n';
            write_color("color_mod_gun_barrel", p.color_mod_gun_barrel);
            write_color("color_mod_cannon_breech", p.color_mod_cannon_breech);
            write_color("color_mod_crew", p.color_mod_crew);
            write_color("color_mod_fuel_tank", p.color_mod_fuel_tank);
            write_color("color_mod_ammo_rack", p.color_mod_ammo_rack);
            write_color("color_mod_engine", p.color_mod_engine);
            write_color("color_mod_transmission", p.color_mod_transmission);
            output << prefix << "xray_alpha_falloff=" << (p.xray_alpha_falloff ? 1 : 0) << '\n';

            output << prefix << "air_lead_enabled=" << (p.air_lead_enabled ? 1 : 0) << '\n';
            output << prefix << "g2a_lead_enabled=" << (p.g2a_lead_enabled ? 1 : 0) << '\n';
            output << prefix << "air_lead_use_simulation=" << (p.air_lead_use_simulation ? 1 : 0) << '\n';
            write_color("air_lead_sim_color", p.air_lead_sim_color);
            output << prefix << "air_lead_opacity=" << p.air_lead_opacity << '\n';
            output << prefix << "air_lead_coverage_opacity=" << (p.air_lead_coverage_opacity ? 1 : 0) << '\n';
            output << prefix << "menu_ui_scale=" << p.menu_ui_scale << '\n';
            output << prefix << "air_lead_marker_size=" << p.air_lead_marker_size << '\n';
            output << prefix << "air_lead_line_thickness=" << p.air_lead_line_thickness << '\n';
            output << prefix << "air_lead_use_weapon_position=" << (p.air_lead_use_weapon_position ? 1 : 0) << '\n';
            output << prefix << "air_lead_smooth_adaptive=" << (p.air_lead_smooth_adaptive ? 1 : 0) << '\n';
            output << prefix << "air_lead_smooth_min_alpha=" << p.air_lead_smooth_min_alpha << '\n';
            output << prefix << "air_lead_smooth_max_alpha=" << p.air_lead_smooth_max_alpha << '\n';
            output << prefix << "air_lead_use_turn_prediction=" << (p.air_lead_use_turn_prediction ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_helicopter=" << (p.air_lead_target_helicopter ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_recon_drone=" << (p.air_lead_target_recon_drone ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_fighter=" << (p.air_lead_target_fighter ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_bomber=" << (p.air_lead_target_bomber ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_attacker=" << (p.air_lead_target_attacker ? 1 : 0) << '\n';
            output << prefix << "air_lead_only_target=" << (p.air_lead_only_target ? 1 : 0) << '\n';
            output << prefix << "air_lead_extra_time=" << p.air_lead_extra_time << '\n';
            output << prefix << "air_lead_weapon_selection=" << p.air_lead_weapon_selection << '\n';
            output << prefix << "air_lead_use_heaviest_weapon=" << (p.air_lead_use_heaviest_weapon ? 1 : 0) << '\n';

            output << prefix << "menu_style=" << p.menu_style << '\n';
            output << prefix << "esp_vsync_enabled=" << (p.esp_vsync_enabled ? 1 : 0) << '\n';
            output << prefix << "esp_vsync_fps=" << p.esp_vsync_fps << '\n';

            output << prefix << "esp_font_size_name=" << p.esp_font_size_name << '\n';
            output << prefix << "esp_font_size_distance=" << p.esp_font_size_distance << '\n';
            output << prefix << "esp_font_size_reload=" << p.esp_font_size_reload << '\n';
            output << prefix << "esp_font_size_invul=" << p.esp_font_size_invul << '\n';
            output << prefix << "esp_font_size_module=" << p.esp_font_size_module << '\n';

            output << prefix << "xray_toggle_key=" << p.xray_toggle_key << '\n';
            output << prefix << "key_esp_enable=" << p.key_esp_enable << '\n';
            output << prefix << "key_aimbot_enable=" << p.key_aimbot_enable << '\n';
            output << prefix << "key_air_lead_enable=" << p.key_air_lead_enable << '\n';
            output << prefix << "key_3d_box=" << p.key_3d_box << '\n';
            output << prefix << "key_2d_box=" << p.key_2d_box << '\n';
            output << prefix << "key_vehicle_name=" << p.key_vehicle_name << '\n';
            output << prefix << "key_distance=" << p.key_distance << '\n';
            output << prefix << "key_show_crew=" << p.key_show_crew << '\n';
            output << prefix << "key_show_barrel=" << p.key_show_barrel << '\n';
            output << prefix << "key_show_ammorack=" << p.key_show_ammorack << '\n';
            output << prefix << "key_show_fueltank=" << p.key_show_fueltank << '\n';

            output << prefix << "monitor_index=" << p.monitor_index << '\n';
        }
    }

    struct settings_snapshot_t
    {
        menu_preset_t current;
        std::array<menu_preset_t, 5> saved_presets;
        int selected = 0;
        int entity_interval = 0;
        int frame_interval = 0;
    };

    bool same_alerts(const missile_alerts::Settings& a,const missile_alerts::Settings& b) {
        return a.enabled==b.enabled && a.audio_enabled==b.audio_enabled && a.max_range_m==b.max_range_m &&
            a.min_closure_mps==b.min_closure_mps && a.max_time_s==b.max_time_s &&
            a.max_miss_distance_m==b.max_miss_distance_m && a.audio_interval_s==b.audio_interval_s;
    }
    bool same_preset(const menu_preset_t& a, const menu_preset_t& b)
    {
        for (const auto& binding : bool_bindings)
            if (a.*(binding.preset) != b.*(binding.preset)) return false;
        for (const auto& binding : float_bindings)
            if (a.*(binding.preset) != b.*(binding.preset)) return false;
        for (const auto& binding : int_bindings)
            if (a.*(binding.preset) != b.*(binding.preset)) return false;
        for (const auto& binding : color_bindings)
        {
            const ImVec4& x = a.*(binding.preset);
            const ImVec4& y = b.*(binding.preset);
            if (x.x != y.x || x.y != y.y || x.z != y.z || x.w != y.w) return false;
        }
        return a.monitor_index == b.monitor_index && a.name == b.name &&
            a.missile_overrides == b.missile_overrides && a.radar_kinds == b.radar_kinds && a.ui==b.ui &&
            a.missile_categories==b.missile_categories && a.missile_policy==b.missile_policy && same_alerts(a.alerts,b.alerts);
    }

    settings_snapshot_t snapshot_settings()
    {
        return { capture_current_settings(), presets, active_preset,
            core::entity_update_interval_ms.load(), core::frame_update_interval_ms.load() };
    }

    bool same_settings(const settings_snapshot_t& a, const settings_snapshot_t& b)
    {
        if (a.entity_interval != b.entity_interval ||
            a.frame_interval != b.frame_interval) return false;
        if (a.selected != b.selected || !same_preset(a.current, b.current)) return false;
        for (size_t i = 0; i < a.saved_presets.size(); ++i)
            if (!same_preset(a.saved_presets[i], b.saved_presets[i])) return false;
        return true;
    }

    enum class save_status_t { saved, pending, error };
    save_status_t save_status = save_status_t::saved;
    settings_snapshot_t last_saved;
    bool save_tracking_ready = false;

    void initialize_save_state()
    {
        last_saved = snapshot_settings();
        save_tracking_ready = true;
        save_status = save_status_t::saved;
    }

    bool save_now()
    {
        sync_active_preset_from_runtime();
        const bool saved = config::save();
        if (saved)
        {
            last_saved = snapshot_settings();
            save_status = save_status_t::saved;
        }
        else
        {
            save_status = save_status_t::error;
        }
        return saved;
    }

    void update_unsaved_indicator()
    {
        if (!save_tracking_ready || save_status == save_status_t::error) return;
        // snapshot_settings copia configuración/colecciones: no hacerlo a cada
        // frame cuando el menú está oculto. Con menú visible sigue inmediato.
        static auto next_hidden_check=std::chrono::steady_clock::time_point{};
        if (!menu_open) {
            const auto now=std::chrono::steady_clock::now();
            if (now<next_hidden_check) return;
            next_hidden_check=now+std::chrono::milliseconds(100);
        } else next_hidden_check={};
        save_status = same_settings(snapshot_settings(), last_saved)
            ? save_status_t::saved : save_status_t::pending;
    }

    bool ExportConfig(const std::string& filepath)
    {
        std::ofstream out(filepath, std::ios::trunc);
        if (!out.is_open()) return false;
        out << "// War Thunder DMA Cheat Configuration\n";
        out << "// Version 2.3.2+\n\n";
        append_presets_to_stream(out);
        out.close();
        return true;
    }

    bool ImportConfig(const std::string& filepath)
    {
        std::ifstream in(filepath);
        if (!in.is_open()) return false;
        std::unordered_map<std::string, std::string> values;
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '/' || line[0] == ' ') continue;
            auto pos = line.find('=');
            if (pos != std::string::npos)
                values[line.substr(0, pos)] = line.substr(pos + 1);
        }
        if (!values.contains("active_preset") || !values.contains("preset1_esp_enabled")) {
            configuration_warning="Importación rechazada: faltan datos esenciales del perfil.";
            return false;
        }
        load_presets_from_map(values);
        apply_preset(presets[active_preset]);
        // Importar sólo cambia la sesión; Guardar persiste los cambios.
        return true;
    }

    void revert_unsaved_settings() {
        if(!save_tracking_ready)return;
        presets=last_saved.saved_presets;active_preset=last_saved.selected;
        apply_preset(last_saved.current);
        core::entity_update_interval_ms.store(last_saved.entity_interval);
        core::frame_update_interval_ms.store(last_saved.frame_interval);
        save_status=save_status_t::saved;
    }
    quality_diagnostics::diagnostics<512> quality; // render thread only
    #include "gui/tactical_menu.hpp"
}

// ====================== main() and helpers ======================
int main(int, char**)
{
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);

    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SDK - ", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"SDK - ", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 720, nullptr, nullptr, wc.hInstance, nullptr);
    GUI::initialize_presets();
    UpdateCurrentMonitor(hwnd);
    missile_registry_storage::load(); // solo nombres, nunca modifica la configuracion
    config::load();
    offset_auto_update::startup(); // before DMA workers: no live-offset races
    GUI::menu_open = true;
    GUI::initialize_save_state();

    if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); ::UnregisterClassW(wc.lpszClassName, wc.hInstance); return TRUE; }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    if (sdk::auto_fullscreen_on_start) ApplyFullscreenToMonitor(hwnd, sdk::monitor_index);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // El overlay sigue en la swap chain transparente DirectComposition.
    // El menu usa una HWND ImGui opaca independiente para salir a otro monitor.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad |
        ImGuiConfigFlags_DockingEnable;
    io.ConfigViewportsNoAutoMerge = true; // ventana separada aun sobre el overlay
    io.ConfigViewportsNoTaskBarIcon = true;
    io.IniFilename = nullptr; // sin grabar configuracion implicitamente en imgui.ini

    ApplyMenuStyle(sdk::menu_style);
    // Separate menu font: leave the overlay/render fonts unchanged.
    io.Fonts->AddFontDefault();
    wchar_t windows_dir[MAX_PATH] = {};
    if (GetWindowsDirectoryW(windows_dir, MAX_PATH)) {
        const auto fonts_dir = std::filesystem::path(windows_dir) / "Fonts";
        // Prefer a readable UI sans-serif; use Bahnschrift when Segoe UI is unavailable.
        for (const char* name : { "segoeui.ttf", "bahnschrift.ttf", "seguisb.ttf" }) {
            const auto font_path = fonts_dir / name;
            if (std::filesystem::exists(font_path) &&
                (GUI::tactical_font = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), 16.0f)))
                break;
        }
    }
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    g_render->init();
    if (!core::Thread()) return FALSE;

    projectile_esp::clear_cache();
    bool done = false;
    while (!done)
    {
        const auto frame_start = std::chrono::steady_clock::now();
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        const bool is_foreground_process = []() {
            const HWND fg = ::GetForegroundWindow();
            if (!fg) return false;
            DWORD pid; ::GetWindowThreadProcessId(fg, &pid);
            return pid == ::GetCurrentProcessId();
            }();

        const bool editing_menu = GUI::menu_open && (GUI::tactical_capture != 0 || ImGui::GetIO().WantTextInput ||
            ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup));
        const bool f1_down_now = (::GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (is_foreground_process && !editing_menu && f1_down_now && !g_F1DownLastFrame) GUI::menu_open = !GUI::menu_open;
        g_F1DownLastFrame = f1_down_now;

        const bool f2_down_now = (::GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (is_foreground_process && !editing_menu && f2_down_now && !g_F2DownLastFrame) { GUI::cycle_preset(); }
        g_F2DownLastFrame = f2_down_now;

        if (is_foreground_process)
        {
            auto check_toggle = [editing_menu](int key, bool& var, bool& last_state) {
                bool down = (::GetAsyncKeyState(key) & 0x8000) != 0;
                if (!editing_menu && down && !last_state) var = !var;
                last_state = down;
                };
            static bool last_esp = false, last_aimbot = false, last_air_lead = false,
                last_3dbox = false, last_2dbox = false, last_vname = false,
                last_dist = false, last_crew = false, last_barrel = false,
                last_ammorack = false, last_fueltank = false;
            check_toggle(sdk::key_esp_enable, sdk::esp_enabled, last_esp);
            check_toggle(sdk::key_aimbot_enable, sdk::aimbot_enabled, last_aimbot);
            check_toggle(sdk::key_air_lead_enable, sdk::air_lead_enabled, last_air_lead);
            static bool last_g2a=false;
            check_toggle(menu_preferences::current.g2a_key,esp::g2a_lead::enabled,last_g2a);
            check_toggle(sdk::key_3d_box, sdk::esp_use_3d_boxes, last_3dbox);
            check_toggle(sdk::key_2d_box, sdk::esp_show_boxes, last_2dbox);
            check_toggle(sdk::key_vehicle_name, sdk::esp_show_vehicle_names, last_vname);
            check_toggle(sdk::key_distance, sdk::esp_show_distance, last_dist);
            check_toggle(sdk::key_show_crew, sdk::bShowModuleCrew, last_crew);
            check_toggle(sdk::key_show_barrel, sdk::bShowModuleGunBarrel, last_barrel);
            check_toggle(sdk::key_show_ammorack, sdk::bShowModuleAmmoRack, last_ammorack);
            check_toggle(sdk::key_show_fueltank, sdk::bShowModuleFuelTank, last_fueltank);
        }

        static bool xray_key_down_last = false;
        bool xray_key_down_now = (::GetAsyncKeyState(sdk::xray_toggle_key) & 0x8000) != 0;
        if (is_foreground_process && !editing_menu && xray_key_down_now && !xray_key_down_last) sdk::bShowDamageModel = !sdk::bShowDamageModel;
        xray_key_down_last = xray_key_down_now;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) { ::Sleep(10); continue; }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            // A flip-model swap chain cannot resize while its back buffer is
            // still bound to the immediate context.
            g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
            CleanupRenderTarget();
            const HRESULT resize_hr = g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(resize_hr))
                LOG("[D3D] ResizeBuffers failed: 0x%08X\n", static_cast<unsigned int>(resize_hr));
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }
        if (!g_mainRenderTargetView) { ::Sleep(10); continue; }

        // Opt-in: ImGui admite activar/desactivar viewports entre frames.
        if (sdk::menu_multimonitor_enabled) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        else io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImVec2 display_size = io.DisplaySize;
        if (display_size.x > 0.0f && display_size.y > 0.0f) { sdk::screen_width = (int)display_size.x; sdk::screen_height = (int)display_size.y; }

        core::MaintainConnection(false);
        const int clamped = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
        if (clamped != sdk::monitor_index) sdk::monitor_index = clamped;
        if (sdk::request_monitor_apply) { ApplyFullscreenToMonitor(hwnd, sdk::monitor_index); sdk::request_monitor_apply = false; }
        if (sdk::request_manual_reload) { if (core::RestartRuntime()) sdk::manual_reload_status = "OK"; else if (sdk::manual_reload_status.empty()) sdk::manual_reload_status = "Failed"; sdk::request_manual_reload = false; }

        g_render->draw_list() = ImGui::GetBackgroundDrawList();
        GUI::OnRender();
        GUI::update_unsaved_indicator();
        uint64_t render_snapshot_age_us = 0, render_snapshot_id = 0;
        if (g_render->draw_list())
        {
            const auto work=render_work_policy::snapshot_demand(core::IsConnected(),
                sdk::esp_enabled,sdk::projectile_esp_enabled);
            std::shared_ptr<const misc::frame_snapshot_t> projectile_frame;
            if(work.frame) {
                misc::UpdateFrameSnapshot(work.unit_positions);
                projectile_frame=misc::snapshot_frame();
            } else {
                misc::prof_update_frame_us.store(0,std::memory_order_relaxed);
            }
            if (projectile_frame) {
                render_snapshot_id = projectile_frame->frame_id;
                const auto age = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - projectile_frame->captured_at).count();
                if (age > 0) render_snapshot_age_us = static_cast<uint64_t>(age);
            }
            misc::prof_snapshot_age_us.store(render_snapshot_age_us, std::memory_order_relaxed);
            misc::prof_snapshot_valid.store(projectile_frame && projectile_frame->frame.valid,std::memory_order_relaxed);
            esp::run(projectile_frame);
            if (sdk::projectile_esp_enabled && core::IsConnected() && projectile_frame && projectile_frame->frame.valid)
            {
                misc::scoped_profile_t projectile_profile(misc::prof_projectile_us);
                projectile_esp::draw(g_render->draw_list(), projectile_frame->frame.camera_matrix,
                    ImGui::GetIO().DisplaySize, &projectile_frame->frame.local_pos);
            }
            else
                projectile_esp::clear_cache();

            // HUD independiente del FPS: el perfil activo se refleja tambien al pulsar F2.
            if (sdk::esp_enabled) {
                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImFont* font = g_render->fonts().m_esp;
                if (viewport && font && viewport->Size.x > 0.0f && viewport->Size.y > 0.0f) {
                    const int selected = std::clamp(GUI::active_preset, 0,
                        static_cast<int>(GUI::presets.size()) - 1);
                    static int cached_profile=-1;
                    static std::string cached_name, label;
                    static ImFont* cached_font=nullptr;
                    static float cached_font_size=0.0f,cached_viewport_width=0.0f;
                    static ImVec2 text_size{};
                    const auto& name=GUI::presets[selected].name;
                    if (selected!=cached_profile || name!=cached_name || font!=cached_font ||
                        font->LegacySize!=cached_font_size || viewport->Size.x!=cached_viewport_width) {
                        cached_profile=selected;cached_name=name;cached_font=font;
                        cached_font_size=font->LegacySize;cached_viewport_width=viewport->Size.x;
                        label="Perfil: "+name;
                        text_size=font->CalcTextSizeA(font->LegacySize,
                            viewport->Size.x,0.0f,label.c_str());
                    }
                    const auto top_right = overlay_profile_position::top_right(
                        viewport->Pos.x, viewport->Pos.y, viewport->Size.x, text_size.x);
                    g_render->draw_list()->PushClipRect(viewport->Pos,
                        ImVec2(viewport->Pos.x + viewport->Size.x,
                               viewport->Pos.y + viewport->Size.y), true);
                    g_render->draw_list()->AddText(font, font->LegacySize,
                        ImVec2(top_right.x, top_right.y), IM_COL32(255, 255, 255, 255),
                        label.c_str());
                    g_render->draw_list()->PopClipRect();
                }
            }
        }
        else
            projectile_esp::clear_cache();
        missile_registry_storage::flush(); // una escritura por frame con nombres nuevos

        ImGui::Render();
        const float clear_color[4] = { 0,0,0,0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        // Render del menu separado; el ESP siempre queda en la ventana principal.
        ImGui::UpdatePlatformWindows(); // cierra HWND extra al desactivar
        if (sdk::menu_multimonitor_enabled) ImGui::RenderPlatformWindowsDefault();

        static auto next_vsync = std::chrono::steady_clock::now();
        HRESULT hr = S_OK;
        if (sdk::esp_vsync_enabled) {
            int target = std::clamp(sdk::esp_vsync_fps, 30, 500);
            auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / target));
            next_vsync += period; auto now = std::chrono::steady_clock::now();
            if (next_vsync > now) std::this_thread::sleep_until(next_vsync); else next_vsync = now;
            hr = g_pSwapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING) hr = S_OK;
            else if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) hr = S_OK;
        }
        else {
            next_vsync = std::chrono::steady_clock::now();
            hr = g_pSwapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING) hr = S_OK;
            else if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) hr = g_pSwapChain->Present(0, 0);
        }
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

        auto frame_end = std::chrono::steady_clock::now();
        const auto elapsed_us = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start).count();
        misc::prof_main_frame_us.store(elapsed_us, std::memory_order_relaxed);
        misc::prof_main_frame_count.fetch_add(1, std::memory_order_relaxed);
        misc::record_performance_frame(elapsed_us, render_snapshot_age_us, render_snapshot_id);
        GUI::quality.frame_ms.add_ms(static_cast<double>(elapsed_us)/1000.0);
        if(render_snapshot_age_us) GUI::quality.esp_ms.add_ms(static_cast<double>(misc::prof_esp_us.load())/1000.0);
    }
    missile_registry_storage::flush(); // ultimo intento, no guarda preferencias
    core::Shutdown();
    ImGui::DestroyPlatformWindows();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D(); ::DestroyWindow(hwnd); ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return FALSE;
}

// ====================== D3D / monitor / WndProc ======================
bool CreateDeviceD3D(HWND hWnd)
{
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    // An HWND swap chain discards alpha: translucent strokes darken against
    // its black clear color. A premultiplied DirectComposition surface lets
    // Windows blend the overlay with the actual desktop/game beneath it.
    constexpr UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags,
        levels, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr))
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, device_flags,
            levels, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr))
    {
        LOG("[D3D] CreateDevice failed: 0x%08X\n", static_cast<unsigned int>(hr));
        CleanupDeviceD3D();
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(dxgi_device.GetAddressOf()));
    if (SUCCEEDED(hr)) hr = dxgi_device->GetAdapter(adapter.GetAddressOf());
    if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr))
    {
        LOG("[D3D] DXGI factory failed: 0x%08X\n", static_cast<unsigned int>(hr));
        CleanupDeviceD3D();
        return false;
    }

    RECT client{};
    ::GetClientRect(hWnd, &client);
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(std::max<LONG>(1, client.right - client.left));
    desc.Height = static_cast<UINT>(std::max<LONG>(1, client.bottom - client.top));
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    hr = factory->CreateSwapChainForComposition(g_pd3dDevice, &desc, nullptr, &g_pSwapChain);
    if (SUCCEEDED(hr)) hr = DCompositionCreateDevice(dxgi_device.Get(), IID_PPV_ARGS(&g_compositionDevice));
    if (SUCCEEDED(hr)) hr = g_compositionDevice->CreateTargetForHwnd(hWnd, TRUE, &g_compositionTarget);
    if (SUCCEEDED(hr)) hr = g_compositionDevice->CreateVisual(&g_compositionVisual);
    if (SUCCEEDED(hr)) hr = g_compositionVisual->SetContent(g_pSwapChain);
    if (SUCCEEDED(hr)) hr = g_compositionTarget->SetRoot(g_compositionVisual);
    if (SUCCEEDED(hr)) hr = g_compositionDevice->Commit();
    if (FAILED(hr))
    {
        LOG("[D3D] Transparent composition failed: 0x%08X\n", static_cast<unsigned int>(hr));
        CleanupDeviceD3D();
        return false;
    }

    CreateRenderTarget();
    if (!g_mainRenderTargetView)
    {
        CleanupDeviceD3D();
        return false;
    }
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_compositionTarget) { g_compositionTarget->SetRoot(nullptr); g_compositionTarget->Release(); g_compositionTarget = nullptr; }
    if (g_compositionVisual) { g_compositionVisual->Release(); g_compositionVisual = nullptr; }
    if (g_compositionDevice) { g_compositionDevice->Commit(); g_compositionDevice->Release(); g_compositionDevice = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    const HRESULT get_buffer_hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(get_buffer_hr) || !pBackBuffer)
    {
        LOG("[D3D] GetBuffer failed: 0x%08X\n", static_cast<unsigned int>(get_buffer_hr));
        g_mainRenderTargetView = nullptr;
        return;
    }

    const HRESULT create_rtv_hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    if (FAILED(create_rtv_hr))
    {
        LOG("[D3D] CreateRenderTargetView failed: 0x%08X\n", static_cast<unsigned int>(create_rtv_hr));
        g_mainRenderTargetView = nullptr;
    }

    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

int clamp_monitor_index(int value, int count)
{
    if (count <= 0) return 0;
    if (value < 0)  return 0;
    if (value > count - 1) return count - 1;
    return value;
}

struct monitor_apply_t
{
    int current_index = -1;
    int target_index = 0;
    bool found = false;
    RECT rc = {};
};

BOOL CALLBACK collect_monitor_for_index(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    monitor_apply_t* result = reinterpret_cast<monitor_apply_t*>(data);
    if (!result) return FALSE;

    result->current_index++;
    if (result->current_index != result->target_index) return TRUE;

    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) return FALSE;

    result->rc = monitor_info.rcMonitor;
    result->found = true;
    return FALSE;
}

struct monitor_list_state_t
{
    int count = 0;
    int current_index = 0;
    HMONITOR current_monitor = nullptr;
    std::vector<std::string> labels = {};
};

BOOL CALLBACK collect_monitor_list_callback(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    monitor_list_state_t* state = reinterpret_cast<monitor_list_state_t*>(data);
    if (!state) return FALSE;

    MONITORINFOEXW monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) return TRUE;

    const RECT& rc = monitor_info.rcMonitor;
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    char device_name[CCHDEVICENAME] = {};
    ::WideCharToMultiByte(CP_UTF8, 0, monitor_info.szDevice, -1, device_name, static_cast<int>(sizeof(device_name)), nullptr, nullptr);

    std::ostringstream label;
    label << device_name << " (" << width << "x" << height << ")";
    state->labels.emplace_back(label.str());

    if (monitor == state->current_monitor)
        state->current_index = state->count;

    state->count++;
    return TRUE;
}

void UpdateCurrentMonitor(HWND hWnd)
{
    monitor_list_state_t state = {};
    state.current_monitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    ::EnumDisplayMonitors(nullptr, nullptr, collect_monitor_list_callback, reinterpret_cast<LPARAM>(&state));

    sdk::monitor_count = std::max(state.count, 1);
    if (!state.labels.empty())
        sdk::monitor_labels = std::move(state.labels);
    else
        sdk::monitor_labels = { "Unknown monitor" };

    sdk::monitor_index = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
    g_CurrentMonitor = state.current_monitor;
}

void ApplyFullscreenToMonitor(HWND hWnd, int index)
{
    sdk::monitor_index = index;

    monitor_apply_t data = {};
    data.target_index = sdk::monitor_index;
    ::EnumDisplayMonitors(nullptr, nullptr, collect_monitor_for_index, reinterpret_cast<LPARAM>(&data));
    if (!data.found) return;

    const RECT& rc = data.rc;
    ::SetWindowLongPtrW(hWnd, GWL_STYLE, WS_POPUP);
    ::SetWindowLongPtrW(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP);
    ::SetWindowPos(hWnd, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    g_ResizeWidth = static_cast<UINT>(rc.right - rc.left);
    g_ResizeHeight = static_cast<UINT>(rc.bottom - rc.top);
    UpdateCurrentMonitor(hWnd);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_MOVE:
    case WM_DISPLAYCHANGE:
        UpdateCurrentMonitor(hWnd);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
