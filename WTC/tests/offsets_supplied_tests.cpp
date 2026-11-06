#include "../src/game/offsets.hpp"
#include "../src/features/esp/projectile_ecs.hpp"
#include <cassert>
#include <cstdio>
int main() {
    assert(offsets::Global::g_EntityManager==0x7E8CBE8);
    assert(offsets::Global::g_RocketListIndexPtr==0x7A27860);
    assert(offsets::Global::g_BombListIndexPtr==0x7A26320);
    assert(offsets::cgame_offset==0x7A4B848);
    assert(offsets::localplayer_offset==0x7A22CB0);
    assert(offsets::my_unit_offset==0x7A4D7F8);
    assert(offsets::view_matrix_offset==0x7AA58B8);
    assert(offsets::proj_ballistics_props_positive_offset==0x7A303E0);
    assert(offsets::name_map_offset==0x7A4A668);
    assert(offsets::function_rayhit_normalized==0x3462BC0);
    assert(offsets::unit_offsets::position_offset==0xD40);
    assert(offsets::map_drawn_this_frame_offset==0x7A98CAC);
    assert(offsets::map_minimap_pos_offset==0x794E7A0);
    assert(offsets::map_minimap_size_offset==0x794E7A8);
    assert(offsets::map_world_center_offset==0x7A98CB0);
    assert(offsets::map_view_size_offset==0x794E798);
    assert(offsets::map_full_world_size_offset==0x794E7B0);
    assert(offsets::map_full_world_center_offset==0x7A98CC0);
    assert(offsets::tactical_map_obj_offset==0x7A76918);
    assert(offsets::unit_offsets::info_offset==0x1028);
    assert(offsets::game_font_list_offset==0x7F669A8);
    assert(offsets::unit_info_offsets::vehicle_name_offset==0x28);
    assert(offsets::unit_info_offsets::unit_type_offset==0x38);
    assert(offsets::projectile_offsets::missile_name_type==0x870);
    projectile_ecs::Layout ecs{};
    assert(ecs.manager_rva==offsets::Global::g_EntityManager);
    assert(ecs.rocket_selector_rva==offsets::Global::g_RocketListIndexPtr);
    assert(ecs.bomb_selector_rva==offsets::Global::g_BombListIndexPtr);
    assert(ecs.missile_name_offset==0x720);
    assert(ecs.missile_name_text_offset==offsets::projectile_offsets::missile_name_type);
    assert(ecs.missile_name_text_offset_alt==0x50);
    assert(ecs.bomb_name_text_offset==0x10);
    assert(projectile_ecs::name_is_air_to_air("aim_9l"));
    assert(!projectile_ecs::name_is_air_to_air("agm_65"));
    assert(!projectile_ecs::name_is_air_to_air(""));
    assert(ecs.missile_name_offset_alt==0);
    std::puts("PASS supplied global RVAs and missile-name pointer chain");
}
