#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
#include <array>
static std::string read(const std::filesystem::path& p) {
    std::ifstream f(p); assert(f); return {std::istreambuf_iterator<char>(f),{}};
}
int main() {
    const auto src=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    const auto menu=read(src/"gui/tactical_menu.hpp"),main=read(src/"main.cpp");
    const auto save=menu.find(R"needle(if(TacticalUI::SmallButton("Guardar configura)needle");
    const auto reload=menu.find(R"needle(if(TacticalUI::SmallButton("Recargar SDK")))needle");
    assert(save!=std::string::npos && reload>save && reload-save<250);
    assert(menu.find(R"needle(TacticalUI::Button("Recargar SDK"))needle")==std::string::npos);
    for(auto text:{"Seleccionar todos los misiles","Misiles Aire-Aire","Misiles Aire-Tierra","Misiles Tierra-Aire","Sin clasificar / multiuso","Marcar resultados","Desmarcar resultados","Aim Tierra-Aire (G2A)"})
        assert(menu.find(text)!=std::string::npos);
    assert(menu.find("sdk::projectile_aam_only=false;")!=std::string::npos);
    assert(menu.find("projectile_visibility::set_all(true)")!=std::string::npos);
    assert(menu.find("missile_categories[i]==missile_category")!=std::string::npos);
    assert(menu.find("listed_category!=missile_category")!=std::string::npos);
    assert(menu.find("missile_name_search.PassFilter")!=std::string::npos);
    assert(main.find(R"needle(read_bool("g2a_lead_enabled", loaded.g2a_lead_enabled))needle")!=std::string::npos);
    assert(main.find(R"needle("g2a_lead_enabled=")needle")!=std::string::npos);
    const auto bindings=read(src/"gui/preset_bindings.hpp");
    assert(bindings.find("&menu_preset_t::g2a_lead_enabled, &esp::g2a_lead::enabled")!=std::string::npos);
    const auto esp=read(src/"features/esp/esp.hpp");
    for(auto text:{"|| process_g2a)","classify_unit_type(g2a_local_type)==UnitClass::Ground",
        "frame.gui_state==GuiState::ALIVE","if(!process_g2a || !best_g2a_base)",
        "aimbot::ReadGroundVelocity(local_unit_base, own_velocity, source)",
        "get_air_container(best_g2a_base, now)", "g2a_cache.ammunition = round"})
        assert(esp.find(text)!=std::string::npos);
    for(auto text:{"Alerta visual de aproximacion","Sonido de alerta","Vista avanzada","Solo favoritos",
        "Cambios / Revertir","Clic derecho sobre un nombre para corregirla manualmente","Nombres futuros",
        "Activar categoria actual y futura","Atajo G2A","frame_quality.p99_ms","ammunition_quality::freshness",
        "Seleccion de arma A2A","projectile_visibility::apply_category_policy({})","missile_alerts::apply_settings"})
        assert(menu.find(text)!=std::string::npos);
    assert(main.find("quality.frame_ms.add_ms")!=std::string::npos);
    assert(main.find("apply_category_overrides")!=std::string::npos && main.find("apply_category_policy")!=std::string::npos);
    assert(main.find("missile_alerts::apply_settings")!=std::string::npos);
    assert(main.find("check_toggle(menu_preferences::current.g2a_key,esp::g2a_lead::enabled,last_g2a)")!=std::string::npos);
    std::puts("PASS menu global/category selection, adjacent SDK reload, persisted independent G2A and integration gates");
}
