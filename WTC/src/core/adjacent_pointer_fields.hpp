#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace adjacent_pointer_fields {
    inline constexpr std::size_t max_bytes=64;
    template<class Reader>
    bool read_pair(std::uintptr_t base,std::uintptr_t offset_a,std::uintptr_t offset_b,
        std::uintptr_t& out_a,std::uintptr_t& out_b,Reader&& reader) {
        const auto first=std::min(offset_a,offset_b);
        const auto last=std::max(offset_a,offset_b);
        if (last-first>max_bytes-sizeof(std::uintptr_t) ||
            base>std::numeric_limits<std::uintptr_t>::max()-first)
            return false;
        const auto address=base+first;
        const auto count=static_cast<std::size_t>(last-first)+sizeof(std::uintptr_t);
        std::array<std::uint8_t,max_bytes> data{};
        if (!reader(address,data.data(),count)) return false;
        std::uintptr_t a=0,b=0;
        std::memcpy(&a,data.data()+(offset_a-first),sizeof(a));
        std::memcpy(&b,data.data()+(offset_b-first),sizeof(b));
        out_a=a;out_b=b;
        return true;
    }
}
