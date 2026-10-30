#pragma once
#include <string_view>

namespace esp::unit_type_filter {
// El nombre de una entidad sintetica tiene prioridad sobre el tipo general:
// dummy_plane puede informar raw_type=Air y dummy_tank raw_type=Ground.
inline constexpr bool noncombatant(std::string_view name) {
    return name=="exp_fortification" || name=="exp_structure" ||
        name=="exp_aaa" || name=="dummy_plane" || name=="exp_bridge" ||
        name=="dummy_tank" || name=="dummy_immortal" ||
        name=="structures/dummy_immortal" || name=="ai_bot";
}
}
