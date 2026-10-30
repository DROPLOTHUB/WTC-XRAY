#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <string_view>

namespace esp::radar::icons {
enum class kind : std::uint8_t { fighter,strike,bomber,helicopter,light,medium,heavy,tank_destroyer,spaa,naval,structure,unknown,count };
inline constexpr std::array<const char*,static_cast<std::size_t>(kind::count)> labels = {
    "Caza", "Ataque", "Bombardero", "Helicoptero", "Tanque ligero", "Tanque medio",
    "Tanque pesado", "Cazatanques", "Antiaereo", "Naval", "Estructura", "Desconocido"};
inline std::array<bool,static_cast<std::size_t>(kind::count)> visible = [] {
    std::array<bool,static_cast<std::size_t>(kind::count)> result{}; result.fill(true); return result; }();
inline bool enabled(kind symbol) {
    const auto i = static_cast<std::size_t>(symbol);
    return i < visible.size() && visible[i];
}
// Game-specific glyph IDs: without the engine font these Unicode points render
// box-drawing characters, so the overlay draws matching vector silhouettes.
inline constexpr char32_t game_glyph(kind symbol) {
    switch(symbol) {
    case kind::fighter:return U'\u2524';
    case kind::strike:return U'\u251E';
    case kind::bomber:return U'\u2520';
    case kind::helicopter:return U'\u2522';
    case kind::heavy:return U'\u2528';
    case kind::light:return U'\u252A';
    case kind::medium:return U'\u252C';
    case kind::spaa:return U'\u2530';
    case kind::tank_destroyer:return U'\u2534';
    default:return 0;
    }
}
inline std::string canonical(std::string_view raw) {
    std::string result;result.reserve(raw.size());
    for(unsigned char c:raw)
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9'))
            result.push_back(c>='A'&&c<='Z'?static_cast<char>(c+('a'-'A')):static_cast<char>(c));
    return result;
}
inline bool has(std::string_view text,std::string_view token) {return text.find(token)!=std::string_view::npos;}
inline kind classify(std::string_view subtype,std::string_view coarse) {
    const auto type=canonical(coarse);
    if(has(type,"helicopter"))return kind::helicopter;
    if(has(type,"bomber"))return kind::bomber;
    if(has(type,"assault")||has(type,"strike"))return kind::strike;
    if(has(type,"fighter")||has(type,"drone"))return kind::fighter;
    const auto sub=canonical(subtype);
    // Subtype strings precede the coarse exp_tank family (English client).
    if(has(sub,"spaa")||has(sub,"antiair")||has(sub,"flak")||has(sub,"zsu")||sub=="aa")return kind::spaa;
    if(has(sub,"tankdestroyer")||has(sub,"cazacarros")||has(sub,"cazatanques")||has(sub,"panzerjager"))return kind::tank_destroyer;
    if(has(sub,"mbt")||has(sub,"medium")||has(sub,"medio"))return kind::medium;
    if(has(sub,"light")||has(sub,"cruiser")||has(sub,"armored")||has(sub,"ligero"))return kind::light;
    if(has(sub,"spg")||has(sub,"heavy")||has(sub,"pesado"))return kind::heavy;
    if(has(type,"spaa")||has(type,"antiair")||type=="expaaa")return kind::spaa;
    if(has(type,"tankdestroyer"))return kind::tank_destroyer;
    if(has(type,"heavytank"))return kind::heavy;
    if(has(type,"lighttank")||has(type,"human")||has(type,"soldier"))return kind::light;
    if(has(type,"tank"))return kind::medium;
    if(has(type,"ship")||has(type,"boat")||has(type,"naval")||has(type,"vessel"))return kind::naval;
    if(has(type,"structure")||has(type,"building")||has(type,"bunker")||
       has(type,"pillbox")||has(type,"airfield"))return kind::structure;
    return kind::unknown;
}
}
