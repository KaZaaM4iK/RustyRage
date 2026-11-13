#include "rr/world/MapSerializer.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rr/world/MapValidator.hpp"

namespace rr::world {
namespace {

using Json = nlohmann::json;

class ParseFailure final : public std::runtime_error {
public:
    explicit ParseFailure(const std::string& message)
        : std::runtime_error(message) {
    }
};

[[noreturn]] void fail(const std::string& path, const std::string& message) {
    throw ParseFailure(path + ": " + message);
}

std::string child_path(const std::string& path, std::string_view key) {
    return path + "." + std::string(key);
}

std::string index_path(const std::string& path, std::size_t index) {
    return path + "[" + std::to_string(index) + "]";
}

void expect_object(const Json& value, const std::string& path) {
    if (!value.is_object()) {
        fail(path, "expected an object");
    }
}

void expect_array(const Json& value, const std::string& path) {
    if (!value.is_array()) {
        fail(path, "expected an array");
    }
}

const Json& required(const Json& object, std::string_view key, const std::string& path) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        fail(path, "missing required field '" + std::string(key) + "'");
    }
    return *iterator;
}

void check_known_keys(const Json& object,
                      std::initializer_list<std::string_view> allowed,
                      const std::string& path) {
    expect_object(object, path);
    for (const auto& item : object.items()) {
        bool known = false;
        for (std::string_view key : allowed) {
            if (item.key() == key) {
                known = true;
                break;
            }
        }
        if (!known) {
            fail(child_path(path, item.key()), "unknown field");
        }
    }
}

std::string read_string(const Json& object,
                        std::string_view key,
                        const std::string& path,
                        const MapLimits& limits,
                        std::size_t max_bytes) {
    const Json& value = required(object, key, path);
    if (!value.is_string()) {
        fail(child_path(path, key), "expected a string");
    }
    std::string result = value.get<std::string>();
    if (result.size() > max_bytes || result.size() > limits.max_string_bytes) {
        fail(child_path(path, key), "string exceeds configured limit");
    }
    return result;
}

std::string read_optional_string(const Json& object,
                                 std::string_view key,
                                 const std::string& path,
                                 const MapLimits& limits,
                                 std::size_t max_bytes,
                                 std::string fallback = {}) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        return fallback;
    }
    if (!iterator->is_string()) {
        fail(child_path(path, key), "expected a string");
    }
    std::string result = iterator->get<std::string>();
    if (result.size() > max_bytes || result.size() > limits.max_string_bytes) {
        fail(child_path(path, key), "string exceeds configured limit");
    }
    return result;
}

bool read_bool(const Json& object, std::string_view key, const std::string& path) {
    const Json& value = required(object, key, path);
    if (!value.is_boolean()) {
        fail(child_path(path, key), "expected a boolean");
    }
    return value.get<bool>();
}

std::uint32_t read_uint32(const Json& object, std::string_view key, const std::string& path) {
    const Json& value = required(object, key, path);
    if (!value.is_number_integer()) {
        fail(child_path(path, key), "expected an integer");
    }
    const std::int64_t raw = value.get<std::int64_t>();
    if (raw < 0 || raw > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        fail(child_path(path, key), "integer is outside the u32 range");
    }
    return static_cast<std::uint32_t>(raw);
}

float read_float(const Json& object, std::string_view key, const std::string& path) {
    const Json& value = required(object, key, path);
    if (!value.is_number()) {
        fail(child_path(path, key), "expected a number");
    }
    const double raw = value.get<double>();
    if (!std::isfinite(raw)) {
        fail(child_path(path, key), "number must be finite");
    }
    return static_cast<float>(raw);
}

std::uint32_t read_optional_uint32(const Json& object,
                                   std::string_view key,
                                   const std::string& path,
                                   std::uint32_t fallback) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        return fallback;
    }
    if (!iterator->is_number_integer()) {
        fail(child_path(path, key), "expected an integer");
    }
    const std::int64_t raw = iterator->get<std::int64_t>();
    if (raw < 0 || raw > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        fail(child_path(path, key), "integer is outside the u32 range");
    }
    return static_cast<std::uint32_t>(raw);
}

