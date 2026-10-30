#include "../src/core/missile_registry_storage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
int main() {
    namespace registry=missile_registry_storage;
    namespace catalog=projectile_visibility;
    const auto path=std::filesystem::path(__FILE__).parent_path().parent_path()/"build"/
        ("missile-registry-test-"+std::to_string(GetCurrentProcessId()))/"registry.txt";
    assert(!std::filesystem::exists(path));
    assert(registry::load_at(path));
    assert(!catalog::allow("agm_65.blk",true));
    assert(catalog::allow("ffa_p16^sws_flz_lwf_63.blk",true));
    assert(!catalog::allow("flare_aim_9",false));
    assert(catalog::registry_pending().size()==2);
    assert(registry::flush_at(path));
    assert(catalog::registry_pending().empty());
    std::ifstream first(path);
    const std::string saved((std::istreambuf_iterator<char>(first)),std::istreambuf_iterator<char>());
    assert(saved.find("agm_65.blk\n")!=std::string::npos);
    assert(saved.find("ffa_p16^sws_flz_lwf_63.blk\n")!=std::string::npos);
    assert(saved.find("flare")==std::string::npos);
    first.close();
    { std::lock_guard lock(catalog::mutex); catalog::observed.clear(); catalog::pending_registry.clear(); }
    assert(registry::load_at(path));
    assert(catalog::capture_observed().size()==2);
    assert(catalog::registry_pending().empty()); // cargar nunca reescribe el registro
    { std::ofstream out(path,std::ios::app); out << "agm_65.blk\n" << "100mm_9m_117m1\n" << "bad name\n" << "torn_without_newline"; }
    { std::lock_guard lock(catalog::mutex); catalog::observed.clear(); }
    assert(registry::load_at(path));
    assert(catalog::capture_observed().size()==2); // deduplicado y truncados ignorados
    catalog::restore_observed({"migrated_from_config.blk"});
    assert(catalog::registry_pending().size()==1);
    assert(registry::flush_at(path));
    assert(catalog::registry_pending().empty());
    { std::lock_guard lock(catalog::mutex); catalog::observed.clear(); }
    assert(registry::load_at(path));
    assert(catalog::capture_observed().size()==3); // migrado, no linea truncada
    catalog::allow("r_73.blk",true);
    assert(!registry::flush_at(path.parent_path())); // ruta es un directorio
    assert(catalog::registry_pending().size()==1); // no perder datos al fallar
    std::puts("PASS automatic missile journal reload, malformed records and write failure");
}
