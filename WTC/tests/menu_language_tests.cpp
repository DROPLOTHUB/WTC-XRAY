#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "../src/gui/menu_preferences.hpp"
#include "../src/gui/runtime_language.hpp"

static std::string source(const char* name) {
    std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path()/name);
    assert(in); return {std::istreambuf_iterator<char>(in),{}};
}
static std::string identity(const std::string& value) {
    const auto reset=value.rfind("###");
    return reset==std::string::npos?value:value.substr(reset+3);
}
int main() {
    using namespace menu_preferences;
    current={};
    assert(current.language==1 && current.filter_static_air);
    assert(valid_language(0)==0 && valid_language(1)==1);
    for(int invalid:{-100,-1,2,100}) {
        current.language=invalid;
        assert(valid_language(invalid)==1);
        assert(std::string(label("Guardar configuración"))=="Save configuration");
    }
    // Controls/headings/actions from every menu page, not only English-key labels.
    const char* spanish[]={"Panel de control","Visuales","Misiles","Perfiles","Ajustes",
        "Vista avanzada","Solo favoritos","Cambios sin guardar","Guardar configuración",
        "Recargar SDK","Filtrar aeronaves estaticas","Idioma","Mostrar INVUL",
        "Colores de módulos","Seleccionar todos los misiles","Nombres futuros",
        "Sonido de alerta","Seleccion de arma A2A","Usar esta arma",
        "Compatibilidad alfa (sin negro)","Suavizado y controles","Selección de objetivos",
        "RADAR / PRINCIPAL","Cazatanques","Perfiles guardados","Cargar","Guardar",
        "Confirmar cambio de perfil","Sí, sobrescribir","Confirmar importación",
        "Respaldar e importar","Pantalla","Interfaz","Acento rojo",
        "Guardar clave API local","Actualizar offsets ahora","Borrar clave API local",
        "F1 / F2 reservadas. Escape cancela; Backspace borra.",
        "Desactivado por defecto tras el fallo reportado.\nAl activarlo, arrastra el titulo del menu a la otra pantalla.\nSi falla el dibujo, desactivalo aqui."};
    for(const char* key:spanish) {
        current.language=0; const std::string es=label(key);
        const auto es_id=widget_label(key);
        current.language=1; const std::string en=label(key);
        const auto en_id=widget_label(key);
        assert(es==key && en!=es && !en.empty());
        assert(identity(es_id)==identity(en_id));
        assert(identity(en_id)==key);
    }
    for(const char* key:{"Enable ESP","Render Distance","Only Incoming Missiles",
        "Turn Prediction (3D)","Apply Fullscreen to Monitor","Export Config","Close Menu"}) {
        current.language=0; const auto es=widget_label(key);
        current.language=1; const auto en=widget_label(key);
        assert(es!=en && identity(es)==identity(en));
    }
    for(const char* key:{"##switch","##menu_language","WTC / REDLINE###WTCMenu",
        "Buscar nombre##missile_filter","first###result"}) {
        current.language=0;const auto es=widget_label(key);
        current.language=1;assert(identity(es)==identity(widget_label(key)));
    }
    assert(widget_label("##switch")=="##switch");
    assert(label(nullptr) && std::string(label(nullptr)).empty());
    assert(std::string(label("Unknown technical data"))=="Unknown technical data");
    current.language=0;toggle_favorite("Enable ESP");toggle_favorite("Filtrar aeronaves estaticas");
    const auto saved=current;
    current.language=1;current.filter_static_air=false;
    assert(current.favorites==saved.favorites && !(current==saved));
    current=saved;assert(current==saved && current.filter_static_air && current.language==0);
    // Integration guards: profile capture/apply/equality are shared with revert;
    // persistence is deliberately checked at its actual manual read/write sites.
    const auto main=source("src/main.cpp");
    for(const char* text:{"preset.ui = menu_preferences::current",
        "menu_preferences::current = preset.ui",
        "read_int(\"language\",loaded.ui.language)",
        "read_bool(\"filter_static_air\",loaded.ui.filter_static_air)",
        "\"language=\" << menu_preferences::valid_language(p.ui.language)",
        "\"filter_static_air=\" << int(p.ui.filter_static_air)"})
        assert(main.find(text)!=std::string::npos);
    const auto ui=source("src/gui/tactical_menu.hpp");
    assert(ui.find("menu_preferences::current.language=language;")!=std::string::npos);
    assert(ui.find("runtime_language::set_language(language);")!=std::string::npos);
    assert(ui.find("TacticalToggle(\"Filtrar aeronaves estaticas\", &menu_preferences::current.filter_static_air)")!=std::string::npos);
    assert(ui.find("namespace TacticalUI")!=std::string::npos);
    assert(ui.find("###search_result")!=std::string::npos);
    assert(ui.find("ImGui::GetStyle()=saved_style;")!=std::string::npos);
    // Every formatted presentation sink localizes its format AND status/enum arguments.
    for(const char* wiring:{"runtime_language::format(menu_preferences::label(source),menu_preferences::current.language,args...)",
        "ImGui::TextUnformatted(Format(source,args...).c_str())", "Format(source,args...).c_str()); }",
        "storage.push_back(LocalText(items[i]))", "LocalText(preview).c_str()", "TacticalUI::BulletText(",
        "localized_format=TacticalUI::LocalText(format)",
        "ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions",
        "ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview",
        "ImGui::ColorPicker4(\"##picker\",rgba,picker_flags)", "TacticalUI::Button(\"Restaurar color\")",
        "TacticalUI::Button(\"Copiar RGBA (float)\")", "TacticalUI::Button(\"Copiar RGBA hexadecimal\")",
        "ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F,rgba,4*sizeof(float))",
        "TacticalUI::TextUnformatted(esp::radar::icons::labels[i])", "TacticalUI::LocalText(group.header)",
        "TacticalUI::LocalText(\"SIMULATION / PREVIEW\")", "TacticalUI::LocalText(\"Mouse 1\")",
        "TacticalUI::TextWrapped(\"G2A calidad: %s\",esp::g2a_lead::quality)",
        "TacticalUI::TextWrapped(\"A2A: %s\",sdk::air_lead_status)",
        "TacticalUI::TextWrapped(\"G2G: %s\", sdk::ground_prediction_status)",
        "TacticalUI::TextWrapped(\"%s\",missile_registry_storage::status.c_str())",
        "TacticalUI::TextWrapped(\"%s\",offset_auto_update::get_status().c_str())",
        "TacticalUI::Format(\"Armas consultadas:", "TacticalUI::Format(\"  array:",
        "TacticalUI::LocalText(\"  velocidad cartucho: \")", "ImGui::SetClipboardText(report.str().c_str())"})
        assert(ui.find(wiring)!=std::string::npos);
    assert(ui.find("aircraft_weapon_trace.report()") == std::string::npos);
    assert(ui.find("ImGui::ColorEdit4(") == std::string::npos);
    assert(ui.find("are not localized") == std::string::npos);
    // Host/device/game labels and user profile strings must never become translation keys.
    for(const char* wiring:{"runtime_language::raw(presets[selected].name.c_str())",
        "runtime_language::raw(pending_overwrite>=0?presets[pending_overwrite].name.c_str():\"\")",
        "runtime_language::raw(offset_auto_update::get_applied_version().c_str())",
        "ImGui::TextUnformatted(name.c_str())", "ImGui::Selectable(sdk::monitor_labels[i].c_str(), is_selected)",
        "ImGui::BeginCombo(\"##monitor\", preview)", "if(ImGui::Button((std::string(capture==id?",
        "Archivos de configuracion (*.cfg)\\0*.cfg\\0Todos los archivos (*.*)"})
        assert(ui.find(wiring)!=std::string::npos);
    for(int language:{0,1}) {
        runtime_language::set_language(language);
        const auto localized=runtime_language::format("ID: %d | valida: %s",language,17,"si");
        assert(localized.find("17")!=std::string::npos);
        assert(localized.find(language==0?"si":"yes")!=std::string::npos);
        for(const char* data:{"Sin datos", "Fighter", "Mouse 1", "C:\\game\\profile.cfg", "User 100% %s"})
            assert(runtime_language::format("%s",language,runtime_language::raw(data))==data);
    }
    assert(runtime_language::format("A2A: %s",1,"Sin intento")!="A2A: Sin intento");
    assert(runtime_language::format("Atajo rechazado: %s (reservado o en uso).",1,"Enable ESP").find("Atajo rechazado")==std::string::npos);
    assert(runtime_language::format("Armas consultadas: %zu | seleccionada: %d (indices desde 0)\n",1,std::size_t{3},2)
        .find("Weapons queried:")==0);
    std::puts("PASS static/dynamic bilingual sinks, diagnostic/tooltips/clipboard wiring, raw data, stable IDs and profile language persistence");
}
