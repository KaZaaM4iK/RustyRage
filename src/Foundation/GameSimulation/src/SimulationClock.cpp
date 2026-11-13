#include "rr/sim/SimulationClock.hpp"

#include <cmath>
#include <stdexcept>

namespace rr::sim {

FixedStepClock::FixedStepClock(double step_seconds, std::uint32_t max_steps_per_advance)
    : step_seconds_(step_seconds),
      max_steps_per_advance_(max_steps_per_advance) {
    if (!std::isfinite(step_seconds_) || step_seconds_ <= 0.0) {
        throw std::invalid_argument("FixedStepClock: step_seconds must be finite and positive");
    }
    if (max_steps_per_advance_ == 0) {
        throw std::invalid_argument("FixedStepClock: max_steps_per_advance must be positive");
    }
}

std::uint32_t FixedStepClock::advance(double elapsed_seconds) {
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0) {
        return 0;
    }

    accumulator_ += elapsed_seconds;

    // Guard against floating-point representation error when the caller passes
    // exactly one step (1/60.0 as a double).
    const double steps_as_double = std::floor((accumulator_ + 1e-12) / step_seconds_);
    if (steps_as_double <= 0.0) {
        return 0;
    }

    std::uint64_t requested = static_cast<std::uint64_t>(steps_as_double);
    std::uint32_t steps = 0;

    if (requested > max_steps_per_advance_) {
        steps = max_steps_per_advance_;
        dropped_steps_ += requested - max_steps_per_advance_;
        accumulator_ = std::fmod(accumulator_, step_seconds_);
    } else {
        steps = static_cast<std::uint32_t>(requested);
        accumulator_ -= step_seconds_ * static_cast<double>(steps);
        if (accumulator_ < 0.0) {
            accumulator_ = 0.0;
        }
    }

    tick_ += steps;
    return steps;
}

std::uint64_t FixedStepClock::tick() const noexcept {
    return tick_;
}

double FixedStepClock::step_seconds() const noexcept {
    return step_seconds_;
}

double FixedStepClock::accumulator() const noexcept {
    return accumulator_;
}

std::uint32_t FixedStepClock::max_steps_per_advance() const noexcept {
    return max_steps_per_advance_;
}

std::uint64_t FixedStepClock::dropped_steps() const noexcept {
    return dropped_steps_;
}

} // namespace rr::sim