float read_optional_float(const Json& object,
                          std::string_view key,
                          const std::string& path,
                          float fallback) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        return fallback;
    }
    if (!iterator->is_number()) {
        fail(child_path(path, key), "expected a number");
    }
    const double raw = iterator->get<double>();
    if (!std::isfinite(raw)) {
        fail(child_path(path, key), "number must be finite");
    }
    return static_cast<float>(raw);
}

rr::math::Vec3 read_vec3(const Json& object, std::string_view key, const std::string& path) {
    const Json& value = required(object, key, path);
    if (!value.is_array() || value.size() != 3) {
        fail(child_path(path, key), "expected a 3-element array");
    }
    for (std::size_t index = 0; index < 3; ++index) {
        if (!value[index].is_number()) {
            fail(index_path(child_path(path, key), index), "expected a number");
        }
    }
    const double x = value[0].get<double>();
    const double y = value[1].get<double>();
    const double z = value[2].get<double>();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        fail(child_path(path, key), "vector must be finite");
    }
    return rr::math::Vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
}

rr::math::Quat read_optional_quat(const Json& object,
                                  std::string_view key,
                                  const std::string& path,
                                  rr::math::Quat fallback) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        return fallback;
    }
    if (!iterator->is_array() || iterator->size() != 4) {
        fail(child_path(path, key), "expected a 4-element [x,y,z,w] array");
    }
    for (std::size_t index = 0; index < 4; ++index) {
        if (!(*iterator)[index].is_number()) {
            fail(index_path(child_path(path, key), index), "expected a number");
        }
    }
    const double x = (*iterator)[0].get<double>();
    const double y = (*iterator)[1].get<double>();
    const double z = (*iterator)[2].get<double>();
    const double w = (*iterator)[3].get<double>();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w)) {
        fail(child_path(path, key), "quaternion must be finite");
    }
    return rr::math::Quat{static_cast<float>(w), static_cast<float>(x),
                          static_cast<float>(y), static_cast<float>(z)};
}

std::vector<std::string> read_optional_string_array(const Json& object,
                                                    std::string_view key,
                                                    const std::string& path,
                                                    const MapLimits& limits,
                                                    std::size_t max_count) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        return {};
    }
    if (!iterator->is_array()) {
        fail(child_path(path, key), "expected an array");
    }
    if (iterator->size() > max_count) {
        fail(child_path(path, key), "array exceeds configured limit");
    }
    std::vector<std::string> result;
    result.reserve(iterator->size());
    for (std::size_t index = 0; index < iterator->size(); ++index) {
        if (!(*iterator)[index].is_string()) {
            fail(index_path(child_path(path, key), index), "expected a string");
        }
        std::string item = (*iterator)[index].get<std::string>();
        if (item.size() > limits.max_string_bytes) {
            fail(index_path(child_path(path, key), index), "string exceeds configured limit");
        }
        result.push_back(std::move(item));
    }
    return result;
}

MapSpaceType read_map_space(const Json& root, const MapLimits& limits) {
    const std::string value = read_string(root, "space", "$", limits, limits.max_kind_string_bytes);
    if (value == "racing") {
        return MapSpaceType::Racing;
    }
    if (value == "survival") {
        return MapSpaceType::Survival;
    }
    if (value == "mixed") {
        return MapSpaceType::Mixed;
    }
    fail("$.space", "expected 'racing', 'survival' or 'mixed'");
}

std::string map_space_to_string(MapSpaceType space) {
    switch (space) {
        case MapSpaceType::Racing:
            return "racing";
        case MapSpaceType::Survival:
            return "survival";
        case MapSpaceType::Mixed:
            return "mixed";
    }
    return "racing";
}

void check_depth(const Json& value,
                 std::size_t depth,
                 std::size_t max_depth,
                 const std::string& path) {
    if (depth > max_depth) {
        fail(path, "JSON nesting depth exceeds configured limit");
    }
    if (value.is_object()) {
        for (const auto& item : value.items()) {
            check_depth(item.value(), depth + 1, max_depth, child_path(path, item.key()));
        }
    } else if (value.is_array()) {
        for (std::size_t index = 0; index < value.size(); ++index) {
            check_depth(value[index], depth + 1, max_depth, index_path(path, index));
        }
    }
}

