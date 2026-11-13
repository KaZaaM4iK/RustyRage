#include "rr/world/MapValidator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rr::world {
namespace {

bool finite_vec(rr::math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite_quat(rr::math::Quat value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

float quat_length(rr::math::Quat value) {
    return std::sqrt(value.x * value.x + value.y * value.y +
                     value.z * value.z + value.w * value.w);
}

bool safe_id(std::string_view id, std::size_t max_bytes) {
    if (id.empty() || id.size() > max_bytes) {
        return false;
    }
    for (char character : id) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) == 0 && character != '_' && character != '-' && character != '.') {
            return false;
        }
    }
    return true;
}

class Validator {
public:
    Validator(const MapData& data, const MapLimits& limits)
        : data_(data), limits_(limits) {
    }

    MapValidationResult run() {
        validate_metadata();
        validate_surfaces();
        validate_tracks();
        validate_checkpoints();
        validate_routes();
        validate_spawns();
        validate_pickups();
        validate_interactive_objects();
        validate_destructibles();
        validate_triggers();
        validate_safe_zone();
        return std::move(result_);
    }

private:
    void error(std::string code, std::string path, std::string message) {
        result_.diagnostics.push_back(
            MapDiagnostic{MapDiagnosticSeverity::Error, std::move(code),
                          std::move(path), std::move(message)});
    }

    void warning(std::string code, std::string path, std::string message) {
        result_.diagnostics.push_back(
            MapDiagnostic{MapDiagnosticSeverity::Warning, std::move(code),
                          std::move(path), std::move(message)});
    }

    bool add_unique_id(const std::string& id, const std::string& path) {
        if (!safe_id(id, limits_.max_identifier_bytes)) {
            error("invalid_id", path, "identifier is empty, too long or contains unsupported characters");
            return false;
        }
        if (!ids_.insert(id).second) {
            error("duplicate_id", path, "identifier is already used by another map element");
            return false;
        }
        return true;
    }

    bool check_coordinate(rr::math::Vec3 value, const std::string& path) {
        if (!finite_vec(value)) {
            error("non_finite", path, "coordinate must be finite");
            return false;
        }
        const float magnitude = std::max({std::fabs(value.x), std::fabs(value.y), std::fabs(value.z)});
        if (magnitude > limits_.max_coordinate_abs_m) {
            error("coordinate_range", path, "coordinate exceeds configured magnitude limit");
            return false;
        }
        return true;
    }

    bool check_quat(rr::math::Quat value, const std::string& path) {
        if (!finite_quat(value)) {
            error("non_finite", path, "quaternion must be finite");
            return false;
        }
        const float length = quat_length(value);
        if (length < 0.99f || length > 1.01f) {
            error("quat_not_normalized", path, "quaternion must be approximately unit length");
            return false;
        }
        return true;
    }

    void validate_metadata() {
        if (data_.metadata.schema_version != kMapSchemaVersion) {
            error("unsupported_schema_version", "$.schema_version",
                  "unsupported map schema version");
        }
        if (!safe_id(data_.metadata.map_id, limits_.max_identifier_bytes)) {
            error("invalid_map_id", "$.map_id",
                  "map identifier is empty, too long or contains unsupported characters");
        }
        if (data_.metadata.name.empty() || data_.metadata.name.size() > limits_.max_string_bytes) {
            error("invalid_name", "$.name", "map name is empty or exceeds configured length");
        }
        if (data_.metadata.author.size() > limits_.max_string_bytes) {
            error("invalid_author", "$.author", "author exceeds configured length");
        }
        if (data_.metadata.author.empty()) {
            warning("empty_author", "$.author", "author is empty");
        }
        if (data_.metadata.max_players == 0 || data_.metadata.max_players > kMapMaxPlayers) {
            error("invalid_player_count", "$.max_players",
                  "max_players must be between 1 and 8");
        }
        switch (data_.metadata.space) {
            case MapSpaceType::Racing:
            case MapSpaceType::Survival:
            case MapSpaceType::Mixed:
                break;
            default:
                error("invalid_space", "$.space", "unsupported map space type");
                break;
        }
    }

