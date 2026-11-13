#pragma once

#include <cstdint>

namespace rr::sim {

/// Fixed-step scheduler for authoritative simulation.
///
/// Rendering/network rates must not drive physics dt. Callers pass elapsed
/// real time to advance(); the clock yields a bounded number of fixed steps and
/// preserves the sub-step remainder.
class FixedStepClock {
public:
    explicit FixedStepClock(double step_seconds = 1.0 / 60.0,
                            std::uint32_t max_steps_per_advance = 8);

    /// Returns the number of fixed steps that should be simulated.
    /// Non-finite or non-positive elapsed time is ignored.
    std::uint32_t advance(double elapsed_seconds);

    [[nodiscard]] std::uint64_t tick() const noexcept;
    [[nodiscard]] double step_seconds() const noexcept;
    [[nodiscard]] double accumulator() const noexcept;
    [[nodiscard]] std::uint32_t max_steps_per_advance() const noexcept;
    [[nodiscard]] std::uint64_t dropped_steps() const noexcept;

private:
    double step_seconds_;
    double accumulator_{0.0};
    std::uint64_t tick_{0};
    std::uint32_t max_steps_per_advance_;
    std::uint64_t dropped_steps_{0};
};

} // namespace rr::sim
