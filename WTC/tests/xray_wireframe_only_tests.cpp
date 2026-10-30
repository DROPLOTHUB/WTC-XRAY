#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static std::string source(const std::filesystem::path& path){
    std::ifstream in(path);assert(in);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
int main(){
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    const auto esp=source(root/"features/esp/esp.hpp");
    const auto sdk=source(root/"game/sdk.hpp");
    const auto main=source(root/"main.cpp");
    const auto menu=source(root/"gui/tactical_menu.hpp");
    for(const auto* part:{&esp,&sdk,&main,&menu}){
        assert(part->find("xray_render_mode")==std::string::npos);
        assert(part->find("xray_wireframe_overlay")==std::string::npos);
    }
    const auto start=esp.find("inline void draw_xray_modules(");
    const auto finish=esp.find("// [SECCIÓN 9] RUN PRINCIPAL",start);
    assert(start!=std::string::npos&&finish!=std::string::npos);
    const auto xray=esp.substr(start,finish-start);
    assert(xray.find("xray::boxes::compact(kind)")!=std::string::npos);
    assert(xray.find("draw_list->AddRect(clipped_min,clipped_max")!=std::string::npos);
    assert(xray.find("draw_module_lines_world(part")!=std::string::npos);
    assert(xray.find("read_unit_skeleton_ctx(unit_base,cache.dm_root)")!=std::string::npos);
    const auto skel_start=esp.find("inline unit_skeleton_ctx_t read_unit_skeleton_ctx");
    const auto skel_end=esp.find("inline bool read_animated_matrix",skel_start);
    assert(skel_start!=std::string::npos && skel_end!=std::string::npos);
    const auto skel=esp.substr(skel_start,skel_end-skel_start);
    assert(skel.find("ctx.dm = dm_root;")!=std::string::npos);
    assert(skel.find("damageModelCont_offset")==std::string::npos);
    assert(xray.find("AddRectFilled")==std::string::npos);
    assert(xray.find("flush_fill_batches")==std::string::npos);
    assert(menu.find("Modo: Wireframe (unico)")!=std::string::npos);
    assert(menu.find("##render_mode")==std::string::npos);
    std::puts("PASS wireframe-only X-Ray, Ammo outline boxes, legacy mode keys ignored");
}