    void validate_surfaces() {
        if (data_.surfaces.size() > limits_.max_surfaces) {
            error("collection_limit", "$.surfaces", "surface count exceeds configured limit");
        }
        if (data_.surfaces.empty()) {
            error("missing_surfaces", "$.surfaces", "map must define at least one surface");
        }
        for (std::size_t index = 0; index < data_.surfaces.size(); ++index) {
            const TrackSurface& surface = data_.surfaces[index];
            const std::string path = "$.surfaces[" + std::to_string(index) + "]";
            add_unique_id(surface.surface_id, path + ".surface_id");
            surface_ids_.insert(surface.surface_id);
            if (!std::isfinite(surface.grip_multiplier) || surface.grip_multiplier < 0.0f) {
                error("invalid_value", path + ".grip_multiplier",
                      "grip multiplier must be finite and non-negative");
            }
            if (!std::isfinite(surface.rolling_resistance) || surface.rolling_resistance < 0.0f) {
                error("invalid_value", path + ".rolling_resistance",
                      "rolling resistance must be finite and non-negative");
            }
            if (surface.renderer_material_id.size() > limits_.max_string_bytes) {
                error("string_limit", path + ".material_id", "material id exceeds configured length");
            }
        }
    }

    bool check_surface_reference(const std::string& surface_id, const std::string& path) {
        if (surface_id.empty()) {
            error("missing_surface_reference", path, "surface_id is required");
            return false;
        }
        if (surface_ids_.find(surface_id) == surface_ids_.end()) {
            error("unknown_surface_reference", path, "surface_id does not reference a declared surface");
            return false;
        }
        return true;
    }

    void validate_tracks() {
        if (data_.tracks.size() > limits_.max_tracks) {
            error("collection_limit", "$.tracks", "track count exceeds configured limit");
        }
        if (data_.tracks.empty()) {
            error("missing_tracks", "$.tracks", "map must define at least one track");
        }

        std::size_t total_points = 0;
        for (std::size_t track_index = 0; track_index < data_.tracks.size(); ++track_index) {
            const TrackSpline& track = data_.tracks[track_index];
            const std::string path = "$.tracks[" + std::to_string(track_index) + "]";
            add_unique_id(track.track_id, path + ".track_id");
            track_ids_.insert(track.track_id);

            const std::size_t minimum_points = track.closed ? 3u : 2u;
            if (track.points.size() < minimum_points) {
                error("too_few_control_points", path + ".points",
                      "track has fewer control points than required");
            }
            if (track.points.size() > limits_.max_control_points_per_track) {
                error("control_point_limit", path + ".points",
                      "control point count exceeds configured limit");
            }
            total_points += track.points.size();
            if (total_points > limits_.max_total_control_points) {
                error("total_control_point_limit", "$.tracks",
                      "total control point count exceeds configured limit");
            }

            if (!std::isfinite(track.texture_repeat_m) ||
                track.texture_repeat_m < limits_.min_texture_repeat_m) {
                error("invalid_texture_repeat", path + ".texture_repeat_m",
                      "texture_repeat_m must be finite and positive");
            }
            if (!std::isfinite(track.max_sample_spacing_m) || track.max_sample_spacing_m <= 0.0f) {
                error("invalid_sample_spacing", path + ".max_sample_spacing_m",
                      "max_sample_spacing_m must be finite and positive");
            }
            if (!std::isfinite(track.max_sagitta_error_m) || track.max_sagitta_error_m <= 0.0f) {
                error("invalid_sagitta", path + ".max_sagitta_error_m",
                      "max_sagitta_error_m must be finite and positive");
            }

            const CrossSectionProfile& profile = track.cross_section;
            if (!std::isfinite(profile.left_shoulder_width_m) ||
                profile.left_shoulder_width_m < 0.0f) {
                error("invalid_shoulder", path + ".cross_section.left_shoulder_width_m",
                      "left shoulder width must be finite and non-negative");
            }
            if (!std::isfinite(profile.right_shoulder_width_m) ||
                profile.right_shoulder_width_m < 0.0f) {
                error("invalid_shoulder", path + ".cross_section.right_shoulder_width_m",
                      "right shoulder width must be finite and non-negative");
            }
            if (!std::isfinite(profile.shoulder_slope_rad) ||
                std::fabs(profile.shoulder_slope_rad) >= 1.5707964f) {
                error("invalid_shoulder_slope", path + ".cross_section.shoulder_slope_rad",
                      "shoulder slope must be finite and below 90 degrees");
            }
            if (profile.left_shoulder_width_m > 0.0f) {
                check_surface_reference(profile.left_shoulder_surface_id,
                                        path + ".cross_section.left_shoulder_surface_id");
            }
            if (profile.right_shoulder_width_m > 0.0f) {
                check_surface_reference(profile.right_shoulder_surface_id,
                                        path + ".cross_section.right_shoulder_surface_id");
            }

            for (std::size_t point_index = 0; point_index < track.points.size(); ++point_index) {
                const TrackPoint& point = track.points[point_index];
                const std::string point_path =
                    path + ".points[" + std::to_string(point_index) + "]";
                check_coordinate(point.position, point_path + ".position");
                if (!std::isfinite(point.road_width_m) ||
                    point.road_width_m < limits_.min_road_width_m ||
                    point.road_width_m > limits_.max_road_width_m) {
                    error("invalid_width", point_path + ".road_width_m",
                          "road width is outside the configured range");
                }
                if (!std::isfinite(point.banking_rad) ||
                    std::fabs(point.banking_rad) > limits_.max_banking_rad) {
                    error("invalid_banking", point_path + ".banking_rad",
                          "banking is outside the configured range");
                }
                check_surface_reference(point.surface_id, point_path + ".surface_id");
            }

            if (track.closed && !track.points.empty() &&
                track.points.front().position == track.points.back().position) {
                warning("duplicate_closed_point", path + ".points",
                        "closed track duplicates its first point at the end");
            }
        }
    }