TrackSurface read_surface(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value,
                     {"surface_id", "grip_multiplier", "rolling_resistance", "material_id"},
                     path);
    TrackSurface surface;
    surface.surface_id = read_string(value, "surface_id", path, limits, limits.max_identifier_bytes);
    surface.grip_multiplier = read_optional_float(value, "grip_multiplier", path, 1.0f);
    surface.rolling_resistance = read_optional_float(value, "rolling_resistance", path, 0.0f);
    surface.renderer_material_id = read_optional_string(value, "material_id", path, limits,
                                                        limits.max_string_bytes);
    return surface;
}

CrossSectionProfile read_cross_section(const Json& object,
                                       const std::string& path,
                                       const MapLimits& limits) {
    const auto iterator = object.find("cross_section");
    if (iterator == object.end()) {
        return CrossSectionProfile{};
    }
    check_known_keys(*iterator,
                     {"left_shoulder_width_m", "right_shoulder_width_m",
                      "shoulder_slope_rad", "left_shoulder_surface_id",
                      "right_shoulder_surface_id"},
                     child_path(path, "cross_section"));
    CrossSectionProfile profile;
    profile.left_shoulder_width_m =
        read_optional_float(*iterator, "left_shoulder_width_m", child_path(path, "cross_section"), 1.5f);
    profile.right_shoulder_width_m =
        read_optional_float(*iterator, "right_shoulder_width_m", child_path(path, "cross_section"), 1.5f);
    profile.shoulder_slope_rad =
        read_optional_float(*iterator, "shoulder_slope_rad", child_path(path, "cross_section"), 0.0f);
    profile.left_shoulder_surface_id =
        read_optional_string(*iterator, "left_shoulder_surface_id", child_path(path, "cross_section"),
                             limits, limits.max_identifier_bytes);
    profile.right_shoulder_surface_id =
        read_optional_string(*iterator, "right_shoulder_surface_id", child_path(path, "cross_section"),
                             limits, limits.max_identifier_bytes);
    return profile;
}

TrackSpline read_track(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value,
                     {"track_id", "closed", "points", "texture_repeat_m",
                      "max_sample_spacing_m", "max_sagitta_error_m", "cross_section"},
                     path);
    TrackSpline track;
    track.track_id = read_string(value, "track_id", path, limits, limits.max_identifier_bytes);
    track.closed = read_bool(value, "closed", path);
    track.texture_repeat_m = read_optional_float(value, "texture_repeat_m", path, 10.0f);
    track.max_sample_spacing_m = read_optional_float(value, "max_sample_spacing_m", path, 3.0f);
    track.max_sagitta_error_m = read_optional_float(value, "max_sagitta_error_m", path, 0.1f);
    track.cross_section = read_cross_section(value, path, limits);

    const Json& points = required(value, "points", path);
    expect_array(points, child_path(path, "points"));
    if (points.size() > limits.max_control_points_per_track) {
        fail(child_path(path, "points"), "control point count exceeds configured limit");
    }
    track.points.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
        const std::string point_path = index_path(child_path(path, "points"), index);
        check_known_keys(points[index],
                         {"position", "road_width_m", "banking_rad", "surface_id"},
                         point_path);
        TrackPoint point;
        point.position = read_vec3(points[index], "position", point_path);
        point.road_width_m = read_float(points[index], "road_width_m", point_path);
        point.banking_rad = read_float(points[index], "banking_rad", point_path);
        point.surface_id = read_string(points[index], "surface_id", point_path, limits,
                                       limits.max_identifier_bytes);
        track.points.push_back(std::move(point));
    }
    return track;
}

Checkpoint read_checkpoint(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value, {"checkpoint_id", "track_id", "position", "radius_m", "is_finish"}, path);
    Checkpoint checkpoint;
    checkpoint.checkpoint_id = read_string(value, "checkpoint_id", path, limits,
                                            limits.max_identifier_bytes);
    checkpoint.track_id = read_string(value, "track_id", path, limits, limits.max_identifier_bytes);
    checkpoint.position = read_vec3(value, "position", path);
    checkpoint.radius_m = read_float(value, "radius_m", path);
    checkpoint.is_finish = read_bool(value, "is_finish", path);
    return checkpoint;
}

