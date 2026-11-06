#include "../src/features/esp/projectile_visibility.hpp"
#include <cassert>
#include <cstdio>
#include <thread>

namespace pv = projectile_visibility;
static void reset() {
    std::lock_guard lock(pv::mutex);
    pv::observed.clear(); pv::overrides.clear(); pv::pending_registry.clear();
    pv::aam_cache.clear(); pv::catalog_full = false;
}
int main() {
    using C = pv::category;
    for (auto name : {"AIM-9L.blk", "aim_120_c5", "r_73.blk", "plane^R-27ER.blk", "ffa_p16^sws_flz_lwf_63.blk"})
        assert(pv::classify(name) == C::air_to_air);
    for (auto name : {"AGM-65D.blk", "kh_29t", "plane^kh_38_mt.blk", "air_to_ground"})
        assert(pv::classify(name) == C::air_to_ground);
    for (auto name : {"9M_311.blk", "9m331", "FIM-92C.blk", "ground_to_air"})
        assert(pv::classify(name) == C::ground_to_air);
    for (auto name : {"", "unknown", "rocket", "sam", "9m", "9m_117", "r_3", "kh_999", "aim_900", "aim9banana",
        "not_aim_9", "aim9_agm65", "aim9.blk.extra", "mica", "python", "derby", "iris_t", "aspide", "spike", "starstreak",
        "bad name", "aim9=1", "flare_aim9", "100MM_aim9", "a^b^aim9", "^aim9"})
        assert(pv::classify(name) == C::unknown);
    assert(pv::classify(std::string(64, 'a')) == C::unknown);
    assert(pv::classify(std::string_view("aim9\0junk", 9)) == C::unknown);
    assert(std::string_view(pv::category_label(C::air_to_air)) == "A2A");
    assert(std::string_view(pv::category_label(C::air_to_ground)) == "A2G");
    assert(std::string_view(pv::category_label(C::ground_to_air)) == "G2A");
    assert(std::string_view(pv::category_label(C::unknown)) == "Unknown");
    assert(std::string_view(pv::category_label(static_cast<C>(99))) == "Unknown");

    reset();
    pv::restore_observed({"aim_9.blk", "agm_65.blk", "unknown.blk"}, false);
    pv::set("profile_only.blk", true);
    const auto names = pv::capture_observed();
    const auto pending = pv::registry_pending();
    const auto revision = pv::revision.load();
    pv::set_all(false);
    assert(pv::revision.load() == revision + 1);
    assert(pv::capture().size() == 4);
    for (auto [name, selected] : pv::entries(false)) {
        assert(!selected);
        assert(!pv::allow(name.c_str(), false));
        assert(!pv::allow(name.c_str(), true));
    }
    auto saved = pv::capture();
    pv::apply({}); pv::apply(saved);
    assert(pv::capture() == saved);
    for (const auto& [name, selected] : saved)
        assert(pv::hex_decode(pv::hex_encode(name)) == name);
    // Classifier never changes the legacy AAM policy (even ambiguous names).
    pv::set_all(true);
    assert(pv::capture().empty());
    assert(pv::allow("mica", true));
    assert(!pv::allow("agm_65.blk", true));
    assert(pv::allow("agm_65.blk", false));
    assert(pv::allow("future_unknown.blk", false));
    assert(!pv::allow("flare_aim9", false));
    assert(!pv::allow("100MM_aim9", false));
    assert(!pv::allow(nullptr, false));
    assert(!pv::allow("bad name", false));

    reset();
    pv::restore_observed(names, false);
    pv::set("profile_only.blk", true);
    pv::set_all(false); pv::set_all(true);
    assert(pv::capture_observed() == names);
    assert(pv::registry_pending() == pending); // bulk never queues journal writes
    assert(!pv::registry_capacity_reached());
    pv::apply({{"flare_aim9", true}, {"100mm_aim9", true}, {"bad name", true}, {"valid", false}});
    assert(pv::capture().size() == 1);

    reset();
    std::vector<std::string> full;
    for (std::size_t i = 0; i < pv::max_names; ++i) full.push_back("observed_" + std::to_string(i));
    pv::restore_observed(full, false);
    // Disjoint profile + full registry: bulk must not silently skip any row.
    pv::set("profile_only", true);
    pv::set_all(false);
    assert(pv::capture().size() == pv::max_names + 1);
    saved = pv::capture();
    pv::apply({}); pv::apply(saved);
    assert(pv::capture() == saved);
    for (const auto& [name, selected] : pv::entries(false)) assert(!selected);
    assert(pv::allow("beyond_catalog", false)); // deselect is current-only
    assert(pv::registry_capacity_reached());
    pv::set("beyond_catalog", false); // may be capped; global select must still work
    pv::set_all(true);
    assert(pv::capture().empty());
    assert(pv::allow("beyond_catalog", false));
    assert(pv::allow("never_seen_before", false));
    assert(pv::observed_count() == pv::max_names);
    assert(pv::registry_pending().empty());

    reset();
    pv::restore_observed({"aim9", "agm65", "9m311"}, false);
    std::thread writer([] { for (int i = 0; i < 500; ++i) pv::set_all(i % 2 == 0); });
    for (int i = 0; i < 500; ++i) {
        const auto rows = pv::entries(false);
        assert(rows.size() == 3);
        assert(rows[0].second == rows[1].second && rows[1].second == rows[2].second);
    }
    writer.join();
    std::puts("PASS conservative missile categories, locked bulk selection, exclusions and profile/registry compatibility");
}
