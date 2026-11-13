#include "rr/world/RouteGraph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rr::world {
namespace {

constexpr std::size_t kMaxRouteStates = 100000;
constexpr std::size_t kMaxMandatoryCheckpoints = 1024;
constexpr std::uint32_t kMaxRouteLaps = 100000;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

bool contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::size_t mandatory_index(const std::vector<std::string>& mandatory,
                            const std::string& checkpoint) {
    const auto iterator = std::find(mandatory.begin(), mandatory.end(), checkpoint);
    return iterator == mandatory.end()
               ? std::numeric_limits<std::size_t>::max()
               : static_cast<std::size_t>(iterator - mandatory.begin());
}

bool bit_set(const std::vector<std::uint64_t>& mask, std::size_t index) {
    return (mask[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
}

void set_bit(std::vector<std::uint64_t>& mask, std::size_t index) {
    mask[index / 64] |= (std::uint64_t{1} << (index % 64));
}

bool all_bits_set(const std::vector<std::uint64_t>& mask, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (!bit_set(mask, index)) {
            return false;
        }
    }
    return true;
}

struct StateKey {
    std::string current{};
    std::uint32_t lap{0};
    std::vector<std::uint64_t> mask{};
    bool finished{false};

    bool operator<(const StateKey& other) const {
        if (current != other.current) {
            return current < other.current;
        }
        if (lap != other.lap) {
            return lap < other.lap;
        }
        if (mask != other.mask) {
            return mask < other.mask;
        }
        return finished < other.finished;
    }
};

void add_diagnostic(MapValidationResult& result,
                    MapDiagnosticSeverity severity,
                    std::string code,
                    std::string path,
                    std::string message) {
    result.diagnostics.push_back(
        MapDiagnostic{severity, std::move(code), std::move(path), std::move(message)});
}

} // namespace

std::optional<RouteGraph> RouteGraph::build(const RouteDefinition& definition,
                                            const std::vector<Checkpoint>& checkpoints,
                                            std::string& error) {
    try {
        if (definition.required_laps == 0) {
            fail("route required_laps must be at least 1");
        }
        if (definition.route_id.empty() || definition.track_id.empty()) {
            fail("route_id and track_id must not be empty");
        }

        std::map<std::string, const Checkpoint*> checkpoint_by_id;
        for (const Checkpoint& checkpoint : checkpoints) {
            if (checkpoint.checkpoint_id.empty()) {
                fail("checkpoint has an empty checkpoint_id");
            }
            if (checkpoint.track_id != definition.track_id) {
                continue;
            }
            if (!checkpoint_by_id.emplace(checkpoint.checkpoint_id, &checkpoint).second) {
                fail("duplicate checkpoint_id in route checkpoint set");
            }
        }

        const auto require_checkpoint = [&](const std::string& id, const char* role) {
            const auto iterator = checkpoint_by_id.find(id);
            if (iterator == checkpoint_by_id.end()) {
                fail(std::string(role) + " checkpoint '" + id +
                     "' does not exist on route track");
            }
        };

        require_checkpoint(definition.start_checkpoint_id, "start");
        require_checkpoint(definition.finish_checkpoint_id, "finish");
        for (const std::string& mandatory : definition.mandatory_checkpoint_ids) {
            require_checkpoint(mandatory, "mandatory");
        }
        {
            std::set<std::string> mandatory_seen;
            for (const std::string& mandatory : definition.mandatory_checkpoint_ids) {
                if (!mandatory_seen.insert(mandatory).second) {
                    fail("mandatory_checkpoint_ids contains duplicates");
                }
            }
        }

        std::map<std::string, std::vector<std::string>> outgoing;
        for (const RouteEdge& edge : definition.edges) {
            require_checkpoint(edge.from_checkpoint_id, "edge source");
            require_checkpoint(edge.to_checkpoint_id, "edge destination");
            if (edge.from_checkpoint_id == edge.to_checkpoint_id) {
                fail("route edge must not be a self-loop");
            }
            outgoing[edge.from_checkpoint_id].push_back(edge.to_checkpoint_id);
        }

        RouteGraph graph;
        graph.definition_ = definition;
        graph.mandatory_checkpoints_ = definition.mandatory_checkpoint_ids;
        for (const auto& item : outgoing) {
            for (const std::string& destination : item.second) {
                graph.edges_.emplace_back(item.first, destination);
            }
        }
        error.clear();
        return graph;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

RouteProgress RouteGraph::initial_progress() const {
    RouteProgress progress;
    progress.current_checkpoint_id = definition_.start_checkpoint_id;
    if (contains(mandatory_checkpoints_, definition_.start_checkpoint_id)) {
        progress.visited_mandatory_checkpoint_ids.push_back(definition_.start_checkpoint_id);
    }
    return progress;
}

RouteAdvanceResult RouteGraph::advance(const RouteProgress& progress,
                                       const std::string& next_checkpoint_id) const {
    RouteAdvanceResult result;
    if (progress.finished) {
        result.error = "route is already finished";
        return result;
    }
    if (next_checkpoint_id.empty()) {
        result.error = "next checkpoint id must not be empty";
        return result;
    }

    const bool edge_exists = std::any_of(
        edges_.begin(), edges_.end(), [&](const std::pair<std::string, std::string>& edge) {
            return edge.first == progress.current_checkpoint_id &&
                   edge.second == next_checkpoint_id;
        });
    if (!edge_exists) {
        result.error = "no route edge from '" + progress.current_checkpoint_id + "' to '" +
                       next_checkpoint_id + "'";
        return result;
    }

    const bool is_finish = next_checkpoint_id == definition_.finish_checkpoint_id;
    if (is_finish) {
        for (const std::string& mandatory : mandatory_checkpoints_) {
            if (!contains(progress.visited_mandatory_checkpoint_ids, mandatory)) {
                result.error = "finish reached before mandatory checkpoint '" + mandatory + "'";
                return result;
            }
        }
        result.progress = progress;
        result.progress.completed_laps = progress.completed_laps + 1;
        result.progress.current_checkpoint_id = next_checkpoint_id;
        if (result.progress.completed_laps >= definition_.required_laps) {
            result.progress.finished = true;
        } else {
            result.progress.visited_mandatory_checkpoint_ids.clear();
        }
        result.accepted = true;
        return result;
    }

    result.progress = progress;
    result.progress.current_checkpoint_id = next_checkpoint_id;
    if (contains(mandatory_checkpoints_, next_checkpoint_id) &&
        !contains(result.progress.visited_mandatory_checkpoint_ids, next_checkpoint_id)) {
        result.progress.visited_mandatory_checkpoint_ids.push_back(next_checkpoint_id);
    }
    result.accepted = true;
    return result;
}

MapValidationResult RouteGraph::validate() const {
    MapValidationResult result;
    if (definition_.required_laps == 0) {
        add_diagnostic(result, MapDiagnosticSeverity::Error, "invalid_lap_count",
                       "route.required_laps", "required_laps must be at least 1");
        return result;
    }
    if (edges_.empty()) {
        add_diagnostic(result, MapDiagnosticSeverity::Error, "empty_route_graph",
                       "route.edges", "route has no transitions");
        return result;
    }
    if (mandatory_checkpoints_.size() > kMaxMandatoryCheckpoints) {
        add_diagnostic(result, MapDiagnosticSeverity::Error, "route_state_limit",
                       "route.mandatory_checkpoint_ids",
                       "mandatory checkpoint count " +
                           std::to_string(mandatory_checkpoints_.size()) +
                           " exceeds the safe bit-mask limit " +
                           std::to_string(kMaxMandatoryCheckpoints));
        return result;
    }
    if (definition_.required_laps > kMaxRouteLaps) {
        add_diagnostic(result, MapDiagnosticSeverity::Error, "route_state_limit",
                       "route.required_laps",
                       "required_laps " + std::to_string(definition_.required_laps) +
                           " exceeds the numeric state limit " +
                           std::to_string(kMaxRouteLaps));
        return result;
    }
    if (definition_.required_laps > 1) {
        for (const std::string& boundary : {definition_.start_checkpoint_id,
                                            definition_.finish_checkpoint_id}) {
            if (contains(mandatory_checkpoints_, boundary)) {
                add_diagnostic(
                    result, MapDiagnosticSeverity::Error,
                    "mandatory_start_finish_multi_lap",
                    "route.mandatory_checkpoint_ids",
                    "multi-lap route cannot re-credit mandatory checkpoint '" + boundary +
                        "' because it is the start/finish checkpoint; use a separate checkpoint");
                return result;
            }
        }
    }

    std::vector<std::uint64_t> initial_mask(
        (mandatory_checkpoints_.size() + 63) / 64, 0);
    const std::size_t start_mandatory =
        mandatory_index(mandatory_checkpoints_, definition_.start_checkpoint_id);
    if (start_mandatory != std::numeric_limits<std::size_t>::max()) {
        set_bit(initial_mask, start_mandatory);
    }

    StateKey initial{definition_.start_checkpoint_id, 0, initial_mask, false};
    std::set<StateKey> visited;
    std::deque<StateKey> queue;
    visited.insert(initial);
    queue.push_back(initial);

    std::vector<std::uint64_t> finished_masks(
        (mandatory_checkpoints_.size() + 63) / 64, 0);
    std::vector<std::uint64_t> reachable_masks(
        (mandatory_checkpoints_.size() + 63) / 64, 0);
    bool has_finished = false;

    while (!queue.empty()) {
        const StateKey state = std::move(queue.front());
        queue.pop_front();
        for (std::size_t index = 0; index < reachable_masks.size(); ++index) {
            reachable_masks[index] |= state.mask[index];
        }
        if (state.finished) {
            has_finished = true;
            for (std::size_t index = 0; index < finished_masks.size(); ++index) {
                finished_masks[index] |= state.mask[index];
            }
            continue;
        }

        for (const auto& edge : edges_) {
            if (edge.first != state.current) {
                continue;
            }
            const std::string& next = edge.second;
            StateKey next_state = state;
            if (next == definition_.finish_checkpoint_id) {
                if (!all_bits_set(state.mask, mandatory_checkpoints_.size())) {
                    continue;
                }
                next_state.current = next;
                next_state.lap = state.lap + 1;
                next_state.finished = next_state.lap >= definition_.required_laps;
                if (!next_state.finished) {
                    next_state.mask.assign(finished_masks.size(), 0);
                }
            } else {
                next_state.current = next;
                const std::size_t index = mandatory_index(mandatory_checkpoints_, next);
                if (index != std::numeric_limits<std::size_t>::max()) {
                    set_bit(next_state.mask, index);
                }
            }
            if (visited.size() >= kMaxRouteStates) {
                add_diagnostic(result, MapDiagnosticSeverity::Error, "route_state_limit",
                               "route", "route state exploration exceeded configured limit");
                return result;
            }
            if (visited.insert(next_state).second) {
                queue.push_back(std::move(next_state));
            }
        }
    }

    if (!has_finished) {
        add_diagnostic(result, MapDiagnosticSeverity::Error, "route_not_completable",
                       "route", "no reachable path satisfies the finish and lap conditions");
        for (std::size_t index = 0; index < mandatory_checkpoints_.size(); ++index) {
            if (!bit_set(reachable_masks, index)) {
                add_diagnostic(result, MapDiagnosticSeverity::Error,
                               "unreachable_mandatory_checkpoint",
                               "route.mandatory_checkpoint_ids[" + std::to_string(index) + "]",
                               "mandatory checkpoint is not reachable in any route state: " +
                                   mandatory_checkpoints_[index]);
            }
        }
        return result;
    }
    for (std::size_t index = 0; index < mandatory_checkpoints_.size(); ++index) {
        if (!bit_set(finished_masks, index)) {
            add_diagnostic(result, MapDiagnosticSeverity::Error,
                           "unreachable_mandatory_checkpoint",
                           "route.mandatory_checkpoint_ids[" + std::to_string(index) + "]",
                           "mandatory checkpoint is not reachable on a finish path: " +
                               mandatory_checkpoints_[index]);
        }
    }
    return result;
}

const RouteDefinition& RouteGraph::definition() const noexcept {
    return definition_;
}

} // namespace rr::world
