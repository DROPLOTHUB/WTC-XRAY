#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace projectile_visibility {
enum class category { air_to_air, air_to_ground, ground_to_air, unknown };

inline constexpr const char* category_label(category value) noexcept {
    switch (value) {
    case category::air_to_air: return "A2A";
    case category::air_to_ground: return "A2G";
    case category::ground_to_air: return "G2A";
    default: return "Unknown";
    }
}

// Automatic heuristic identification, not proof of the actual launch platform. Exact
// known designations only: broad substrings (r_, 9m, kh, sam) and dual-role
// trade names (MICA, Python, Derby, IRIS-T, etc.) are deliberately Unknown.
// This does NOT replace the legacy AAM visibility default or its exclusions.
inline category classify_automatic(std::string_view name) {
    if (name.empty() || name.size() > 63) return category::unknown;
    std::string lower;
    lower.reserve(name.size());
    for (unsigned char c : name) {
        if (c < 0x21 || c > 0x7e || c == '=') return category::unknown;
        lower += c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
    }
    if (lower.find("flare") != std::string::npos || lower.find("mm") != std::string::npos)
        return category::unknown;
    // ECS aircraft prefix is metadata, not part of the weapon designation.
    std::string_view weapon(lower);
    if (const auto prefix = weapon.find('^'); prefix != std::string_view::npos) {
        if (prefix == 0 || weapon.find('^', prefix + 1) != std::string_view::npos)
            return category::unknown;
        weapon.remove_prefix(prefix + 1);
    }
    if (weapon.ends_with(".blk")) weapon.remove_suffix(4);
    std::string key;
    key.reserve(weapon.size());
    for (char c : weapon) if (c != '_' && c != '-') key += c;
    static constexpr std::string_view a2a[] = {
        "airtoair", "swsflzlwf63",
        "aim9", "aim9b", "aim9d", "aim9e", "aim9g", "aim9h", "aim9j", "aim9l", "aim9m", "aim9p", "aim9x",
        "aim7", "aim7d", "aim7e", "aim7e2", "aim7f", "aim7m", "aim7p",
        "aim54", "aim54a", "aim54c", "aim120", "aim120a", "aim120b", "aim120c", "aim120c5", "aim120c7", "aim120d",
        "aim132", "aim174b", "r3s", "r3r", "r13m", "r13m1", "r23r", "r23t", "r24r", "r24t",
        "r27", "r27r", "r27t", "r27er", "r27et", "r33", "r37", "r37m", "r40r", "r40t",
        "r60", "r60m", "r73", "r74", "r77", "r771", "pl2", "pl5b", "pl5c", "pl7", "pl8", "pl10", "pl12", "pl15", "pl17",
        "rb24", "rb24j", "rb27", "rb28", "rb71", "rb74", "rb99", "aam3", "aam4", "aam5",
        "skyflash", "firestreak", "redtop", "meteor", "shafrir", "shafrir2", "r530", "r550", "magic2"
    };
    static constexpr std::string_view a2g[] = {
        "airtoground", "airtosurface", "agm12", "agm12b", "agm12c", "agm45", "agm62", "agm65", "agm65a", "agm65b", "agm65d", "agm65g", "agm65h", "agm65k",
        "agm78", "agm84", "agm84a", "agm84d", "agm88", "agm88b", "agm88c", "agm114", "agm114b", "agm114k", "agm119",
        "kh23", "kh23m", "kh25", "kh25ml", "kh25mp", "kh29", "kh29l", "kh29t", "kh31", "kh31p", "kh38", "kh38ml", "kh38mt", "kh59"
    };
    static constexpr std::string_view g2a[] = {
        "groundtoair", "surfacetoair", "9m31", "9m32", "9m33", "9m33m2", "9m33m3", "9m37", "9m37m",
        "9m38", "9m38m1", "9m39", "9m311", "9m311m", "9m330", "9m331", "9m338", "48n6", "48n6e", "57e6", "57e6e",
        "fim43", "fim92", "fim92a", "fim92c", "fim92e", "mim23", "mim23a", "mim23b", "mim72", "mim104"
    };
    for (auto known : a2a) if (key == known) return category::air_to_air;
    for (auto known : a2g) if (key == known) return category::air_to_ground;
    for (auto known : g2a) if (key == known) return category::ground_to_air;
    return category::unknown;
}
inline constexpr bool valid_category(category value) noexcept {
    return value >= category::air_to_air && value <= category::unknown;
}
// Profile-only exact-name metadata. Never observes names or writes the registry.
inline std::mutex category_mutex;
inline std::map<std::string, category> category_overrides;
inline std::atomic<std::uint64_t> category_revision{0};
inline constexpr std::size_t max_category_overrides = 16384;
inline bool category_name_allowed(std::string_view name) {
    if (name.empty() || name.size() > 63) return false;
    std::string lower;
    for (unsigned char c : name) {
        if (c < 0x21 || c > 0x7e || c == '=') return false;
        lower += c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : static_cast<char>(c);
    }
    return lower.find("flare") == std::string::npos && lower.find("mm") == std::string::npos;
}
inline category classify(std::string_view name) {
    if (!category_name_allowed(name)) return category::unknown;
    std::lock_guard lock(category_mutex);
    const auto it = category_overrides.find(std::string(name));
    return it != category_overrides.end() ? it->second : classify_automatic(name);
}
inline bool set_category_override(const std::string& name, category value) {
    if (!category_name_allowed(name) || !valid_category(value)) return false;
    std::lock_guard lock(category_mutex);
    if (!category_overrides.contains(name) && category_overrides.size() >= max_category_overrides) return false;
    category_overrides[name] = value;
    category_revision.fetch_add(1, std::memory_order_relaxed);
    return true;
}
inline void clear_category_override(const std::string& name) {
    std::lock_guard lock(category_mutex);
    if (category_overrides.erase(name)) category_revision.fetch_add(1, std::memory_order_relaxed);
}
inline std::map<std::string, category> capture_category_overrides() {
    std::lock_guard lock(category_mutex);
    return category_overrides;
}
inline void apply_category_overrides(const std::map<std::string, category>& values) {
    std::lock_guard lock(category_mutex);
    category_overrides.clear();
    for (const auto& [name, value] : values) {
        if (category_overrides.size() >= max_category_overrides) break;
        if (category_name_allowed(name) && valid_category(value)) category_overrides.emplace(name, value);
    }
    category_revision.fetch_add(1, std::memory_order_relaxed);
}
}
