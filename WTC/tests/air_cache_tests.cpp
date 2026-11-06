#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
struct vec3_t {float x=0,y=0,z=0;};
#include "../src/features/esp/g2a_lead.hpp"
using namespace esp;
using clock_type=std::chrono::steady_clock;
using time_point=clock_type::time_point;
static time_point at(int ms) {return time_point{}+std::chrono::milliseconds(ms);}

// Isolated integration model, bound to the ESP gates by source contracts below.
// Exercises the actual motion estimator rather than testing timestamps alone.
struct air_cache {
    time_point sample{},latest{},next{};
    air_lead_tracking::motion_state motion{};
    air_lead_tracking::estimate held{};
    bool initialized=false;
    int observations=0;
    void render(time_point now,time_point captured,const vec3_t* native=nullptr) {
        const float age=std::chrono::duration<float>(now-captured).count();
        if(!std::isfinite(age)||age<0||age>.2f||captured<latest) {
            initialized=false;motion={};return;
        }
        latest=captured;
        const double interval=std::chrono::duration<double>(captured-sample).count();
        if(interval>.2){initialized=false;motion={};next={};}
        const float held_age=std::chrono::duration<float>(now-sample).count();
        if(!std::isfinite(held_age)||held_age<0||held_age>.2f)initialized=false;
        if(now>=next && interval>=.025) {
            sample=captured;next=now+std::chrono::milliseconds(30);
            const double stamp=std::chrono::duration<double>(captured.time_since_epoch()).count();
            held=air_lead_tracking::observe(motion,{float(stamp*20),0,0},stamp,native);
            initialized=held.valid;++observations;
        }
    }
};
int main() {
    // Position fallback survives repeated renders of a snapshot, including when
    // the 30 ms wall-clock solve timer has elapsed. Re-observing would reset it.
    air_cache cache;
    cache.render(at(1000),at(1000));assert(!cache.initialized);
    cache.render(at(1050),at(1050));
    assert(cache.initialized && cache.held.provenance==air_lead_tracking::source::positions);
    const auto count=cache.observations;
    cache.render(at(1066),at(1066)); // fresh frame before solve deadline: retain result
    assert(cache.initialized && cache.observations==count);
    cache.render(at(1085),at(1050)); // reordered relative to latest 1066
    assert(!cache.initialized && cache.latest==at(1066));
    cache.render(at(1100),at(1100));
    cache.render(at(1150),at(1150));assert(cache.initialized);
    const auto solved=cache.observations;
    for(int ms=1151;ms<=1349;++ms)cache.render(at(ms),at(1150));
    assert(cache.initialized && cache.observations==solved);
    cache.render(at(1351),at(1150));assert(!cache.initialized);

    // Fresh newest frame cannot keep a >200 ms old solved sample alive while
    // solving is blocked; held-solution age is not latest-snapshot age.
    const vec3_t velocity{20,0,0};
    cache={};cache.render(at(2000),at(2000),&velocity);assert(cache.initialized);
    cache.next=at(3000);
    cache.render(at(2201),at(2199),&velocity);
    assert(!cache.initialized && cache.latest==at(2199) && cache.sample==at(2000));
    cache={};cache.render(at(1000),at(1001),&velocity);assert(cache.observations==0);

    // 1 kHz rendering, high- and low-rate snapshots: every observation must
    // advance at least 25 ms and no duplicate may erase position fallback.
    for(int period:{8,100}) {
        cache={};
        for(int ms=1000;ms<2000;++ms) {
            const auto old_sample=cache.sample;const int old_count=cache.observations;
            cache.render(at(ms),at(ms/period*period));
            if(cache.observations!=old_count)
                assert(std::chrono::duration<double>(cache.sample-old_sample).count()>=.025);
            if(cache.observations>=2) {
                assert(cache.initialized);
                assert(cache.held.provenance==air_lead_tracking::source::positions);
                assert(std::abs(cache.held.velocity.x-20)<.01f);
            }
        }
        assert(cache.observations>=10 && cache.observations<=34);
    }

    // Real solve-context equality already invalidates ammo and API-mode changes.
    const air_lead_tracking::solve_context original{900,.12f,20,.1f,.02f,0,0,true,false,true,true,false,true,true};
    assert(original==original);
    auto changed=original;changed.speed=901;assert(!(changed==original));
    changed=original;changed.drag=.2f;assert(!(changed==original));
    changed=original;changed.selected=1;assert(!(changed==original));
    changed=original;changed.api_velocity=false;assert(!(changed==original));

    // Read completion, not read start, controls freshness supplied to the solver.
    const ballistic_validation::round_data round{900,.12f,.02f,.1f,0};
    const float start_age=std::chrono::duration<float>(at(1190)-at(1000)).count();
    const float complete_age=std::chrono::duration<float>(at(1210)-at(1000)).count();
    assert(g2a_lead::solve({0,10,0},{0,210,800},{},true,velocity,true,start_age,round).valid);
    assert(!g2a_lead::solve({0,10,0},{0,210,800},{},true,velocity,true,complete_age,round).valid);

    std::ifstream input(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/esp/esp.hpp");
    assert(input);const std::string src{std::istreambuf_iterator<char>(input),{}};
    for(const char* contract:{
        "last_sample{},latest{}",
        "curr_snapshot.captured_at<cache.latest",
        "cache.latest=curr_snapshot.captured_at",
        "now >= cache.next_sample && interval>=.025",
        "cache.last_sample=curr_snapshot.captured_at",
        "now-cache.last_sample",
        "held_age>.2f",
        "now>=shooter.next && interval>=.025",
        "shooter.sampled_at=curr_snapshot.captured_at",
        "const float sample_age = read_age",
        "read_complete-curr_snapshot.captured_at",
        "read_complete-shooter.sampled_at",
        "read_age>.2f",
        "cache.last_update = read_complete",
        "render_sample_age>.2f",
        "publication_age>.2f",
        "air_lead_epoch!=current_epoch",
        "previous_context==context",
        "cache.shooter_base!=local_unit_base",
        "now >= g2a_cache.next && interval >= .025 && !invalidate_solution",
        "curr_snapshot.captured_at < g2a_cache.latest",
        "g2a_cache.owner != local_unit_base",
        "g2a_cache.epoch != current_epoch",
        "previous.speed != round.speed",
        "g2a_cache.own_sample, g2a_cache.enemy_sample, read_age, round",
        "read_age > .2f"})assert(src.find(contract)!=std::string::npos);
    std::puts("PASS air cache cadence/duplicates/reordering/held age and read-latency regressions");
}
