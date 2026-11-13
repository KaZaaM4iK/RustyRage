#pragma once

#include "rr/net/Protocol.hpp"
#include "rr/net/Runtime.hpp"
#include "rr/sim/SimulationClock.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace rr::net {

/// Thin integration seam between validated network input and fixed-step
/// simulation. It deliberately does not implement gameplay rules, prediction,
/// reconciliation or input deduplication; those are subsequent tasks.
class NetSimAdapter {
public:
    explicit NetSimAdapter(double fixed_step_seconds = 1.0 / 60.0,
                           std::uint32_t max_steps_per_advance = 8);

    bool submit_input(ClientId client, const ClientInput& input);
    std::uint32_t advance(double elapsed_seconds);

    [[nodiscard]] std::uint64_t tick() const;
    [[nodiscard]] std::uint32_t last_input_tick() const;
    [[nodiscard]] std::optional<ClientInput> latest_input(ClientId client) const;

    /// Builds an authoritative tick acknowledgement. The protocol requires a
    /// non-zero tick; before the first simulation step, tick 1 is reported.
    [[nodiscard]] ServerTick make_server_tick() const;

private:
    rr::sim::FixedStepClock clock_;
    std::unordered_map<ClientId, ClientInput> latest_inputs_;
    std::uint32_t last_input_tick_{0};
};

} // namespace rr::net
