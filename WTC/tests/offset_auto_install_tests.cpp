#include "../src/core/offset_auto_update.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
int main() {
    const auto unique=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/".validation"/"auto_offset_test"/unique;
    std::filesystem::create_directories(root);
    const auto path=std::filesystem::absolute(root/"api-offsets.cache");
    const auto legacy=std::filesystem::absolute(root/"offsets.txt");
    {std::ofstream manual(legacy);manual<<"cgame_offset=0xBADF00D\n";}
    offset_api_payload::manifest ignored;
    std::string error;std::size_t mapped=0;
    assert(!offset_auto_update::read_cache(legacy,ignored,error));
    assert(offset_auto_update::previous_version(legacy).empty());
    std::ifstream in(std::filesystem::path(__FILE__).parent_path()/"free_offsets_fixture.json");
    assert(in.is_open());
    const std::string json((std::istreambuf_iterator<char>(in)),{});
    offset_api_payload::manifest manifest;
    assert(offset_api_payload::parse(json,manifest,error));
    assert(!offset_auto_update::install_at(legacy,manifest,mapped,error));
    assert(!offset_auto_update::install_at(path,manifest,mapped,error));
    assert(offset_auto_update::get_applied_version()=="Offsets incorporados (sin API activa)");
    manifest.entries["Global.g_EntityManager"]=0x7E8CBE8;
    manifest.version="999.1.1.1";
    assert(offset_auto_update::install_at(path,manifest,mapped,error));
    assert(mapped>=45 && std::filesystem::exists(path));
    {std::ifstream encrypted(path,std::ios::binary);
     const std::string bytes((std::istreambuf_iterator<char>(encrypted)),{});
     assert(bytes.find("999.1.1.1")==std::string::npos);}

    offset_api_payload::manifest cached;
    assert(offset_auto_update::read_cache(path,cached,error));
    assert(cached.entries.at("Global.g_EntityManager")==0x7E8CBE8);
    manifest.version="999.1.1.2";
    manifest.entries["Global.g_RocketListIndexPtr"]+=0x50;
    assert(offset_auto_update::install_at(path,manifest,mapped,error));
    auto backup=path;backup+=L".bak";
    assert(std::filesystem::exists(backup));
    assert(offset_auto_update::previous_version(path)=="999.1.1.2");
    assert(offset_auto_update::previous_version(backup)=="999.1.1.1");
    manifest.version="998.9.0.0";
    assert(!offset_auto_update::install_at(path,manifest,mapped,error));
    assert(offset_auto_update::previous_version(path)=="999.1.1.2");
    assert(offset_auto_update::get_applied_version()=="Offsets incorporados (sin API activa)"); // install is not apply
    offset_auto_update::restore_built_in_offsets();
    const auto original=offsets::Global::g_RocketListIndexPtr;
    assert(offset_auto_update::read_cache(path,cached,error));
    assert(offset_auto_update::apply_manifest(cached));
    assert(offset_auto_update::get_applied_version()=="API 999.1.1.2");
    assert(offsets::api_ground_velocity_ready);
    assert(!offsets::api_ballistics_ready); // free fixture has no active round fields
    assert(offsets::Global::g_RocketListIndexPtr==manifest.entries.at("Global.g_RocketListIndexPtr"));
    assert(offsets::Global::g_RocketListIndexPtr!=original);
    assert(offset_auto_update::load_backup_for_recovery_at(path));
    assert(offset_auto_update::get_applied_version()=="API 999.1.1.1");
    assert(offsets::Global::g_RocketListIndexPtr==manifest.entries.at("Global.g_RocketListIndexPtr")-0x50);
    assert(offset_auto_update::accept_recovered_backup_at(path));
    assert(offset_auto_update::previous_version(path)=="999.1.1.1");
    offset_auto_update::restore_built_in_offsets();
    assert(offset_auto_update::get_applied_version()=="Offsets incorporados (sin API activa)");
    {std::fstream corrupt(path,std::ios::binary|std::ios::in|std::ios::out);char c{};corrupt.get(c);corrupt.seekp(0);c^=0x7f;corrupt.put(c);}
    assert(!offset_auto_update::read_cache(path,cached,error));
    assert(offset_auto_update::read_cache(backup,cached,error));
    std::puts("PASS DPAPI API-only cache, legacy rejection, version rollback and corruption recovery");
}
