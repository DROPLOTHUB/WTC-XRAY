#pragma once
#include <array>
#include <cstddef>

namespace hotkey_validation
{
    // F1/F2 belong to the menu; Escape cancels capture, Backspace clears it.
    inline bool valid(int key) {
        return key >= 0 && key <= 255 && key != 0x70 && key != 0x71 && key != 0x1b && key != 0x08;
    }
    template<std::size_t N> inline bool taken(const std::array<int,N>& keys, std::size_t owner, int key) {
        if (key == 0) return false;
        for (std::size_t i=0;i<N;i++) if(i!=owner && keys[i]==key) return true;
        return false;
    }
    // Keep the earliest binding when an imported profile defines a duplicate.
    template<std::size_t N> inline int sanitize(std::array<int,N>& keys) {
        int repaired=0;
        for (std::size_t i=0;i<N;i++) {
            if(!valid(keys[i])) { keys[i]=0; ++repaired; continue; }
            if(keys[i]==0) continue;
            for(std::size_t j=0;j<i;j++) if(keys[j]==keys[i]) {
                keys[i]=0; ++repaired; break;
            }
        }
        return repaired;
    }
}