    bool check_track_reference(const std::string& track_id, const std::string& path) {
        if (track_id.empty() || track_ids_.find(track_id) == track_ids_.end()) {
            error("unknown_track_reference", path, "track_id does not reference a declared track");
            return false;
        }
        return true;
    }

    void validate_checkpoints() {
        if (data_.checkpoints.size() > limits_.max_checkpoints) {
            error("collection_limit", "$.checkpoints", "checkpoint count exceeds configured limit");
        }
        for (std::size_t index = 0; index < data_.checkpoints.size(); ++index) {
            const Checkpoint& checkpoint = data_.checkpoints[index];
            const std::string path = "$.checkpoints[" + std::to_string(index) + "]";
            add_unique_id(checkpoint.checkpoint_id, path + ".checkpoint_id");
            checkpoint_track_ids_[checkpoint.checkpoint_id] = checkpoint.track_id;
            check_track_reference(checkpoint.track_id, path + ".track_id");
            check_coordinate(checkpoint.position, path + ".position");
            if (!std::isfinite(checkpoint.radius_m) || checkpoint.radius_m <= 0.0f) {
                error("invalid_radius", path + ".radius_m", "checkpoint radius must be positive");
            }
        }
    }

    bool check_checkpoint_reference(const std::string& checkpoint_id,
                                    const std::string& track_id,
                                    const std::string& path) {
        const auto iterator = checkpoint_track_ids_.find(checkpoint_id);
        if (iterator == checkpoint_track_ids_.end()) {
            error("unknown_checkpoint_reference", path,
                  "checkpoint_id does not reference a declared checkpoint");
            return false;
        }
        if (iterator->second != track_id) {
            error("checkpoint_track_mismatch", path,
                  "checkpoint belongs to a different track");
            return false;
        }
        return true;
    }

    void validate_routes() {
        if (data_.routes.size() > limits_.max_routes) {
            error("collection_limit", "$.routes", "route count exceeds configured limit");
        }
        for (std::size_t route_index = 0; route_index < data_.routes.size(); ++route_index) {
            const RouteDefinition& route = data_.routes[route_index];
            const std::string path = "$.routes[" + std::to_string(route_index) + "]";
            add_unique_id(route.route_id, path + ".route_id");
            const bool track_ok = check_track_reference(route.track_id, path + ".track_id");
            check_checkpoint_reference(route.start_checkpoint_id, route.track_id,
                                       path + ".start_checkpoint_id");
            check_checkpoint_reference(route.finish_checkpoint_id, route.track_id,
                                       path + ".finish_checkpoint_id");
            if (route.start_checkpoint_id == route.finish_checkpoint_id) {
                const auto track_iterator = std::find_if(
                    data_.tracks.begin(), data_.tracks.end(),
                    [&route](const TrackSpline& track) {
                        return track.track_id == route.track_id;
                    });
                if (track_iterator != data_.tracks.end() && !track_iterator->closed) {
                    warning("start_equals_finish_on_open_track", path,
                            "start and finish are the same checkpoint on an open track");
                }
            }
            if (route.required_laps == 0) {
                error("invalid_lap_count", path + ".required_laps",
                      "required_laps must be at least 1");
            }
            for (std::size_t index = 0; index < route.mandatory_checkpoint_ids.size(); ++index) {
                check_checkpoint_reference(route.mandatory_checkpoint_ids[index],
                                           route.track_id,
                                           path + ".mandatory_checkpoint_ids[" +
                                               std::to_string(index) + "]");
            }
            if (route.edges.size() > limits_.max_route_edges) {
                error("route_edge_limit", path + ".edges",
                      "route edge count exceeds configured limit");
            }
            std::unordered_set<std::string> edge_keys;
            for (std::size_t index = 0; index < route.edges.size(); ++index) {
                const RouteEdge& edge = route.edges[index];
                const std::string edge_path = path + ".edges[" + std::to_string(index) + "]";
                check_checkpoint_reference(edge.from_checkpoint_id, route.track_id,
                                           edge_path + ".from");
                check_checkpoint_reference(edge.to_checkpoint_id, route.track_id,
                                           edge_path + ".to");
                if (edge.from_checkpoint_id == edge.to_checkpoint_id) {
                    error("self_route_edge", edge_path, "route edge must not be a self-loop");
                }
                const std::string key = edge.from_checkpoint_id + "->" + edge.to_checkpoint_id;
                if (!edge_keys.insert(key).second) {
                    warning("duplicate_route_edge", edge_path, "duplicate route edge");
                }
            }
            if (!track_ok) {
                continue;
            }
        }
    }

