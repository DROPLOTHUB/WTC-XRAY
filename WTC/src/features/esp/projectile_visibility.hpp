#pragma once
#include <atomic>
#include <array>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "projectile_ecs.hpp"
#include "missile_categories.hpp"

// Nombres exactos observados en el ECS; los overrides pertenecen al perfil activo.
// Ninguna observacion escribe configuracion en disco.
namespace projectile_visibility {
inline constexpr std::size_t max_names = 16384; // limite defensivo, no una lista fija de misiles
inline std::mutex mutex;
inline std::set<std::string> observed;
inline std::set<std::string> pending_registry;
inline bool catalog_full = false;
inline std::map<std::string, bool> overrides;
inline std::map<std::string, bool> aam_cache;
inline std::atomic<std::uint64_t> revision{0};

inline bool valid_name(std::string_view name) {
    if (name.empty() || name.size() > 63) return false;
    for (unsigned char c : name)
        if (c < 0x21 || c > 0x7e || c == '=') return false;
    return true;
}
// Per-category defaults apply to unoverridden names, including future discoveries.
// Explicit per-name visibility wins; legacy preserves the historical AAM switch.
enum class category_policy { legacy, show, hide };
using category_policy_map = std::array<category_policy, 4>;
inline category_policy_map future_category_policy{};
inline category_policy_map capture_category_policy() {
    std::lock_guard lock(mutex);
    return future_category_policy;
}
inline void apply_category_policy(category_policy_map values) {
    for (auto& value : values)
        if (value != category_policy::show && value != category_policy::hide) value = category_policy::legacy;
    std::lock_guard lock(mutex);
    future_category_policy = values;
    revision.fetch_add(1, std::memory_order_relaxed);
}
inline void set_category_policy(category group, category_policy value) {
    if (!valid_category(group)) return;
    if (value != category_policy::show && value != category_policy::hide) value = category_policy::legacy;
    std::lock_guard lock(mutex);
    future_category_policy[static_cast<std::size_t>(group)] = value;
    revision.fetch_add(1, std::memory_order_relaxed);
}
// Requires visibility mutex; category mutex is always acquired second.
inline bool effective_visibility_locked(const std::string& key, bool aam_default) {
    if (auto it = overrides.find(key); it != overrides.end()) return it->second;
    const auto policy = future_category_policy[static_cast<std::size_t>(classify(key))];
    if (policy != category_policy::legacy) return policy == category_policy::show;
    if (!aam_default) return true;
    if (auto it = aam_cache.find(key); it != aam_cache.end()) return it->second;
    const bool is_aam = projectile_ecs::name_is_air_to_air(key.c_str());
    if (aam_cache.size() < max_names) aam_cache.emplace(key, is_aam);
    return is_aam;
}
inline bool allow(const char* raw, bool aam_default) {
    if (!raw) return false;
    const std::string_view name(raw);
    if (!valid_name(name) || projectile_ecs::name_has_flare(raw) ||
        projectile_ecs::name_has_mm(raw)) return false;
    const std::string key(name);
    std::lock_guard lock(mutex);
    if (!observed.contains(key)) {
        if (observed.size() < max_names) {
            observed.emplace(key);
            pending_registry.emplace(key);
            revision.fetch_add(1,std::memory_order_relaxed);
        } else catalog_full = true;
    }
    return effective_visibility_locked(key, aam_default);
}
inline bool callback(const char* raw, void* context) {
    return allow(raw, *static_cast<const bool*>(context));
}
inline std::vector<std::pair<std::string,bool>> entries(bool aam_default) {
    std::lock_guard lock(mutex);
    std::vector<std::pair<std::string,bool>> result;
    result.reserve(observed.size() + overrides.size());
    for (const auto& name : observed) {
        if (projectile_ecs::name_has_mm(name.c_str())) continue;
        result.emplace_back(name, effective_visibility_locked(name, aam_default));
    }
    for (const auto& [name, enabled] : overrides)
        if (!observed.contains(name) && !projectile_ecs::name_has_mm(name.c_str()))
            result.emplace_back(name, enabled);
    return result;
}
inline void set(const std::string& name, bool enabled) {
    if (!valid_name(name) || projectile_ecs::name_has_flare(name.c_str()) ||
        projectile_ecs::name_has_mm(name.c_str())) return;
    std::lock_guard lock(mutex);
    if (overrides.size() < max_names || overrides.contains(name) || observed.contains(name)) {
        overrides[name] = enabled;
        revision.fetch_add(1,std::memory_order_relaxed);
    }
}
// Global select: caller also sets its existing aam_default=false. Clearing
// overrides selects even valid names beyond the catalog cap and future names
// under legacy category policy. This legacy API does NOT reset category policy;
// a UI global reset may explicitly call apply_category_policy({}) as well.
// Deselect affects the entire current union, NOT future/unlisted names: those
// continue to follow aam_default. No disk/registry I/O or classification change.
inline void set_all(bool enabled) {
    std::lock_guard lock(mutex);
    if (enabled) overrides.clear();
    else {
        for (auto& [name, selected] : overrides) selected = false;
        for (const auto& name : observed) overrides[name] = false;
    }
    revision.fetch_add(1, std::memory_order_relaxed);
}
// Unlike set_all(true), this writes explicit selections only for the current
// observed/profile union. It NEVER changes future policy, classification or registry.
inline std::size_t set_current_category_visibility(category group, bool enabled) {
    if (!valid_category(group)) return 0;
    std::lock_guard lock(mutex);
    // Snapshot manual metadata once: concurrent edits cannot split a bulk action.
    const auto manual = capture_category_overrides();
    const auto matches = [&](const std::string& name) {
        if (!category_name_allowed(name)) return false;
        const auto it = manual.find(name);
        return (it == manual.end() ? classify_automatic(name) : it->second) == group;
    };
    std::size_t count = 0;
    for (auto& [name, selected] : overrides) if (matches(name)) { selected = enabled; ++count; }
    for (const auto& name : observed)
        if (!overrides.contains(name) && matches(name)) { overrides.emplace(name, enabled); ++count; }
    if (count) revision.fetch_add(1, std::memory_order_relaxed);
    return count;
}
inline std::size_t observed_count() {
    std::lock_guard lock(mutex);
    return observed.size();
}
inline std::vector<std::string> capture_observed() {
    std::lock_guard lock(mutex);
    return {observed.begin(), observed.end()};
}
inline void restore_observed(const std::vector<std::string>& names, bool queue_for_registry = true) {
    std::lock_guard lock(mutex);
    for (const auto& name : names) {
        if (!valid_name(name) || projectile_ecs::name_has_flare(name.c_str()) ||
            projectile_ecs::name_has_mm(name.c_str()) || observed.contains(name)) continue;
        if (observed.size() >= max_names) { catalog_full = true; break; }
        observed.emplace(name);
        if (queue_for_registry) pending_registry.emplace(name);
        revision.fetch_add(1,std::memory_order_relaxed);
    }
}
inline bool registry_has_pending() {
    std::lock_guard lock(mutex);
    return !pending_registry.empty();
}
inline std::vector<std::string> registry_pending() {
    std::lock_guard lock(mutex);
    return {pending_registry.begin(), pending_registry.end()};
}
inline void registry_acknowledge(const std::vector<std::string>& written) {
    std::lock_guard lock(mutex);
    for (const auto& name : written) pending_registry.erase(name);
}
inline bool registry_capacity_reached() {
    std::lock_guard lock(mutex);
    return catalog_full;
}
inline std::map<std::string,bool> capture() {
    std::lock_guard lock(mutex);
    return overrides;
}
inline void apply(const std::map<std::string,bool>& values) {
    std::lock_guard lock(mutex);
    overrides.clear();
    // Bulk deselection can cover both the bounded registry and a disjoint
    // profile. Retain every registered name plus at most max_names unobserved
    // names, so capture/apply round-trips this union without losing selections.
    std::size_t unobserved = 0;
    for (const auto& [name, enabled] : values) {
        if (!valid_name(name) || projectile_ecs::name_has_flare(name.c_str()) ||
            projectile_ecs::name_has_mm(name.c_str())) continue;
        if (!observed.contains(name)) {
            if (unobserved >= max_names) continue;
            ++unobserved;
        }
        overrides.emplace(name, enabled);
    }
    revision.fetch_add(1,std::memory_order_relaxed);
}
inline std::string hex_encode(std::string_view name) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result; result.reserve(name.size()*2);
    for (unsigned char c : name) { result += hex[c >> 4]; result += hex[c & 15]; }
    return result;
}
inline std::string hex_decode(std::string_view text) {
    if (text.size() % 2 || text.size() > 126) return {};
    auto digit=[](char c)->int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    std::string name; name.reserve(text.size()/2);
    for (std::size_t i=0; i<text.size(); i+=2) {
        const int high=digit(text[i]), low=digit(text[i+1]);
        if (high < 0 || low < 0) return {};
        name += static_cast<char>((high << 4) | low);
    }
    return valid_name(name) ? name : std::string{};
}
}
