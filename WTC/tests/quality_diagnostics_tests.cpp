#include "../src/core/quality_diagnostics.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using quality_diagnostics::timing_accumulator;
int main() {
    timing_accumulator<4> ring;
    assert(ring.summary().samples==0 && ring.summary().p95_ms==0);
    for(double v:{1.0,2.0,3.0,4.0,5.0,6.0}) assert(ring.add_ms(v));
    auto s=ring.summary();
    assert(s.samples==4 && s.accepted==6 && s.min_ms==3 && s.max_ms==6);
    assert(std::abs(s.mean_ms-4.5)<1e-9);
    assert(s.p95_ms==6 && s.p99_ms==6);
    timing_accumulator<100> pct;
    for(int i=1;i<=100;++i) assert(pct.add_ms(i));
    s=pct.summary();
    assert(s.samples==100 && s.p95_ms==95 && s.p99_ms==99 && s.min_ms==1 && s.mean_ms==50.5);
    timing_accumulator<8> rejecting;
    assert(!rejecting.add_ms(-1) && !rejecting.add_ms(std::nan("")) && !rejecting.add_ms(1e9) && !rejecting.add_ms(std::numeric_limits<double>::infinity()));
    assert(rejecting.add_ms(0));
    s=rejecting.summary();
    assert(s.samples==1 && s.rejected==4 && s.min_ms==0 && s.mean_ms==0 && s.p95_ms==0);
    quality_diagnostics::diagnostics<16> diag;
    diag.frame_ms.add_ms(20.0);diag.frame_ms.add_ms(22.0);
    diag.esp_ms.add_ms(1.5);diag.lead_ms.add_ms(0.055);
    assert(diag.frame().samples==2 && diag.frame().p99_ms==22);
    assert(diag.lead().mean_ms>0.05 && diag.lead().mean_ms<0.06);
    diag.reset();
    assert(diag.frame().samples==0 && diag.esp().samples==0 && diag.lead().samples==0);
    std::puts("PASS bounded p95/p99 timing: ring eviction, nearest-rank percentiles, invalid rejection and per-metric reset");
}
