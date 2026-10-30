#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace adjacent_value_fields {
template<class T> struct result {
    T first{},second{};
    bool first_valid=false,second_valid=false;
};
// Reader must return true only for a complete read. Never publish partial bytes.
// Batch only adjacent/equal fields: no extra range or guessed layout is read.
template<class T,class Reader> result<T> read_pair(std::uintptr_t base,
    std::uintptr_t first_offset,std::uintptr_t second_offset,Reader&& reader) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T)<=64);
    result<T> out{};
    const auto addressable=[&](std::uintptr_t offset) {
        return base!=0 && offset<=UINTPTR_MAX-base &&
            base+offset<=UINTPTR_MAX-(sizeof(T)-1);
    };
    const bool first_safe=addressable(first_offset),second_safe=addressable(second_offset);
    const auto low=first_offset<second_offset ? first_offset : second_offset;
    const auto high=first_offset<second_offset ? second_offset : first_offset;
    if(first_safe && second_safe && (high-low==sizeof(T) || high==low)) {
        std::array<std::byte,2*sizeof(T)> buffer{};
        const auto bytes=static_cast<std::size_t>(high-low)+sizeof(T);
        if(reader(base+low,buffer.data(),bytes)) {
            std::memcpy(&out.first,buffer.data()+(first_offset-low),sizeof(T));
            std::memcpy(&out.second,buffer.data()+(second_offset-low),sizeof(T));
            out.first_valid=out.second_valid=true;
            return out;
        }
    }
    // Distant fields or a failed batch preserve independent success/failure.
    T value{};
    if(first_safe && reader(base+first_offset,&value,sizeof(T))) {
        out.first=value;out.first_valid=true;
    }
    value={};
    if(second_safe && reader(base+second_offset,&value,sizeof(T))) {
        out.second=value;out.second_valid=true;
    }
    return out;
}
}