    void validate_spawns() {
        if (data_.spawn_points.size() > limits_.max_spawn_points) {
            error("collection_limit", "$.spawn_points", "spawn count exceeds configured limit");
        }
        for (std::size_t index = 0; index < data_.spawn_points.size(); ++index) {
            const SpawnPoint& spawn = data_.spawn_points[index];
            const std::string path = "$.spawn_points[" + std::to_string(index) + "]";
            add_unique_id(spawn.spawn_id, path + ".spawn_id");
            check_track_reference(spawn.track_id, path + ".track_id");
            check_coordinate(spawn.position, path + ".position");
            check_quat(spawn.rotation, path + ".rotation");
        }
    }

    void validate_pickups() {
        if (data_.pickup_spawns.size() > limits_.max_pickup_spawns) {
            error("collection_limit", "$.pickup_spawns", "pickup spawn count exceeds configured limit");
        }
        for (std::size_t index = 0; index < data_.pickup_spawns.size(); ++index) {
            const PickupSpawn& pickup = data_.pickup_spawns[index];
            const std::string path = "$.pickup_spawns[" + std::to_string(index) + "]";
            add_unique_id(pickup.pickup_spawn_id, path + ".pickup_spawn_id");
            check_coordinate(pickup.position, path + ".position");
            if (pickup.pickup_kind.empty() || pickup.pickup_kind.size() > limits_.max_kind_string_bytes) {
                error("invalid_kind", path + ".pickup_kind", "pickup kind is empty or too long");
            }
            if (!std::isfinite(pickup.respawn_seconds) || pickup.respawn_seconds < 0.0f) {
                error("invalid_respawn", path + ".respawn_seconds",
                      "respawn_seconds must be finite and non-negative");
            }
        }
    }

    void validate_interactive_objects() {
        if (data_.interactive_objects.size() > limits_.max_interactive_objects) {
            error("collection_limit", "$.interactive_objects",
                  "interactive object count exceeds configured limit");
        }
        for (std::size_t index = 0; index < data_.interactive_objects.size(); ++index) {
            const InteractiveObject& object = data_.interactive_objects[index];
            const std::string path = "$.interactive_objects[" + std::to_string(index) + "]";
            add_unique_id(object.object_id, path + ".object_id");
            check_coordinate(object.position, path + ".position");
            check_quat(object.rotation, path + ".rotation");
            if (object.kind.empty() || object.kind.size() > limits_.max_kind_string_bytes) {
                error("invalid_kind", path + ".kind", "interactive object kind is empty or too long");
            }
            if (!std::isfinite(object.radius_m) || object.radius_m <= 0.0f) {
                error("invalid_radius", path + ".radius_m", "radius_m must be positive");
            }
        }
    }

    void validate_destructibles() {
        if (data_.destructibles.size() > limits_.max_destructibles) {
            error("collection_limit", "$.destructibles",
                  "destructible object count exceeds configured limit");
        }
        for (std::size_t index = 0; index < data_.destructibles.size(); ++index) {
            const DestructibleObject& object = data_.destructibles[index];
            const std::string path = "$.destructibles[" + std::to_string(index) + "]";
            add_unique_id(object.object_id, path + ".object_id");
            check_coordinate(object.position, path + ".position");
            check_quat(object.rotation, path + ".rotation");
            if (object.kind.empty() || object.kind.size() > limits_.max_kind_string_bytes) {
                error("invalid_kind", path + ".kind", "destructible object kind is empty or too long");
            }
            if (!std::isfinite(object.health) || object.health <= 0.0f) {
                error("invalid_health", path + ".health", "health must be positive");
            }
            if (!std::isfinite(object.radius_m) || object.radius_m <= 0.0f) {
                error("invalid_radius", path + ".radius_m", "radius_m must be positive");
            }
        }
    }

