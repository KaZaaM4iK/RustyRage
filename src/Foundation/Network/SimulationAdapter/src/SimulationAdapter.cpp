#include "rr/net/SimulationAdapter.hpp"

#include <algorithm>

namespace rr::net {

NetSimAdapter::NetSimAdapter(double fixed_step_seconds, std::uint32_t max_steps_per_advance)
    : clock_(fixed_step_seconds, max_steps_per_advance) {
}

bool NetSimAdapter::submit_input(ClientId client, const ClientInput& input) {
    std::string error;
    if (client == kInvalidClientId || !validate_client_input(input, error)) {
        return false;
    }

    latest_inputs_[client] = input;
    last_input_tick_ = std::max(last_input_tick_, input.tick);
    return true;
}

std::uint32_t NetSimAdapter::advance(double elapsed_seconds) {
    return clock_.advance(elapsed_seconds);
}

std::uint64_t NetSimAdapter::tick() const {
    return clock_.tick();
}

std::uint32_t NetSimAdapter::last_input_tick() const {
    return last_input_tick_;
}

std::optional<ClientInput> NetSimAdapter::latest_input(ClientId client) const {
    const auto iterator = latest_inputs_.find(client);
    if (iterator == latest_inputs_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

ServerTick NetSimAdapter::make_server_tick() const {
    ServerTick tick;
    tick.tick = static_cast<std::uint32_t>(std::max<std::uint64_t>(clock_.tick(), 1));
    tick.last_processed_input = last_input_tick_;
    return tick;
}

} // namespace rr::net