RouteDefinition read_route(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value,
                     {"route_id", "track_id", "start_checkpoint_id", "finish_checkpoint_id",
                      "mandatory_checkpoint_ids", "edges", "required_laps"},
                     path);
    RouteDefinition route;
    route.route_id = read_string(value, "route_id", path, limits, limits.max_identifier_bytes);
    route.track_id = read_string(value, "track_id", path, limits, limits.max_identifier_bytes);
    route.start_checkpoint_id =
        read_string(value, "start_checkpoint_id", path, limits, limits.max_identifier_bytes);
    route.finish_checkpoint_id =
        read_string(value, "finish_checkpoint_id", path, limits, limits.max_identifier_bytes);
    route.mandatory_checkpoint_ids =
        read_optional_string_array(value, "mandatory_checkpoint_ids", path, limits,
                                   limits.max_checkpoints);
    route.required_laps = read_optional_uint32(value, "required_laps", path, 1);

    const auto edges_iterator = value.find("edges");
    if (edges_iterator != value.end()) {
        expect_array(*edges_iterator, child_path(path, "edges"));
        if (edges_iterator->size() > limits.max_route_edges) {
            fail(child_path(path, "edges"), "route edge count exceeds configured limit");
        }
        route.edges.reserve(edges_iterator->size());
        for (std::size_t index = 0; index < edges_iterator->size(); ++index) {
            const std::string edge_path = index_path(child_path(path, "edges"), index);
            const Json& edge = (*edges_iterator)[index];
            check_known_keys(edge, {"from", "to"}, edge_path);
            RouteEdge route_edge;
            route_edge.from_checkpoint_id =
                read_string(edge, "from", edge_path, limits, limits.max_identifier_bytes);
            route_edge.to_checkpoint_id =
                read_string(edge, "to", edge_path, limits, limits.max_identifier_bytes);
            route.edges.push_back(std::move(route_edge));
        }
    }
    return route;
}

SpawnPoint read_spawn(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value, {"spawn_id", "track_id", "position", "rotation"}, path);
    SpawnPoint spawn;
    spawn.spawn_id = read_string(value, "spawn_id", path, limits, limits.max_identifier_bytes);
    spawn.track_id = read_string(value, "track_id", path, limits, limits.max_identifier_bytes);
    spawn.position = read_vec3(value, "position", path);
    spawn.rotation = read_optional_quat(value, "rotation", path,
                                        rr::math::Quat{1.0f, 0.0f, 0.0f, 0.0f});
    return spawn;
}

PickupSpawn read_pickup(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value, {"pickup_spawn_id", "position", "pickup_kind", "respawn_seconds"}, path);
    PickupSpawn pickup;
    pickup.pickup_spawn_id = read_string(value, "pickup_spawn_id", path, limits,
                                          limits.max_identifier_bytes);
    pickup.position = read_vec3(value, "position", path);
    pickup.pickup_kind = read_string(value, "pickup_kind", path, limits,
                                     limits.max_kind_string_bytes);
    pickup.respawn_seconds = read_optional_float(value, "respawn_seconds", path, 10.0f);
    return pickup;
}

InteractiveObject read_interactive(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value, {"object_id", "position", "rotation", "kind", "radius_m"}, path);
    InteractiveObject object;
    object.object_id = read_string(value, "object_id", path, limits, limits.max_identifier_bytes);
    object.position = read_vec3(value, "position", path);
    object.rotation = read_optional_quat(value, "rotation", path,
                                         rr::math::Quat{1.0f, 0.0f, 0.0f, 0.0f});
    object.kind = read_string(value, "kind", path, limits, limits.max_kind_string_bytes);
    object.radius_m = read_optional_float(value, "radius_m", path, 1.0f);
    return object;
}

DestructibleObject read_destructible(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value, {"object_id", "position", "rotation", "kind", "health", "radius_m"}, path);
    DestructibleObject object;
    object.object_id = read_string(value, "object_id", path, limits, limits.max_identifier_bytes);
    object.position = read_vec3(value, "position", path);
    object.rotation = read_optional_quat(value, "rotation", path,
                                         rr::math::Quat{1.0f, 0.0f, 0.0f, 0.0f});
    object.kind = read_string(value, "kind", path, limits, limits.max_kind_string_bytes);
    object.health = read_optional_float(value, "health", path, 100.0f);
    object.radius_m = read_optional_float(value, "radius_m", path, 1.0f);
    return object;
}

