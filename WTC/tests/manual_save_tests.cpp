#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    assert(file.is_open());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int main() {
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    const auto main=read_file(source/"main.cpp");
    const auto menu=read_file(source/"gui"/"tactical_menu.hpp");
    assert(main.find("output << prefix << \"missile_count=\"")!=std::string::npos);
    assert(main.find("output << prefix << \"radar_kind_\"")!=std::string::npos);
    assert(main.find("preset.missile_overrides = projectile_visibility::capture()")!=std::string::npos);
    assert(main.find("preset.radar_kinds = esp::radar::icons::visible")!=std::string::npos);
    assert(main.find("missile_registry_storage::load();")!=std::string::npos);
    assert(main.find("projectile_visibility::restore_observed(names)")!=std::string::npos);
    assert(main.find("values.find(\"missile_catalog_count\")")!=std::string::npos); // migracion
    assert(main.find("output << \"missile_catalog_count=\"")==std::string::npos); // sin duplicar en config
    assert(main.find("update_autosave")==std::string::npos);
    assert(main.find("initialize_autosave")==std::string::npos);
    assert(main.find("GUI::save_now()")==std::string::npos); // F2 and shutdown must not save
    assert(main.find("GUI::update_unsaved_indicator();")!=std::string::npos);
    auto indicator=main.substr(main.find("void update_unsaved_indicator()"));
    indicator.resize(indicator.find("bool ExportConfig("));
    assert(indicator.find("save_now(")==std::string::npos);
    assert(indicator.find("config::save(")==std::string::npos);
    assert(indicator.find("if (!menu_open)")!=std::string::npos);
    assert(indicator.find("std::chrono::milliseconds(100)")!=std::string::npos);
    auto importer=main.substr(main.find("bool ImportConfig("));
    importer.resize(importer.find(R"tag(#include "gui/tactical_menu.hpp")tag"));
    assert(importer.find("save_now(")==std::string::npos);
    const auto load=menu.find(R"tag(if(TacticalUI::Button("Cargar")tag");
    assert(load!=std::string::npos);
    const auto load_end=menu.find(R"tag(TacticalSearchFocus("Cargar"))tag",load);
    assert(load_end!=std::string::npos && menu.substr(load,load_end-load).find("save_now(")==std::string::npos);
    const auto discard=menu.find(R"tag(if(TacticalUI::Button("Descartar y cargar")tag");
    const auto discard_end=menu.find(R"tag(if(TacticalUI::Button("Cancelar")tag",discard);
    assert(discard!=std::string::npos&&discard_end!=std::string::npos);
    assert(menu.substr(discard,discard_end-discard).find("save_now(")==std::string::npos);
    assert(menu.find(R"tag(if(TacticalUI::SmallButton("Guardar configuración")) save_now();)tag")!=std::string::npos);
    std::puts("PASS manual save only: no debounce, F2, import, load or shutdown writes");
}
