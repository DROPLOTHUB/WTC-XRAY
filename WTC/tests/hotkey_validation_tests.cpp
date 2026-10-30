#include "../src/core/hotkey_validation.hpp"
#include <cassert>
#include <cstdio>
int main() {
    std::array<int,9> keys{0x41,0x42,0x41,0x70,300,-1,0x1b,0x08,0};
    assert(hotkey_validation::taken(keys,0,0x41));
    assert(!hotkey_validation::taken(keys,0,0));
    assert(hotkey_validation::sanitize(keys)==6);
    assert(keys[0]==0x41 && keys[1]==0x42);
    for(std::size_t i=2;i<keys.size();i++) assert(keys[i]==0);
    assert(!hotkey_validation::taken(keys,0,0x43));
    assert(!hotkey_validation::valid(0x71));
    assert(hotkey_validation::valid(0));
    std::puts("PASS reserved, duplicate, out-of-range and free hotkeys");
}
