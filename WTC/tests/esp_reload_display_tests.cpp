#include "../src/features/esp/reload_display_policy.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <cstdio>
int main() {
    using namespace esp::reload_display_policy;
    assert(eligible(true,true,500.f*500.f,2000.f*2000.f)); // sin G2G/FOV
    assert(!eligible(false,true,100.f,400.f));
    assert(!eligible(true,false,100.f,400.f));
    assert(!eligible(true,true,500.f,400.f));
    assert(!eligible(true,true,std::numeric_limits<float>::quiet_NaN(),400.f));
    assert(supported_visual_value(0) && supported_visual_value(16));
    assert(!supported_visual_value(255));
    assert(ready_fraction(0)==1.f && ready_fraction(16)==0.f);
    const auto path=std::filesystem::path(__FILE__).parent_path().parent_path()/
        "src/features/esp/esp.hpp";
    std::ifstream in(path);assert(in);
    const std::string src{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    assert(src.find("if (sdk::esp_show_reload && has_best_aimbot_target)")==std::string::npos);
    assert(src.find("eligible(sdk::esp_show_reload && offsets::reload_offset_available")==std::string::npos);
    assert(src.find("entry.valid=TargetProcess->Read(address,&value,sizeof(value))")!=std::string::npos);
    assert(src.find("(10.f/16.f)*visual.value")!=std::string::npos);
    assert(src.find("get_cached_reload_info(target.base)")!=std::string::npos);
    assert(src.find("needs_reload_box ? &projected_box : nullptr")!=std::string::npos);
    assert(src.find("projected_box=projected_bounds_from_screen_corners(sc)")!=std::string::npos);
    assert(src.find("target.cached_box.valid ? target.cached_box :")!=std::string::npos);
    assert(src.find("get_projected_box_bounds(target.unit,target.position,")!=std::string::npos);
    assert(src.find("prof_esp_units_us.store(")!=std::string::npos);
    assert(src.find("prof_esp_radar_us.store(")!=std::string::npos);
    assert(src.find("prof_esp_xray_us.store(")!=std::string::npos);
    assert(src.find("prof_esp_reload_us.store(")!=std::string::npos);
    assert(src.find("prof_esp_invul_us.store(")!=std::string::npos);
    std::ifstream menu_in(path.parent_path().parent_path().parent_path()/"gui/tactical_menu.hpp");assert(menu_in);
    const std::string menu{std::istreambuf_iterator<char>(menu_in),{}};
    assert(menu.find("ESP radar %.2f | unidades %.2f | X-Ray %.2f")!=std::string::npos);
    assert(menu.find("ESP recarga %.2f | INVUL %.2f")!=std::string::npos);
    assert(src.find("Unit.m_VisualReload")==std::string::npos); // field name is not a magic offset here
    std::puts("PASS reload independent of ground aim/FOV and invalid visual values rejected");
}
