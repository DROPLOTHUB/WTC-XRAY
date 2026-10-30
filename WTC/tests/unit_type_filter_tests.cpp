#include "../src/features/esp/unit_type_filter.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    using esp::unit_type_filter::noncombatant;
    for (const auto name : {"dummy_plane","dummy_tank","dummy_immortal",
        "structures/dummy_immortal","exp_fortification","exp_structure",
        "exp_aaa","exp_bridge","ai_bot"}) assert(noncombatant(name));
    for (const auto name : {"exp_fighter","exp_recon_drone","exp_helicopter",
        "exp_tank","exp_heavy_tank","exp_SPAA"}) assert(!noncombatant(name));
    // Check priority: dummy names must win even when raw type is Air/Ground.
    const auto file=std::filesystem::path(__FILE__).parent_path().parent_path()/
        "src/features/esp/esp.hpp";
    std::ifstream in(file);
    assert(in);
    const std::string src{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    const auto filter=src.find("if (unit_type_filter::noncombatant(unit_type))");
    const auto general=src.find("UnitClass cls = classify_unit_type(raw_type);");
    assert(filter!=std::string::npos && general!=std::string::npos && filter<general);
    std::puts("PASS dummies preempt air/ground classification; genuine drones remain");
}