    void validate_triggers() {
        if (data_.trigger_zones.size() > limits_.max_trigger_zones) {
            error("collection_limit", "$.trigger_zones",
                  "trigger zone count exceeds configured limit");
        }
        for (std::size_t index = 0; index < data_.trigger_zones.size(); ++index) {
            const TriggerZone& trigger = data_.trigger_zones[index];
            const std::string path = "$.trigger_zones[" + std::to_string(index) + "]";
            add_unique_id(trigger.trigger_id, path + ".trigger_id");
            check_coordinate(trigger.center, path + ".center");
            if (!finite_vec(trigger.half_extents) ||
                trigger.half_extents.x <= 0.0f || trigger.half_extents.y <= 0.0f ||
                trigger.half_extents.z <= 0.0f) {
                error("invalid_extents", path + ".half_extents",
                      "trigger extents must be finite and positive");
            }
            if (trigger.event_type.empty() || trigger.event_type.size() > limits_.max_kind_string_bytes) {
                error("invalid_event", path + ".event_type", "event_type is empty or too long");
            }
            if (trigger.action_type.empty() || trigger.action_type.size() > limits_.max_kind_string_bytes) {
                error("invalid_action", path + ".action_type", "action_type is empty or too long");
            }
            if (!std::isfinite(trigger.cooldown_seconds) || trigger.cooldown_seconds < 0.0f) {
                error("invalid_cooldown", path + ".cooldown_seconds",
                      "cooldown_seconds must be finite and non-negative");
            }
            if (trigger.max_activations == 0) {
                error("invalid_activation_limit", path + ".max_activations",
                      "max_activations must be at least 1");
            }
        }
    }

    void validate_safe_zone() {
        if (!data_.survival_safe_zone.has_value()) {
            return;
        }
        const SurvivalSafeZone& zone = *data_.survival_safe_zone;
        check_coordinate(zone.start_center, "$.survival_safe_zone.start_center");
        check_coordinate(zone.end_center, "$.survival_safe_zone.end_center");
        if (!std::isfinite(zone.start_radius_m) || zone.start_radius_m <= 0.0f) {
            error("invalid_radius", "$.survival_safe_zone.start_radius_m",
                  "start radius must be positive");
        }
        if (!std::isfinite(zone.end_radius_m) || zone.end_radius_m <= 0.0f) {
            error("invalid_radius", "$.survival_safe_zone.end_radius_m",
                  "end radius must be positive");
        }
        if (!std::isfinite(zone.damage_per_second) || zone.damage_per_second < 0.0f) {
            error("invalid_damage", "$.survival_safe_zone.damage_per_second",
                  "damage_per_second must be finite and non-negative");
        }
        if (!std::isfinite(zone.radar_pulse_interval_seconds) ||
            zone.radar_pulse_interval_seconds <= 0.0f) {
            error("invalid_radar_interval", "$.survival_safe_zone.radar_pulse_interval_seconds",
                  "radar pulse interval must be positive");
        }
        if (zone.phases.size() > limits_.max_safe_zone_phases) {
            error("collection_limit", "$.survival_safe_zone.phases",
                  "safe-zone phase count exceeds configured limit");
        }
        for (std::size_t index = 0; index < zone.phases.size(); ++index) {
            const SafeZonePhase& phase = zone.phases[index];
            const std::string path = "$.survival_safe_zone.phases[" + std::to_string(index) + "]";
            if (!std::isfinite(phase.duration_seconds) || phase.duration_seconds <= 0.0f) {
                error("invalid_duration", path + ".duration_seconds",
                      "phase duration must be positive");
            }
            if (!std::isfinite(phase.target_radius_m) || phase.target_radius_m <= 0.0f) {
                error("invalid_radius", path + ".target_radius_m",
                      "phase target radius must be positive");
            }
        }
    }

    const MapData& data_;
    const MapLimits& limits_;
    MapValidationResult result_{};
    std::unordered_set<std::string> ids_;
    std::unordered_set<std::string> surface_ids_;
    std::unordered_set<std::string> track_ids_;
    std::unordered_map<std::string, std::string> checkpoint_track_ids_;
};

} // namespace

MapValidationResult MapValidator::validate(const MapData& data, const MapLimits& limits) {
    Validator validator(data, limits);
    return validator.run();
}

} // namespace rr::world
