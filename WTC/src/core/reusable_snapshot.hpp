#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <unordered_map>

namespace reusable_snapshot {
// The resource belongs to the snapshot, not the producer or the publishing ring.
// Retained read-only snapshots therefore outlive both without dangling storage.
template<class FrameState,class Position> struct snapshot {
private:
    std::pmr::unsynchronized_pool_resource position_memory_;
public:
    std::uint64_t frame_id=0;
    std::chrono::steady_clock::time_point captured_at{};
    FrameState frame{};
    std::pmr::unordered_map<std::uintptr_t,Position> positions;
    explicit snapshot(std::pmr::memory_resource* upstream=std::pmr::get_default_resource())
        :position_memory_(std::pmr::pool_options{0,64},upstream),positions(&position_memory_) {}
    snapshot(const snapshot&)=delete;
    snapshot& operator=(const snapshot&)=delete;
    // Only call while exclusively owned by the producer. Keep node/bucket storage.
    void reset() {
        frame_id=0;captured_at={};frame={};positions.clear();
    }
};

// Single producer. Never expose weak references: a snapshot is writable only
// when this pool holds the sole shared owner, so published readers stay immutable.
// Three published slots + one builder normally require four buffers. If readers
// retain all buffers, allocate an unpooled spill rather than overwrite or block.
template<class Snapshot,std::size_t Capacity=4> class pool {
    static_assert(Capacity>0);
    std::array<std::shared_ptr<Snapshot>,Capacity> slots_{};
    std::size_t next_=0;
    std::pmr::memory_resource* upstream_;
public:
    explicit pool(std::pmr::memory_resource* upstream=std::pmr::get_default_resource())
        :upstream_(upstream) {}
    pool(const pool&)=delete;
    pool& operator=(const pool&)=delete;
    std::shared_ptr<Snapshot> acquire() {
        for(std::size_t checked=0;checked<Capacity;++checked) {
            const auto index=(next_+checked)%Capacity;
            auto& slot=slots_[index];
            if(!slot)slot=std::make_shared<Snapshot>(upstream_);
            if(slot.use_count()==1) {
                next_=(index+1)%Capacity;
                return slot;
            }
        }
        return std::make_shared<Snapshot>(upstream_);
    }
};
}
