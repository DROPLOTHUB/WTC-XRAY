#pragma once
#include <chrono>
#include <cstdint>
namespace render_work_policy {
struct snapshot_work {bool frame=false,unit_positions=false;};
constexpr snapshot_work snapshot_demand(bool connected,bool esp,bool projectiles) {
    return {connected && (esp || projectiles),connected && esp};
}
// Stack-local to one render frame. Share one attempted read across all targets,
// including failures, but never carry a rotation into another frame.
template<class T> struct frame_sample {
    T value{};
    bool attempted=false,valid=false;
    template<class Reader> bool get(Reader&& reader) {
        if(!attempted) {
            attempted=true;
            T candidate{};
            valid=reader(candidate);
            if(valid)value=candidate;
        }
        return valid;
    }
};
struct failed_read_gate {
    using clock=std::chrono::steady_clock;
    clock::time_point next{};
    bool due(clock::time_point now,bool context_changed) const {return context_changed || now>=next;}
    void record(clock::time_point now,bool success) {
        next=success ? now : now+std::chrono::milliseconds(250);
    }
};
// Caller protects source and revision with the same shared mutex.
// Only identities are cached; camera and position sampling is not slowed down.
template<class Container> bool copy_if_new(const Container& source,std::uint64_t revision,
    Container& destination,std::uint64_t& seen) {
    if(seen==revision)return false;
    destination=source;seen=revision;return true;
}
}
