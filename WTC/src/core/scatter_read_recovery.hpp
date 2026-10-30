#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "startup_pointer_probe.hpp"

namespace scatter_read_recovery {
inline std::atomic<std::uint64_t> camera_fallback_ok{0}, camera_unavailable{0};
inline std::atomic<std::uint64_t> units_scatter_fail{0}, units_fallback_ok{0}, units_unavailable{0};
inline constexpr int max_units = 4096;
inline bool valid_unit_list(std::uintptr_t list,int count) {
    constexpr std::uintptr_t limit=0x00007FFFFFFFF000ULL;
    return count>0 && count<=max_units &&
        startup_pointer_probe::plausible_pointer(list) &&
        list<=limit-static_cast<std::uintptr_t>(count)*sizeof(std::uintptr_t);
}
inline std::size_t unit_list_bytes(std::uintptr_t list,int count) {
    return valid_unit_list(list,count) ? static_cast<std::size_t>(count)*sizeof(std::uintptr_t) : 0;
}
template<class Reader>
std::vector<std::uintptr_t> read_unit_list_individually(
    std::uintptr_t list,int count,Reader&& reader) {
    std::vector<std::uintptr_t> result;
    if (!valid_unit_list(list,count)) return result;
    result.reserve(static_cast<std::size_t>(count));
    for (int i=0;i<count;++i) {
        std::uintptr_t value=0;
        if (reader(list+static_cast<std::uintptr_t>(i)*sizeof(value),&value,sizeof(value)) &&
            startup_pointer_probe::plausible_pointer(value)) result.push_back(value);
    }
    return result;
}
}
