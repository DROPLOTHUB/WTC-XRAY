#include "../src/features/esp/esp_frame_policy.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    using namespace esp::frame_policy;
    assert(in_render_range(0.f,100.f));
    assert(in_render_range(100.f,100.f));
    assert(!in_render_range(101.f,100.f));
    assert(!in_render_range(-1.f,100.f));
    assert(!in_render_range(std::numeric_limits<float>::quiet_NaN(),100.f));
    assert(!in_render_range(std::numeric_limits<float>::infinity(),100.f));
    assert(label_anchor(400.f,600.f,true)==600.f);
    assert(label_anchor(400.f,600.f,false)==400.f);
    assert(label_anchor(400.f,std::numeric_limits<float>::quiet_NaN(),true)==400.f);
    assert(label_anchor(400.f,-1.f,true)==400.f);
    assert(!needs_global_ballistics(false,false,true,true));
    assert(needs_global_ballistics(false,true,false,false));
    assert(!needs_global_ballistics(true,true,false,false));
    assert(!needs_global_ballistics(true,false,true,false));
    assert(needs_global_ballistics(true,false,true,true));
    assert(!needs_local_rotation(false,false,false));
    assert(!needs_local_rotation(true,false,false));
    assert(!needs_local_rotation(true,true,false));
    assert(!needs_local_rotation(false,true,true));
    assert(needs_local_rotation(true,true,true));
    using clock=std::chrono::steady_clock;
    const auto t=clock::time_point{}+std::chrono::milliseconds(1000);
    assert(!refresh_local_team(0x1000,0x1000,7,7,t,t+std::chrono::milliseconds(50)));
    assert(refresh_local_team(0x2000,0x1000,7,7,t,t+std::chrono::milliseconds(50)));
    assert(refresh_local_team(0x1000,0x1000,8,7,t,t+std::chrono::milliseconds(50)));
    assert(refresh_local_team(0x1000,0x1000,7,7,t,t));
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    std::ifstream frame_file(root/"features/misc/misc.hpp"); assert(frame_file);
    const std::string frame{std::istreambuf_iterator<char>(frame_file),{}};
    assert(frame.find("next.frame.local_unit_base=local_base")!=std::string::npos);
    std::ifstream esp_file(root/"features/esp/esp.hpp"); assert(esp_file);
    const std::string esp{std::istreambuf_iterator<char>(esp_file),{}};
    assert(esp.find("local_unit_base = frame.local_unit_base")!=std::string::npos);
    assert(esp.find("frame_policy::needs_global_ballistics")!=std::string::npos);
    assert(esp.find("frame_policy::needs_local_rotation")!=std::string::npos);
    assert(esp.find("is_scoping_offset")==std::string::npos);
    std::puts("PASS finite ESP range, partial boxes and selective DMA refresh");
}
