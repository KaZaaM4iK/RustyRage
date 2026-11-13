#pragma once

#include "rr/world/MapData.hpp"
#include "rr/world/MapDiagnostics.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rr::world {

struct RouteProgress {
    std::string current_checkpoint_id{};
    std::uint32_t completed_laps{0};
    std::vector<std::string> visited_mandatory_checkpoint_ids{};
    bool finished{false};
};

struct RouteAdvanceResult {
    bool accepted{false};
    std::string error{};
    RouteProgress progress{};
};

/// Stateful route graph. Closed racing routes are legal; validation explores
/// reachable (checkpoint, lap, mandatory-progress) states rather than rejecting
/// cycles as a static DAG would.
class RouteGraph {
public:
    [[nodiscard]] static std::optional<RouteGraph> build(
        const RouteDefinition& definition,
        const std::vector<Checkpoint>& checkpoints,
        std::string& error);

    [[nodiscard]] RouteProgress initial_progress() const;

    [[nodiscard]] RouteAdvanceResult advance(const RouteProgress& progress,
                                             const std::string& next_checkpoint_id) const;

    [[nodiscard]] MapValidationResult validate() const;

    [[nodiscard]] const RouteDefinition& definition() const noexcept;

private:
    RouteDefinition definition_{};
    std::vector<std::string> mandatory_checkpoints_{};
    std::vector<std::pair<std::string, std::string>> edges_{};
};

} // namespace rr::world