TriggerZone read_trigger(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value,
                     {"trigger_id", "center", "half_extents", "trigger_once", "event_type",
                      "action_type", "cooldown_seconds", "max_activations"},
                     path);
    TriggerZone trigger;
    trigger.trigger_id = read_string(value, "trigger_id", path, limits, limits.max_identifier_bytes);
    trigger.center = read_vec3(value, "center", path);
    trigger.half_extents = read_vec3(value, "half_extents", path);
    trigger.trigger_once = read_bool(value, "trigger_once", path);
    trigger.event_type = read_string(value, "event_type", path, limits, limits.max_kind_string_bytes);
    trigger.action_type = read_string(value, "action_type", path, limits, limits.max_kind_string_bytes);
    trigger.cooldown_seconds = read_optional_float(value, "cooldown_seconds", path, 0.0f);
    trigger.max_activations = read_uint32(value, "max_activations", path);
    return trigger;
}

SurvivalSafeZone read_safe_zone(const Json& value, const std::string& path, const MapLimits& limits) {
    check_known_keys(value,
                     {"start_center", "end_center", "start_radius_m", "end_radius_m",
                      "damage_per_second", "radar_pulse_interval_seconds", "phases"},
                     path);
    SurvivalSafeZone zone;
    zone.start_center = read_vec3(value, "start_center", path);
    zone.end_center = read_vec3(value, "end_center", path);
    zone.start_radius_m = read_float(value, "start_radius_m", path);
    zone.end_radius_m = read_float(value, "end_radius_m", path);
    zone.damage_per_second = read_optional_float(value, "damage_per_second", path, 1.0f);
    zone.radar_pulse_interval_seconds =
        read_optional_float(value, "radar_pulse_interval_seconds", path, 10.0f);

    const auto phases = value.find("phases");
    if (phases != value.end()) {
        expect_array(*phases, child_path(path, "phases"));
        if (phases->size() > limits.max_safe_zone_phases) {
            fail(child_path(path, "phases"), "safe-zone phase count exceeds configured limit");
        }
        zone.phases.reserve(phases->size());
        for (std::size_t index = 0; index < phases->size(); ++index) {
            const std::string phase_path = index_path(child_path(path, "phases"), index);
            check_known_keys((*phases)[index], {"duration_seconds", "target_radius_m"}, phase_path);
            SafeZonePhase phase;
            phase.duration_seconds = read_float((*phases)[index], "duration_seconds", phase_path);
            phase.target_radius_m = read_float((*phases)[index], "target_radius_m", phase_path);
            zone.phases.push_back(phase);
        }
    }
    return zone;
}

const Json& optional_array(const Json& root, std::string_view key, const std::string& path) {
    const auto iterator = root.find(std::string(key));
    if (iterator != root.end()) {
        expect_array(*iterator, child_path(path, key));
        return *iterator;
    }
    static const Json empty = Json::array();
    return empty;
}

} // namespace

