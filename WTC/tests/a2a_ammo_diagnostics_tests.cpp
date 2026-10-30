#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static std::string read(const std::filesystem::path& file) {
    std::ifstream f(file);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
int main() {
    const auto src=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    const auto a=read(src/"features/aimbot/aimbot.hpp");
    const auto c=read(src/"core/core.hpp");
    assert(c.find("if(sdk::init())")!=std::string::npos);
    const auto m=read(src/"gui/tactical_menu.hpp");
    for(const auto stage:{"ID de cartucho no legible","Velocidad del cartucho no legible",
        "Tabla de velocidades no disponible","ID de proyectil fuera de tabla",
        "Entrada de velocidad no disponible","Velocidad de tabla no legible"})
        assert(a.find(stage)!=std::string::npos);
    assert(a.find("inline bool GetAircraftBallisticsInfo(uintptr_t snapshot_owner=0)")!=std::string::npos);
    assert(a.find("BallisticsData& result=ballisticsData;")!=std::string::npos);
    assert(a.find("independent_output")==std::string::npos);
    assert(a.find("if (!aircraft_ballistics_policy::real_speed(speed)) return false;")!=std::string::npos);
    assert(a.find("diag.cartridge_read=TargetProcess->ReadOptional(ammo+0x20,&speed,sizeof(speed))")!=std::string::npos);
    assert(a.find("diag.table_read=TargetProcess->ReadOptional(entry+0x20,&speed,sizeof(speed))")!=std::string::npos);
    assert(m.find("Diagnostico municion A2A (sin claves ni punteros)")!=std::string::npos);
    assert(m.find("d.cartridge_speed")!=std::string::npos);
    assert(m.find("d.table_speed")!=std::string::npos);
    std::puts("PASS local offset provenance, isolated A2A ammo failure stages and safe UI");
}
