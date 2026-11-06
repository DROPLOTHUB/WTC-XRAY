#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Standard-library only. Single-owner, not thread safe. No allocations on add_ms.
namespace quality_diagnostics {
inline void increment(std::uint64_t& n) noexcept {
    if (n != (std::numeric_limits<std::uint64_t>::max)()) ++n;
}
struct timing_summary {
    std::size_t samples = 0;
    std::uint64_t accepted = 0, rejected = 0;
    double min_ms = 0, mean_ms = 0, p95_ms = 0, p99_ms = 0, max_ms = 0;
};
template<std::size_t Capacity = 512> class timing_accumulator {
    static_assert(Capacity > 0 && Capacity <= 65536, "bounded timing capacity required");
    std::array<double, Capacity> samples_{};
    std::size_t next_ = 0, size_ = 0;
    std::uint64_t accepted_ = 0, rejected_ = 0;
public:
    // Reject rather than clamp invalid or implausible (>60 s) durations.
    bool add_ms(double ms) noexcept {
        if (!std::isfinite(ms) || ms < 0 || ms > 60000) { increment(rejected_); return false; }
        samples_[next_] = ms; next_ = (next_ + 1) % Capacity;
        if (size_ < Capacity) ++size_;
        increment(accepted_); return true;
    }
    void reset() noexcept { *this = {}; }
    timing_summary summary() const {
        timing_summary s{}; s.samples = size_; s.accepted = accepted_; s.rejected = rejected_;
        if (!size_) return s;
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.begin() + size_);
        for (std::size_t i = 0; i < size_; ++i) s.mean_ms += sorted[i];
        s.mean_ms /= static_cast<double>(size_);
        s.min_ms = sorted[0]; s.max_ms = sorted[size_ - 1];
        // Nearest rank: ceil(p*N)-1. Integer arithmetic is exact for small N.
        s.p95_ms = sorted[(95 * size_ + 99) / 100 - 1];
        s.p99_ms = sorted[(99 * size_ + 99) / 100 - 1];
        return s;
    }
};
template<std::size_t Capacity = 512> struct diagnostics {
    timing_accumulator<Capacity> frame_ms, esp_ms, lead_ms;
    timing_summary frame() const noexcept { return frame_ms.summary(); }
    timing_summary esp() const noexcept { return esp_ms.summary(); }
    timing_summary lead() const noexcept { return lead_ms.summary(); }
    void reset() noexcept { frame_ms.reset(); esp_ms.reset(); lead_ms.reset(); }
};
} // namespace quality_diagnostics
