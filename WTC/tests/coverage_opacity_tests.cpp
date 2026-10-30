#include "../src/features/esp/coverage_opacity.hpp"
#include <cassert>
#include <cstdio>
int main() {
    assert(!lead_opacity::visible(0,0.f));
    assert(!lead_opacity::visible(45,-1.f));
    assert(lead_opacity::visible(45,1.f));
    assert((lead_opacity::solid(IM_COL32(255,128,20,32)) & IM_COL32_A_MASK) == IM_COL32_A_MASK);
    int previous=0;
    for(int step=1;step<=10;step++) {
        int count=0;
        for(int i=0;i<256;i++)count+=lead_opacity::visible(i,step/10.f);
        assert(count>=previous && count<=256);
        previous=count;
    }
    assert(previous==256);
    std::puts("PASS alpha-safe coverage monotonic, zero and full opaque");
}
