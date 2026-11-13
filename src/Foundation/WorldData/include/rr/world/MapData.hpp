#pragma once

#include "rr/core/Math.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rr::world {

/// New TASK_002 map schema. This is intentionally not legacy MapData v3.
inline constexpr std::uint32_t kMapSchemaVersion = 1;
inline constexpr std::uint32_t kMapMaxPlayers = 8;

enum class MapSpaceType : std::uint8_t {
    Racing = 0,
    Survival = 1,
    Mixed = 2,
};

struct MapMetadata {
    std::uint32_t schema_version{kMapSchemaVersion};
    std::string map_id{};
    std::string name{};
    std::string author{};
    MapSpaceType space{MapSpaceType::Racing};
    std::uint32_t max_players{kMapMaxPlayers};
};

/// Shared, renderer-neutral surface definition.
///
/// `renderer_material_id` is an opaque project-owned reference, not a GPU
/// object. A physical surface id is not automatically a Jolt material index.
struct TrackSurface {
    std::string surface_id{};
    float grip_multiplier{1.0f};
    float rolling_resistance{0.0f};
    std::string renderer_material_id{};
};

struct TrackPoint {
    rr::math::Vec3 position{};
    float road_width_m{12.0f};
    float banking_rad{0.0f};
    std::string surface_id{};
};

struct CrossSectionProfile {
    float left_shoulder_width_m{1.5f};
    float right_shoulder_width_m{1.5f};
    float shoulder_slope_rad{0.0f};
    std::string left_shoulder_surface_id{};
    std::string right_shoulder_surface_id{};
};

struct TrackSpline {
    std::string track_id{};
    bool closed{false};
    std::vector<TrackPoint> points{};
    float texture_repeat_m{10.0f};
    float max_sample_spacing_m{3.0f};
    float max_sagitta_error_m{0.1f};
    CrossSectionProfile cross_section{};
};

struct Checkpoint {
    std::string checkpoint_id{};
    std::string track_id{};
    rr::math::Vec3 position{};
    float radius_m{10.0f};
    bool is_finish{false};
};

struct RouteEdge {
    std::string from_checkpoint_id{};
    std::string to_checkpoint_id{};
};

struct RouteDefinition {
    std::string route_id{};
    std::string track_id{};
    std::string start_checkpoint_id{};
    std::string finish_checkpoint_id{};
    std::vector<std::string> mandatory_checkpoint_ids{};
    std::vector<RouteEdge> edges{};
    std::uint32_t required_laps{1};
};

struct SpawnPoint {
    std::string spawn_id{};
    std::string track_id{};
    rr::math::Vec3 position{};
    rr::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct PickupSpawn {
    std::string pickup_spawn_id{};
    rr::math::Vec3 position{};
    std::string pickup_kind{};
    float respawn_seconds{10.0f};
};

struct InteractiveObject {
    std::string object_id{};
    rr::math::Vec3 position{};
    rr::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    std::string kind{};
    float radius_m{1.0f};
};

struct DestructibleObject {
    std::string object_id{};
    rr::math::Vec3 position{};
    rr::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    std::string kind{};
    float health{100.0f};
    float radius_m{1.0f};
};

/// Declarative trigger data only. No scripts, native code or network endpoints.
struct TriggerZone {
    std::string trigger_id{};
    rr::math::Vec3 center{};
    rr::math::Vec3 half_extents{1.0f, 1.0f, 1.0f};
    bool trigger_once{false};
    std::string event_type{};
    std::string action_type{};
    float cooldown_seconds{0.0f};
    std::uint32_t max_activations{1};
};

struct SafeZonePhase {
    float duration_seconds{60.0f};
    float target_radius_m{100.0f};
};

struct SurvivalSafeZone {
    rr::math::Vec3 start_center{};
    rr::math::Vec3 end_center{};
    float start_radius_m{100.0f};
    float end_radius_m{50.0f};
    float damage_per_second{1.0f};
    float radar_pulse_interval_seconds{10.0f};
    std::vector<SafeZonePhase> phases{};
};

/// Authoritative, renderer/physics/network-neutral map data.
struct MapData {
    MapMetadata metadata{};
    std::vector<TrackSurface> surfaces{};
    std::vector<TrackSpline> tracks{};
    std::vector<Checkpoint> checkpoints{};
    std::vector<RouteDefinition> routes{};
    std::vector<SpawnPoint> spawn_points{};
    std::vector<PickupSpawn> pickup_spawns{};
    std::vector<InteractiveObject> interactive_objects{};
    std::vector<DestructibleObject> destructibles{};
    std::vector<TriggerZone> trigger_zones{};
    std::optional<SurvivalSafeZone> survival_safe_zone{};
};

} // namespace rr::world