std::optional<MapData> MapSerializer::parse(std::string_view json,
                                            const MapLimits& limits,
                                            std::string& error) {
    if (json.size() > limits.max_document_bytes) {
        error = "$: document exceeds configured size limit";
        return std::nullopt;
    }

    Json root;
    try {
        root = Json::parse(json.begin(), json.end());
    } catch (const Json::exception& exception) {
        error = "JSON parse error: ";
        error += exception.what();
        return std::nullopt;
    } catch (const std::exception& exception) {
        error = "JSON parse failure: ";
        error += exception.what();
        return std::nullopt;
    }

    try {
        check_depth(root, 0, limits.max_json_nesting_depth, "$");
        check_known_keys(root,
                         {"schema_version", "map_id", "name", "author", "space", "max_players",
                          "surfaces", "tracks", "checkpoints", "routes", "spawn_points",
                          "pickup_spawns", "interactive_objects", "destructibles",
                          "trigger_zones", "survival_safe_zone"},
                         "$");

        MapData data;
        data.metadata.schema_version = read_uint32(root, "schema_version", "$");
        if (data.metadata.schema_version != kMapSchemaVersion) {
            error = "$.schema_version: unsupported schema version " +
                    std::to_string(data.metadata.schema_version) +
                    "; expected " + std::to_string(kMapSchemaVersion);
            return std::nullopt;
        }
        data.metadata.map_id = read_string(root, "map_id", "$", limits, limits.max_identifier_bytes);
        data.metadata.name = read_string(root, "name", "$", limits, limits.max_string_bytes);
        data.metadata.author = read_string(root, "author", "$", limits, limits.max_string_bytes);
        data.metadata.space = read_map_space(root, limits);
        data.metadata.max_players = read_uint32(root, "max_players", "$");

        const Json& surfaces = optional_array(root, "surfaces", "$");
        if (surfaces.size() > limits.max_surfaces) {
            error = "$.surfaces: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < surfaces.size(); ++index) {
            data.surfaces.push_back(read_surface(surfaces[index],
                                                 index_path("$.surfaces", index), limits));
        }

        const Json& tracks = optional_array(root, "tracks", "$");
        if (tracks.size() > limits.max_tracks) {
            error = "$.tracks: collection exceeds configured limit";
            return std::nullopt;
        }
        std::size_t total_points = 0;
        for (std::size_t index = 0; index < tracks.size(); ++index) {
            TrackSpline track = read_track(tracks[index], index_path("$.tracks", index), limits);
            total_points += track.points.size();
            if (total_points > limits.max_total_control_points) {
                error = "$.tracks: total control point count exceeds configured limit";
                return std::nullopt;
            }
            data.tracks.push_back(std::move(track));
        }

        const Json& checkpoints = optional_array(root, "checkpoints", "$");
        if (checkpoints.size() > limits.max_checkpoints) {
            error = "$.checkpoints: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < checkpoints.size(); ++index) {
            data.checkpoints.push_back(read_checkpoint(checkpoints[index],
                                                       index_path("$.checkpoints", index), limits));
        }

        const Json& routes = optional_array(root, "routes", "$");
        if (routes.size() > limits.max_routes) {
            error = "$.routes: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < routes.size(); ++index) {
            data.routes.push_back(read_route(routes[index], index_path("$.routes", index), limits));
        }

        const Json& spawns = optional_array(root, "spawn_points", "$");
        if (spawns.size() > limits.max_spawn_points) {
            error = "$.spawn_points: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < spawns.size(); ++index) {
            data.spawn_points.push_back(read_spawn(spawns[index],
                                                   index_path("$.spawn_points", index), limits));
        }

        const Json& pickups = optional_array(root, "pickup_spawns", "$");
        if (pickups.size() > limits.max_pickup_spawns) {
            error = "$.pickup_spawns: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < pickups.size(); ++index) {
            data.pickup_spawns.push_back(read_pickup(pickups[index],
                                                     index_path("$.pickup_spawns", index), limits));
        }

        const Json& interactives = optional_array(root, "interactive_objects", "$");
        if (interactives.size() > limits.max_interactive_objects) {
            error = "$.interactive_objects: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < interactives.size(); ++index) {
            data.interactive_objects.push_back(read_interactive(
                interactives[index], index_path("$.interactive_objects", index), limits));
        }

        const Json& destructibles = optional_array(root, "destructibles", "$");
        if (destructibles.size() > limits.max_destructibles) {
            error = "$.destructibles: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < destructibles.size(); ++index) {
            data.destructibles.push_back(read_destructible(
                destructibles[index], index_path("$.destructibles", index), limits));
        }

        const Json& triggers = optional_array(root, "trigger_zones", "$");
        if (triggers.size() > limits.max_trigger_zones) {
            error = "$.trigger_zones: collection exceeds configured limit";
            return std::nullopt;
        }
        for (std::size_t index = 0; index < triggers.size(); ++index) {
            data.trigger_zones.push_back(read_trigger(triggers[index],
                                                      index_path("$.trigger_zones", index), limits));
        }

        const auto safe_zone = root.find("survival_safe_zone");
        if (safe_zone != root.end() && !safe_zone->is_null()) {
            data.survival_safe_zone = read_safe_zone(*safe_zone, "$.survival_safe_zone", limits);
        }

        error.clear();
        return data;
    } catch (const ParseFailure& failure) {
        error = failure.what();
        return std::nullopt;
    } catch (const Json::exception& exception) {
        error = "JSON field error: ";
        error += exception.what();
        return std::nullopt;
    } catch (const std::exception& exception) {
        error = "map parse failed: ";
        error += exception.what();
        return std::nullopt;
    }
}

