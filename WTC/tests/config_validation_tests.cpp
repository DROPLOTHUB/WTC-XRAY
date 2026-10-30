#include "../src/core/config_validation.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    assert(config_validation::parse_int("42",7)==42);
    assert(config_validation::parse_int("42garbage",7)==7);
    assert(config_validation::parse_int("999999999999999999999",7)==7);
    assert(config_validation::parse_int("",7)==7);
    assert(config_validation::parse_float("0.65",.4f)==.65f);
    assert(config_validation::parse_float("0.65ms",.4f)==.4f);
    assert(config_validation::parse_float("NaN",.4f)==.4f);
    assert(config_validation::parse_float("1e100",.4f)==.4f);
    std::array<float,4> rgba{.1f,.2f,.3f,.4f};
    assert(config_validation::parse_rgba("2,-1,0.5,0.75",rgba));
    assert(rgba[0]==1.f && rgba[1]==0.f && rgba[2]==.5f && rgba[3]==.75f);
    const auto previous=rgba;
    assert(!config_validation::parse_rgba("1|0|0|1",rgba));
    assert(!config_validation::parse_rgba("1,0,0,1garbage",rgba));
    assert(!config_validation::parse_rgba("1,0,NaN,1",rgba));
    assert(!config_validation::parse_rgba("1,0,0",rgba));
    assert(rgba==previous);
    std::puts("PASS strict floats, integers, color bounds and malformed fields");
}
