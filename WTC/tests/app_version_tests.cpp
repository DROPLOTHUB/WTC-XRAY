#include "../src/core/app_version.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    assert(std::string(app_version::release_name)=="v5.25 Full Language Public");
    assert(std::string(app_version::sidebar_label())=="TACTICAL / v5.25");
    const auto src=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    std::ifstream menu_file(src/"gui/tactical_menu.hpp");assert(menu_file);
    const std::string menu{std::istreambuf_iterator<char>(menu_file),{}};
    assert(menu.find("app_version::release_name")!=std::string::npos);
    assert(menu.find("app_version::sidebar_label()")!=std::string::npos);
    assert(menu.find("TACTICAL CONTROL  //  4.3")==std::string::npos);
    assert(menu.find("TACTICAL / 4.3")==std::string::npos);
    std::ifstream main_file(src/"main.cpp");assert(main_file);
    const std::string main{std::istreambuf_iterator<char>(main_file),{}};
    assert(main.find("core/app_version.hpp")!=std::string::npos);
    std::puts("PASS central release label shared by header and sidebar");
}
