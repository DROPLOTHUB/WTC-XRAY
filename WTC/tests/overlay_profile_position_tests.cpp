#include "../src/gui/overlay_profile_position.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    using overlay_profile_position::top_right;
    const auto standard=top_right(0.f,0.f,1920.f,140.f);
    assert(standard.x==1765.f && standard.y==15.f);
    const auto offset=top_right(120.f,40.f,1280.f,200.f);
    assert(offset.x==1185.f && offset.y==55.f);
    const auto narrow=top_right(300.f,70.f,100.f,220.f);
    assert(narrow.x==315.f && narrow.y==85.f);
    const auto invalid=top_right(4.f,8.f,std::numeric_limits<float>::quiet_NaN(),80.f);
    assert(invalid.x==4.f && invalid.y==8.f);
    std::ifstream main_file(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/main.cpp");
    assert(main_file);
    const std::string main{std::istreambuf_iterator<char>(main_file),{}};
    assert(main.find("name!=cached_name")!=std::string::npos);
    assert(main.find("font!=cached_font")!=std::string::npos);
    assert(main.find("viewport->Size.x!=cached_viewport_width")!=std::string::npos);
    std::puts("PASS active preset HUD aligns right with FPS height across viewports");
}
