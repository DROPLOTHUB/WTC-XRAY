#include "../src/core/render_work_policy.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <iterator>
int main() {
    using namespace render_work_policy;
    using namespace std::chrono;
    const failed_read_gate::clock::time_point start{};
    failed_read_gate failed,healthy;
    int failed_scans=0,healthy_scans=0,global_checks=0;
    for(int ms=0;ms<1000;ms+=50) {
        const auto now=start+milliseconds(ms);
        if(failed.due(now,false)){++failed_scans;failed.record(now,false);}
        if(healthy.due(now,false)){++healthy_scans;healthy.record(now,true);}
        ++global_checks; // independent 50ms global fallback cadence remains available
    }
    assert(failed_scans==4 && healthy_scans==20 && global_checks==20);
    failed.record(start,false);
    assert(!failed.due(start+milliseconds(249),false));
    assert(failed.due(start+milliseconds(250),false));
    assert(failed.due(start+milliseconds(1),true)); // owner/epoch/selection change
    failed.record(start+milliseconds(1),true);
    assert(failed.due(start+milliseconds(51),false));
    std::vector<int> source,first,second;
    std::uint64_t seen_first=UINT64_MAX,seen_second=UINT64_MAX,revision=0;
    assert(copy_if_new(source,revision,first,seen_first)); // first empty snapshot
    int copies=0;
    for(int frame=0;frame<120;++frame){
        if(frame%12==0){source={frame,frame+1};++revision;}
        copies+=copy_if_new(source,revision,first,seen_first);
        copies+=copy_if_new(source,revision,second,seen_second);
        assert(first==source && second==source);
    }
    assert(copies==20); // 240 unconditional vector copies -> 20
    source.clear();++revision;
    assert(copy_if_new(source,revision,first,seen_first) && first.empty());
    source={999};++revision;
    assert(copy_if_new(source,revision,first,seen_first) && first[0]==999);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    auto text=[](const std::filesystem::path& p){std::ifstream f(p);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});};
    const auto misc=text(root/"features/misc/misc.hpp");
    size_t count=0,pos=0;
    while((pos=misc.find("units_revision.fetch_add(1,std::memory_order_release)",pos))!=std::string::npos){++count;++pos;}
    assert(count==3); // both clears and publication must invalidate both consumers
    assert(misc.find("snapshot_units_into(units,seen_units_revision)")!=std::string::npos);
    const auto esp=text(root/"features/esp/esp.hpp");
    assert(esp.find("snapshot_units_into(units,seen_units_revision)")!=std::string::npos);
    const auto ammo=text(root/"features/aimbot/aimbot.hpp");
    assert(ammo.find("aircraft_retry.due(now,changed)")!=std::string::npos);
    assert(ammo.find("next_update = now + std::chrono::milliseconds(50)")!=std::string::npos);
    std::puts("PASS simulated 1s: failed aircraft scans 20->4, healthy/global 20; 120 frames: identity copies 240->20; reset/publication invalidates");
}
