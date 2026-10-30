#include <cassert>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static std::string read(const std::filesystem::path& p){
    std::ifstream f(p);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
int main(){
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    for(const auto* path:{"game/sdk.hpp","main.cpp","gui/tactical_menu.hpp",
        "features/esp/esp.hpp","features/esp/esp_frame_policy.hpp"}){
        auto source=read(root/path);
        for(char& ch:source)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        assert(source.find("a2g")==std::string::npos);
    }
    assert(!std::filesystem::exists(root/"features/esp/a2g_prediction.hpp"));
    const auto a=read(root/"features/aimbot/aimbot.hpp");
    assert(a.find("inline bool GetAircraftBallisticsInfo(uintptr_t snapshot_owner=0)")!=std::string::npos);
    assert(a.find("GetAircraftBallisticsInfoEx")==std::string::npos);
    assert(a.find("GetAircraftBallisticsInfo(owner)")!=std::string::npos);
    assert(a.find("GetBallisticsInfo())")!=std::string::npos);
    std::puts("PASS only A2A/G2G paths remain; aircraft ammo reader has one output");
}
