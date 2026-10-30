#include "../src/core/scatter_read_recovery.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    using namespace scatter_read_recovery;
    const std::uintptr_t list=0x000001aa20000000ULL;
    const std::uintptr_t units[]={0x000001aa30000000ULL,0,0x100000001ULL,0x000001aa40000000ULL};
    assert(valid_unit_list(list,4));
    assert(unit_list_bytes(list,4)==4*sizeof(std::uintptr_t));
    assert(unit_list_bytes(list,max_units)==static_cast<std::size_t>(max_units)*sizeof(std::uintptr_t));
    assert(unit_list_bytes(list,max_units+1)==0);
    assert(!valid_unit_list(list,0) && !valid_unit_list(list,500000));
    assert(!valid_unit_list(0x100000001ULL,4));
    assert(!valid_unit_list(0x00007FFFFFFFF000ULL,4));
    int reads=0;
    auto reader=[&](std::uintptr_t address,void* dest,std::size_t size) {
        ++reads;
        const auto i=(address-list)/sizeof(std::uintptr_t);
        if (address<list || i>=4 || size!=sizeof(std::uintptr_t) || i==2) return false;
        std::memcpy(dest,&units[i],size);return true;
    };
    const auto found=read_unit_list_individually(list,4,reader);
    assert(reads==4 && found.size()==2);
    assert(found[0]==units[0] && found[1]==units[3]);
    assert(read_unit_list_individually(list,500000,reader).empty());
    assert(reads==4); // lista imposible nunca dispara lecturas
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path()/"src/features/misc/misc.hpp";
    std::ifstream in(source); assert(in);
    const std::string code{std::istreambuf_iterator<char>(in),{}};
    assert(code.find("pointers.data(),pointer_bytes,&pointer_bytes_read")!=std::string::npos);
    assert(code.find("pointer_bytes_read==pointer_bytes")!=std::string::npos);
    assert(code.find("read_unit_list_individually(")!=std::string::npos);
    assert(code.find("ExecuteReadScatter(hScatter,0,false)")!=std::string::npos);
    std::puts("PASS scatter failure direct-read recovery with bounded unit list");
}
