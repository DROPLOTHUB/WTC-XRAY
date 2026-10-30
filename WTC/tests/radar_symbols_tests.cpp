#include "../src/features/esp/radar_symbols.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace esp::radar::icons;
    assert(classify("","exp_fighter")==kind::fighter&&game_glyph(kind::fighter)==0x2524);
    assert(classify("","exp_assault")==kind::strike&&game_glyph(kind::strike)==0x251E);
    assert(classify("","exp_bomber")==kind::bomber&&game_glyph(kind::bomber)==0x2520);
    assert(classify("","exp_helicopter")==kind::helicopter&&game_glyph(kind::helicopter)==0x2522);
    assert(classify("Medium","exp_tank")==kind::medium);
    assert(classify("MBT","exp_tank")==kind::medium);
    assert(classify("Light Cruiser","exp_tank")==kind::light);
    assert(classify("Armored","exp_tank")==kind::light);
    assert(classify("SPG","exp_tank")==kind::heavy);
    assert(classify("","exp_SPAA")==kind::spaa&&game_glyph(kind::spaa)==0x2530);
    assert(classify("","exp_tank_destroyer")==kind::tank_destroyer&&game_glyph(kind::tank_destroyer)==0x2534);
    assert(classify("","exp_heavy_tank")==kind::heavy&&game_glyph(kind::heavy)==0x2528);
    assert(classify("","exp_tank")==kind::medium&&game_glyph(kind::medium)==0x252C);
    assert(classify("","human/soldier")==kind::light&&game_glyph(kind::light)==0x252A);
    assert(classify("","unknown")==kind::unknown);
    std::puts("PASS aircraft silhouettes, tank subtypes and supplied game glyph IDs");
}
