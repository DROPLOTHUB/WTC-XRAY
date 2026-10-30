#include "../src/features/esp/projectile_visibility.hpp"
#include "../src/features/esp/radar_symbols.hpp"
#include <cassert>
#include <cstdio>
int main() {
    namespace missile=projectile_visibility;
    namespace radar=esp::radar::icons;
    missile::apply({});
    assert(missile::allow("ffa_p16^sws_flz_lwf_63.blk",true));
    assert(!missile::allow("agm_65.blk",true));
    assert(!missile::allow("flare_aim_9",false));
    assert(!missile::allow("100mm_9m_117m1",false));
    missile::set("127MM_spike_lr2",true);
    assert(!missile::capture().contains("127MM_spike_lr2"));
    auto seen=missile::entries(true);
    assert(seen.size()==2);
    missile::set("agm_65.blk",true);
    missile::set("ffa_p16^sws_flz_lwf_63.blk",false);
    assert(missile::allow("agm_65.blk",true));
    assert(!missile::allow("ffa_p16^sws_flz_lwf_63.blk",false));
    auto selected=missile::capture();
    assert(selected.size()==2);
    missile::apply({});
    assert(!missile::allow("agm_65.blk",true));
    missile::apply(selected);
    assert(missile::allow("agm_65.blk",true));
    for (const auto& [name,enabled]:selected) {
        const auto hex=missile::hex_encode(name);
        assert(missile::hex_decode(hex)==name);
        assert(missile::capture().at(name)==enabled);
    }
    assert(missile::hex_decode("X0").empty());
    assert(missile::hex_decode("0").empty());
    assert(!missile::valid_name("bad\nname"));
    assert(!missile::allow("unselected_rocket_42.blk",true));
    const auto observed_before_save=missile::capture_observed();
    assert(observed_before_save.size()==3);
    std::vector<std::string> reloaded;
    for (const auto& name:observed_before_save)
        reloaded.push_back(missile::hex_decode(missile::hex_encode(name)));
    { std::lock_guard lock(missile::mutex); missile::observed.clear(); } // nueva sesion
    assert(missile::capture_observed().empty());
    reloaded.push_back("flare_aim_9");
    reloaded.push_back("150mm_shin33");
    reloaded.push_back("invalid\nname");
    missile::restore_observed(reloaded);
    assert(missile::capture_observed()==observed_before_save);
    assert(missile::entries(true).size()==3);
    assert(radar::classify("","exp_ship")==radar::kind::naval);
    assert(radar::classify("","exp_building")==radar::kind::structure);
    assert(radar::classify("","exp_fighter")==radar::kind::fighter);
    for (std::size_t i=0;i<radar::visible.size();++i) {
        assert(radar::enabled(static_cast<radar::kind>(i)));
        assert(radar::labels[i][0]);
    }
    radar::visible[static_cast<std::size_t>(radar::kind::structure)]=false;
    assert(!radar::enabled(radar::kind::structure));
    assert(radar::enabled(radar::kind::fighter));
    std::puts("PASS exact missile-name overrides, bounded names, radar categories and visibility");
}
