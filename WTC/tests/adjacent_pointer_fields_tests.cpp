#include "../src/core/adjacent_pointer_fields.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
int main() {
    constexpr std::uintptr_t base=0x000001aa20000000ULL;
    constexpr std::uintptr_t header=0x000001aa30000000ULL,transform=0x000001aa40000000ULL;
    std::array<std::uint8_t,256> memory{};
    std::memcpy(memory.data()+0x78,&header,sizeof(header));
    std::memcpy(memory.data()+0xa0,&transform,sizeof(transform));
    std::uintptr_t a=0,b=0;
    int reads=0;
    auto reader=[&](std::uintptr_t address,void* dest,std::size_t n){
        ++reads;
        assert(address>=base && address-base+n<=memory.size());
        std::memcpy(dest,memory.data()+(address-base),n);
        return true;
    };
    assert(adjacent_pointer_fields::read_pair(base,0x78,0xa0,a,b,reader));
    assert(reads==1 && a==header && b==transform);
    a=0;b=0;
    assert(adjacent_pointer_fields::read_pair(base,0xa0,0x78,a,b,reader));
    assert(reads==2 && a==transform && b==header);
    assert(!adjacent_pointer_fields::read_pair(base,0x78,0x180,a,b,reader));
    assert(reads==2); // Offsets locales distantes: caller debe leer cada campo
    assert(!adjacent_pointer_fields::read_pair(std::numeric_limits<std::uintptr_t>::max(),0x78,0xa0,a,b,reader));
    assert(reads==2); // no envolver dirección
    a=header;b=transform;
    assert(!adjacent_pointer_fields::read_pair(base,0x78,0xa0,a,b,
        [&](std::uintptr_t,void*,std::size_t){++reads;return false;}));
    assert(a==header && b==transform && reads==3); // fallo no publica campos parciales
    std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/esp/xray_bvh.hpp");
    assert(in);
    const std::string code{std::istreambuf_iterator<char>(in),{}};
    assert(code.find("adjacent_pointer_fields::read_pair(")!=std::string::npos);
    assert(code.find("header_new=TargetProcess->Read<uintptr_t>(")!=std::string::npos);
    assert(code.find("matrix_array_new=TargetProcess->Read<uintptr_t>(")!=std::string::npos);
    std::puts("PASS adjacent X-Ray pointer DMA and safe fallback on partial or distant fields");
}
