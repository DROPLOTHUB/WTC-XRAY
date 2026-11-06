#include "../src/core/offset_api_mapping.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
int main() {
    std::ifstream in(std::filesystem::path(__FILE__).parent_path()/"free_offsets_fixture.json");
    assert(in.is_open());
    const std::string json((std::istreambuf_iterator<char>(in)),{});
    offset_api_payload::manifest manifest;
    std::string error;
    assert(offset_api_payload::parse(json,manifest,error));
    assert(offset_api_mapping::reload_available(manifest));
    manifest.entries["Unit.m_AirContainer"]=0xD50;
    manifest.entries["AirContainer.m_Velocity"]=0x220;
    assert(offset_api_mapping::air_velocity_available(manifest));
    manifest.entries.erase("AirContainer.m_Velocity");
    assert(!offset_api_mapping::air_velocity_available(manifest));
    manifest.entries["AirContainer.m_Velocity"]=0x220;
    assert(offset_api_mapping::invulnerable_available(manifest));
    assert(offset_api_mapping::invul_timer_available(manifest));
    const auto flag_offset=manifest.entries.at("Unit.m_Invulnerable");
    manifest.entries.erase("Unit.m_Invulnerable");
    assert(!offset_api_mapping::invulnerable_available(manifest));
    manifest.entries["Unit.m_Invulnerable"]=flag_offset;
    const auto timer_offset=manifest.entries.at("Unit.m_InvulTimer");
    manifest.entries.erase("Unit.m_InvulTimer");
    assert(!offset_api_mapping::invul_timer_available(manifest));
    manifest.entries["Unit.m_InvulTimer"]=timer_offset;
    const auto reload_offset=manifest.entries.at("Unit.m_VisualReload");
    manifest.entries.erase("Unit.m_VisualReload");
    assert(!offset_api_mapping::reload_available(manifest));
    manifest.entries["Unit.m_VisualReload"]=reload_offset;
    std::unordered_map<std::string,std::uintptr_t> old_values,next;
    for(const auto& mapping:offset_api_mapping::bindings)old_values.emplace(mapping.local,0x123);
    std::size_t count=0;
    assert(!offset_api_mapping::apply(manifest,old_values,next,error,count));
    assert(next.empty()); // public/free manifest is incomplete for ECS
    manifest.entries["Global.g_EntityManager"]=0x7E8CBE8; // fixture-only premium field
    assert(offset_api_mapping::apply(manifest,old_values,next,error,count));
    assert(count>=45);
    assert(next.at("g_RocketListIndexPtr")==next.at("rocket_index"));
    assert(next.at("g_BombListIndexPtr")==next.at("bomb_index"));
    assert(next.at("g_EntityManager")==next.at("alllistdata_offset"));
    assert(next.at("projectile_offsets.missile_name_type")==0x870);
    assert(old_values.at("g_RocketListIndexPtr")==0x123);
    std::puts("PASS public response rejected; complete response staged atomically");
}
