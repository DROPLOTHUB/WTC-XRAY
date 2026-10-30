#include "../src/core/adjacent_value_fields.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
struct vector3d {double x=0,y=0,z=0;bool operator==(const vector3d&) const=default;};
int main() {
    constexpr std::uintptr_t base=0x100000,velocity=0x15B0,acceleration=0x15C8;
    const vector3d expected_v{100,200,300},expected_a{1,2,3};
    std::map<std::uintptr_t,std::byte> memory;
    auto put=[&](std::uintptr_t offset,const vector3d& value) {
        std::array<std::byte,sizeof(vector3d)> bytes{};
        std::memcpy(bytes.data(),&value,bytes.size());
        for(size_t i=0;i<bytes.size();++i)memory[base+offset+i]=bytes[i];
    };
    std::vector<std::pair<std::uintptr_t,size_t>> reads;
    auto reader=[&](std::uintptr_t address,void* data,size_t size) {
        reads.emplace_back(address,size);
        auto* bytes=static_cast<std::byte*>(data);
        for(size_t i=0;i<size;++i) {
            auto it=memory.find(address+i);if(it==memory.end())return false;
            bytes[i]=it->second;
        }
        return true;
    };
    const auto run=[&](std::uintptr_t a,std::uintptr_t b) {
        reads.clear();return adjacent_value_fields::read_pair<vector3d>(base,a,b,reader);
    };
    put(velocity,expected_v);put(acceleration,expected_a);
    auto sample=run(velocity,acceleration);
    assert(sample.first_valid && sample.second_valid && sample.first==expected_v && sample.second==expected_a);
    assert(reads.size()==1 && reads[0].second==48);
    sample=run(acceleration,velocity);
    assert(sample.first==expected_a && sample.second==expected_v && reads.size()==1);
    sample=run(velocity,velocity);
    assert(sample.first==sample.second && sample.first==expected_v && reads.size()==1 && reads[0].second==24);
    put(0x2000,expected_a);sample=run(velocity,0x2000);
    assert(reads.size()==2 && sample.first==expected_v && sample.second==expected_a);
    // A reader may mutate a prefix and still report an incomplete read.
    memory.erase(base+acceleration+sizeof(vector3d)-1);
    sample=run(velocity,acceleration);
    assert(reads.size()==3 && sample.first_valid && !sample.second_valid);
    assert(sample.first==expected_v && sample.second==vector3d{});
    put(acceleration,expected_a);memory.erase(base+velocity);
    sample=run(velocity,acceleration);
    assert(reads.size()==3 && !sample.first_valid && sample.second_valid);
    assert(sample.first==vector3d{} && sample.second==expected_a);
    memory.clear();sample=run(velocity,acceleration);
    assert(!sample.first_valid && !sample.second_valid && sample.first==vector3d{} && sample.second==vector3d{});
    reads.clear();sample=adjacent_value_fields::read_pair<vector3d>(UINTPTR_MAX-8,16,24,reader);
    assert(reads.empty() && !sample.first_valid && !sample.second_valid);
    reads.clear();sample=adjacent_value_fields::read_pair<vector3d>(0,velocity,acceleration,reader);
    assert(reads.empty() && !sample.first_valid && !sample.second_valid);
    put(velocity,expected_v);put(acceleration,expected_a);reads.clear();
    for(int i=0;i<1000;++i) {
        sample=adjacent_value_fields::read_pair<vector3d>(base,velocity,acceleration,reader);
        assert(sample.first==expected_v && sample.second==expected_a);
    }
    assert(reads.size()==1000); // two independent fields previously required 2000 reads
    std::puts("PASS adjacent A2A vectors: 2000->1000 complete reads; reversed/distant/equal fields; partial failures isolated; overflow rejected");
}