std::optional<MapData> MapSerializer::load_file(const std::string& path,
                                                const MapLimits& limits,
                                                std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open map file: " + path;
        return std::nullopt;
    }
    const std::streamsize size = file.tellg();
    if (size < 0) {
        error = "cannot determine map file size: " + path;
        return std::nullopt;
    }
    if (static_cast<std::size_t>(size) > limits.max_document_bytes) {
        error = "map file exceeds configured size limit: " + path;
        return std::nullopt;
    }

    std::string buffer(static_cast<std::size_t>(size), '\0');
    file.seekg(0, std::ios::beg);
    if (!file.read(buffer.data(), size)) {
        error = "cannot read map file: " + path;
        return std::nullopt;
    }
    return parse(buffer, limits, error);
}

std::string MapSerializer::write(const MapData& data,
                                 const MapLimits& limits,
                                 std::string& error) {
    const MapValidationResult validation = MapValidator::validate(data, limits);
    if (!validation.ok()) {
        for (const MapDiagnostic& diagnostic : validation.diagnostics) {
            if (diagnostic.severity == MapDiagnosticSeverity::Error) {
                error = diagnostic.path + ": " + diagnostic.message;
                return {};
            }
        }
        error = "map validation failed";
        return {};
    }

    try {
        Json root;
        root["schema_version"] = data.metadata.schema_version;
        root["map_id"] = data.metadata.map_id;
        root["name"] = data.metadata.name;
        root["author"] = data.metadata.author;
        root["space"] = map_space_to_string(data.metadata.space);
        root["max_players"] = data.metadata.max_players;

        root["surfaces"] = Json::array();
        for (const TrackSurface& surface : data.surfaces) {
            Json item;
            item["surface_id"] = surface.surface_id;
            item["grip_multiplier"] = surface.grip_multiplier;
            item["rolling_resistance"] = surface.rolling_resistance;
            item["material_id"] = surface.renderer_material_id;
            root["surfaces"].push_back(std::move(item));
        }

        root["tracks"] = Json::array();
        for (const TrackSpline& track : data.tracks) {
            Json item;
            item["track_id"] = track.track_id;
            item["closed"] = track.closed;
            item["texture_repeat_m"] = track.texture_repeat_m;
            item["max_sample_spacing_m"] = track.max_sample_spacing_m;
            item["max_sagitta_error_m"] = track.max_sagitta_error_m;
            item["cross_section"] = {
                {"left_shoulder_width_m", track.cross_section.left_shoulder_width_m},
                {"right_shoulder_width_m", track.cross_section.right_shoulder_width_m},
                {"shoulder_slope_rad", track.cross_section.shoulder_slope_rad},
                {"left_shoulder_surface_id", track.cross_section.left_shoulder_surface_id},
                {"right_shoulder_surface_id", track.cross_section.right_shoulder_surface_id},
            };
            item["points"] = Json::array();
            for (const TrackPoint& point : track.points) {
                Json point_json;
                point_json["position"] = {point.position.x, point.position.y, point.position.z};
                point_json["road_width_m"] = point.road_width_m;
                point_json["banking_rad"] = point.banking_rad;
                point_json["surface_id"] = point.surface_id;
                item["points"].push_back(std::move(point_json));
            }
            root["tracks"].push_back(std::move(item));
        }

        root["checkpoints"] = Json::array();
        for (const Checkpoint& checkpoint : data.checkpoints) {
            Json item;
            item["checkpoint_id"] = checkpoint.checkpoint_id;
            item["track_id"] = checkpoint.track_id;
            item["position"] = {checkpoint.position.x, checkpoint.position.y, checkpoint.position.z};
            item["radius_m"] = checkpoint.radius_m;
            item["is_finish"] = checkpoint.is_finish;
            root["checkpoints"].push_back(std::move(item));
        }

        root["routes"] = Json::array();
        for (const RouteDefinition& route : data.routes) {
            Json item;
            item["route_id"] = route.route_id;
            item["track_id"] = route.track_id;
            item["start_checkpoint_id"] = route.start_checkpoint_id;
            item["finish_checkpoint_id"] = route.finish_checkpoint_id;
            item["mandatory_checkpoint_ids"] = route.mandatory_checkpoint_ids;
            item["required_laps"] = route.required_laps;
            item["edges"] = Json::array();
            for (const RouteEdge& edge : route.edges) {
                item["edges"].push_back({{"from", edge.from_checkpoint_id},
                                         {"to", edge.to_checkpoint_id}});
            }
            root["routes"].push_back(std::move(item));
        }

        root["spawn_points"] = Json::array();
        for (const SpawnPoint& spawn : data.spawn_points) {
            Json item;
            item["spawn_id"] = spawn.spawn_id;
            item["track_id"] = spawn.track_id;
            item["position"] = {spawn.position.x, spawn.position.y, spawn.position.z};
            item["rotation"] = {spawn.rotation.x, spawn.rotation.y, spawn.rotation.z, spawn.rotation.w};
            root["spawn_points"].push_back(std::move(item));
        }

        root["pickup_spawns"] = Json::array();
        for (const PickupSpawn& pickup : data.pickup_spawns) {
            Json item;
            item["pickup_spawn_id"] = pickup.pickup_spawn_id;
            item["position"] = {pickup.position.x, pickup.position.y, pickup.position.z};
            item["pickup_kind"] = pickup.pickup_kind;
            item["respawn_seconds"] = pickup.respawn_seconds;
            root["pickup_spawns"].push_back(std::move(item));
        }

        root["interactive_objects"] = Json::array();
        for (const InteractiveObject& object : data.interactive_objects) {
            Json item;
            item["object_id"] = object.object_id;
            item["position"] = {object.position.x, object.position.y, object.position.z};
            item["rotation"] = {object.rotation.x, object.rotation.y, object.rotation.z, object.rotation.w};
            item["kind"] = object.kind;
            item["radius_m"] = object.radius_m;
            root["interactive_objects"].push_back(std::move(item));
        }

        root["destructibles"] = Json::array();
        for (const DestructibleObject& object : data.destructibles) {
            Json item;
            item["object_id"] = object.object_id;
            item["position"] = {object.position.x, object.position.y, object.position.z};
            item["rotation"] = {object.rotation.x, object.rotation.y, object.rotation.z, object.rotation.w};
            item["kind"] = object.kind;
            item["health"] = object.health;
            item["radius_m"] = object.radius_m;
            root["destructibles"].push_back(std::move(item));
        }

        root["trigger_zones"] = Json::array();
        for (const TriggerZone& trigger : data.trigger_zones) {
            Json item;
            item["trigger_id"] = trigger.trigger_id;
            item["center"] = {trigger.center.x, trigger.center.y, trigger.center.z};
            item["half_extents"] = {trigger.half_extents.x, trigger.half_extents.y, trigger.half_extents.z};
            item["trigger_once"] = trigger.trigger_once;
            item["event_type"] = trigger.event_type;
            item["action_type"] = trigger.action_type;
            item["cooldown_seconds"] = trigger.cooldown_seconds;
            item["max_activations"] = trigger.max_activations;
            root["trigger_zones"].push_back(std::move(item));
        }

        if (data.survival_safe_zone.has_value()) {
            const SurvivalSafeZone& zone = *data.survival_safe_zone;
            Json item;
            item["start_center"] = {zone.start_center.x, zone.start_center.y, zone.start_center.z};
            item["end_center"] = {zone.end_center.x, zone.end_center.y, zone.end_center.z};
            item["start_radius_m"] = zone.start_radius_m;
            item["end_radius_m"] = zone.end_radius_m;
            item["damage_per_second"] = zone.damage_per_second;
            item["radar_pulse_interval_seconds"] = zone.radar_pulse_interval_seconds;
            item["phases"] = Json::array();
            for (const SafeZonePhase& phase : zone.phases) {
                item["phases"].push_back({{"duration_seconds", phase.duration_seconds},
                                           {"target_radius_m", phase.target_radius_m}});
            }
            root["survival_safe_zone"] = std::move(item);
        }

        std::string output = root.dump(2);
        output.push_back('\n');
        if (output.size() > limits.max_document_bytes) {
            error = "serialized map exceeds configured size limit";
            return {};
        }
        error.clear();
        return output;
    } catch (const std::exception& exception) {
        error = "map serialization failed: ";
        error += exception.what();
        return {};
    }
}

bool MapSerializer::write_file(const std::string& path,
                               const MapData& data,
                               const MapLimits& limits,
                               std::string& error) {
    const std::string output = write(data, limits, error);
    if (output.empty()) {
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot open map file for writing: " + path;
        return false;
    }
    file.write(output.data(), static_cast<std::streamsize>(output.size()));
    if (!file) {
        error = "failed while writing map file: " + path;
        return false;
    }
    error.clear();
    return true;
}

} // namespace rr::world
