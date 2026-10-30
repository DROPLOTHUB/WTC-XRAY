#include "../src/core/render_work_policy.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    using namespace render_work_policy;
    for(int connected=0;connected<2;++connected)
    for(int esp=0;esp<2;++esp)
    for(int missiles=0;missiles<2;++missiles) {
        const auto work=snapshot_demand(connected,esp,missiles);
        assert(work.frame==bool(connected && (esp||missiles)));
        assert(work.unit_positions==bool(connected && esp));
    }
    int read_count=0;
    const auto read=[&](int& value){value=++read_count;return true;};
    frame_sample<int> lazy;
    assert(!lazy.attempted && read_count==0);
    for(int target=0;target<20;++target)assert(lazy.get(read));
    assert(read_count==1 && lazy.value==1);
    frame_sample<int> next_frame;
    assert(next_frame.get(read) && next_frame.value==2);
    frame_sample<int> failure;
    int attempts=0;
    const auto fail=[&](int& value){++attempts;value=999;return false;};
    for(int target=0;target<20;++target)assert(!failure.get(fail));
    assert(attempts==1 && failure.value==0); // no partial output or stale successful value
    read_count=0;
    for(int frame=0;frame<240;++frame) {
        frame_sample<int> rotation;
        if(frame%8==0)for(int target=0;target<20;++target)assert(rotation.get(read));
    }
    assert(read_count==30); // 240 frame reads -> 30 sampled frames, shared by all targets
    auto text=[](const std::filesystem::path& p){std::ifstream f(p);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});};
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    const auto main=text(root/"main.cpp"),misc=text(root/"features/misc/misc.hpp"),esp=text(root/"features/esp/esp.hpp");
    assert(main.find("snapshot_demand(core::IsConnected()")!=std::string::npos);
    assert(main.find("std::shared_ptr<const misc::frame_snapshot_t> projectile_frame;")!=std::string::npos);
    assert(main.find("if(work.frame) {")<main.find("misc::UpdateFrameSnapshot(work.unit_positions)"));
    assert(misc.find("if(include_unit_positions) snapshot_units_into(units,seen_units_revision)")!=std::string::npos);
    assert(misc.find("units.clear();\n\t\t\tseen_units_revision=UINT64_MAX;")!=std::string::npos);
    assert(esp.find("frame_sample<rotation_matrix_t> local_rotation;")!=std::string::npos);
    assert(esp.find("if (weapon_valid) {")<esp.find("local_rotation.get("));
    assert(esp.find("adjacent_value_fields::read_pair<vec3d_t>")!=std::string::npos);
    assert(esp.find("velocity_ok=pair.first_valid")!=std::string::npos);
    assert(esp.find("acceleration_ok=pair.second_valid")!=std::string::npos);
    std::puts("PASS snapshot demand matrix, no stale inactive frame, missile-only skips unit lists, lazy rotation simulation 240->30 reads");
}
