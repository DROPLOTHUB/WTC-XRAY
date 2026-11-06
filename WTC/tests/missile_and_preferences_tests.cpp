#include <cassert>
#include <cmath>
#include <cstdio>
#include "../src/features/esp/projectile_visibility.hpp"
#include "../src/features/esp/missile_alerts.hpp"
namespace pv = projectile_visibility;
using C = pv::category;
static void reset() {
    std::lock_guard lock(pv::mutex);
    pv::observed.clear(); pv::overrides.clear(); pv::pending_registry.clear();
    pv::aam_cache.clear(); pv::catalog_full = false;
    pv::future_category_policy = {};
}
int main() {
    reset();
    pv::restore_observed({"mystery_a2g.blk","mystery_g2a.blk","aim_9.blk"}, false);
    assert(pv::classify("mystery_a2g.blk")==C::unknown);
    assert(pv::set_category_override("mystery_a2g.blk",C::air_to_ground));
    assert(pv::classify("mystery_a2g.blk")==C::air_to_ground);
    assert(pv::classify_automatic("mystery_a2g.blk")==C::unknown);
    assert(!pv::set_category_override("flare_x",C::air_to_air));
    assert(!pv::set_category_override("100mm_x",C::air_to_air));
    assert(!pv::set_category_override("bad name",C::air_to_air));
    assert(!pv::set_category_override("ok",static_cast<C>(9)));
    assert(!pv::set_category_override(std::string(64,'a'),C::air_to_air));
    const auto before=pv::category_revision.load();
    pv::clear_category_override("mystery_a2g.blk");
    assert(pv::category_revision.load()>before);
    assert(pv::classify("mystery_a2g.blk")==C::unknown);
    pv::set_category_override("mystery_g2a.blk",C::ground_to_air);
    pv::set_category_policy(C::ground_to_air,pv::category_policy::hide);
    assert(!pv::allow("mystery_g2a.blk",false));
    assert(pv::allow("a_unseen_new.blk",false)); // auto-classified unknown: policy not applicable
    assert(pv::allow("future_unknown.blk",false));
    assert(!pv::allow("future_unknown.blk",true));
    pv::set_category_policy(C::ground_to_air,pv::category_policy::show);
    assert(pv::allow("mystery_g2a.blk",true));
    pv::set("mystery_g2a.blk",false);
    assert(!pv::allow("mystery_g2a.blk",false));
    assert(pv::allow("a_unseen_new.blk",false));
    auto policies=pv::capture_category_policy();
    policies[0]=pv::category_policy::show;
    pv::apply_category_policy(policies);
    assert(pv::allow("aim_9.blk",true));
    assert(pv::allow("aim_9.blk",false));
    policies[0]=static_cast<pv::category_policy>(7);
    pv::apply_category_policy(policies);
    assert(pv::capture_category_policy()[0]==pv::category_policy::legacy);
    reset();
    pv::restore_observed({"aim9","agm65","9m311","unknownx"}, false);
    pv::set_category_override("unknownx",C::ground_to_air);
    pv::set_category_policy(C::ground_to_air,pv::category_policy::hide);
    const auto pending=pv::registry_pending();
    const auto count=pv::set_current_category_visibility(C::ground_to_air,true);
    assert(count>=2);
    assert(pv::allow("unknownx",false));
    assert(pv::allow("9m311",false));
    assert(pv::capture_category_policy()[2]==pv::category_policy::hide);
    assert(pv::registry_pending()==pending);
    assert(pv::observed_count()==4);
    auto overrides=pv::capture_category_overrides();
    auto policy=pv::capture_category_policy();
    pv::apply_category_overrides({});
    pv::apply_category_policy({});
    assert(pv::capture_category_overrides().empty());
    pv::apply_category_overrides(overrides);
    pv::apply_category_policy(policy);
    assert(pv::capture_category_overrides()==overrides && pv::capture_category_policy()==policy);
    pv::set_category_policy(C::air_to_ground,pv::category_policy::hide);
    pv::apply_category_policy({});
    pv::set_all(true);
    assert(pv::capture().empty());
    assert(pv::allow("aim9",false) && pv::allow("agm65",false));

    using namespace missile_alerts;
    Settings cfg; cfg.enabled=true;
    const vec3 zero{0,0,0};
    const auto incoming=evaluate({0,0,1000},{0,0,-200},false,"aim9",zero,zero,true,cfg);
    assert(incoming.approaching && incoming.range_m>999 && incoming.closure_mps>199);
    assert(incoming.time_to_closest_s>4 && incoming.miss_distance_m<1);
    assert(!evaluate({0,0,1000},{0,0,200},false,"aim9",zero,zero,true,cfg).approaching);
    assert(!evaluate({0,0,1000},{0,0,-20},false,"aim9",zero,zero,true,cfg).approaching);
    assert(!evaluate({0,0,1000},{0,0,-200},true,"bomb",zero,zero,true,cfg).approaching);
    assert(!evaluate({0,0,1000},{0,0,-200},false,"aim9",zero,zero,false,cfg).approaching);
    assert(!evaluate({0,0,1000},{0,0,-200},false,"aim9",zero,zero,true,Settings{}).approaching);
    assert(!evaluate({0,0,1000},{0,0,-200},false,"09mm_x",zero,zero,true,cfg).approaching);
    assert(!evaluate({NAN,0,0},{0,0,-200},false,"aim9",zero,zero,true,cfg).approaching);
    assert(!evaluate({0,0,4000},{0,0,-200},false,"aim9",zero,zero,true,cfg).approaching);
    assert(!evaluate({0,0,1000},{0,0,-30},false,"aim9",zero,zero,true,cfg).approaching);
    const auto earlier=evaluate({0,0,3000},{0,0,-80},false,"aim9",zero,zero,true,cfg);
    assert(!earlier.approaching && earlier.miss_distance_m==0);
    Settings sanitized; sanitized.max_range_m=NAN;sanitized.min_closure_mps=-5;
    sanitized.max_time_s=1e9;sanitized.max_miss_distance_m=INFINITY;sanitized.audio_interval_s=0;
    apply_settings(sanitized);
    const auto applied=capture_settings();
    assert(applied.max_range_m==3000 && applied.min_closure_mps>=1 && applied.max_time_s<=120 &&
        applied.max_miss_distance_m==150 && applied.audio_interval_s>=1 && !applied.enabled && !applied.audio_enabled);
    std::puts("PASS manual missile categories, future rules, bounded category selection and geometric approach alerts");
}
