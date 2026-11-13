#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace rr::physics {

using BodyId = std::uint32_t;
using VehicleId = std::uint32_t;

inline constexpr BodyId kInvalidBodyId = 0;
inline constexpr VehicleId kInvalidVehicleId = 0;
inline constexpr std::uint32_t kMaxVehicles = 8;
inline constexpr std::uint32_t kWheelCount = 4;

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Quat {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};
};

enum class ContactPhase : std::uint8_t {
    Began = 0,
    Persisted = 1,
    Ended = 2,
};

/// Normalized contact event produced by the physics backend.
///
/// The simulation layer uses Began/Persisted/Ended phases rather than raw Jolt
/// contact internals. This is the seam used by later impact-episode tests.
struct ContactEvent {
    BodyId body_a{kInvalidBodyId};
    BodyId body_b{kInvalidBodyId};
    ContactPhase phase{ContactPhase::Began};
    Vec3 point{};
    Vec3 normal{};
    float normal_impulse{0.0f};
    bool is_sensor{false};
};

struct GroundConfig {
    Vec3 position{0.0f, -0.5f, 0.0f};
    Vec3 half_extent{100.0f, 0.5f, 100.0f};
    float friction{0.8f};
};

struct WheelConfig {
    Vec3 position{}; // local chassis space
    float radius{0.35f};
    float width{0.25f};
    float suspension_min_length{0.12f};
    float suspension_max_length{0.35f};
    float suspension_frequency{2.0f};
    float suspension_damping{0.5f};
    bool steerable{false};
    bool driven{false};
};

struct VehicleConfig {
    Vec3 initial_position{0.0f, 1.5f, 0.0f};
    Quat initial_rotation{};
    Vec3 chassis_half_extent{0.9f, 0.35f, 2.0f};
    float chassis_mass{1200.0f};
    Vec3 center_of_mass{0.0f, -0.1f, 0.0f};
    std::array<WheelConfig, kWheelCount> wheels{};

    float max_engine_torque{600.0f};
    float max_steer_angle{0.5f};
    float max_brake_torque{2000.0f};
    float max_handbrake_torque{3000.0f};
    float reverse_engine_torque{200.0f};
    float engine_min_rpm{500.0f};
    float engine_max_rpm{6000.0f};
};

/// Normalized control input, one per vehicle per simulation tick.
struct VehicleInput {
    float throttle{0.0f};  // -1.0 .. 1.0
    float steering{0.0f};  // -1.0 .. 1.0
    float brake{0.0f};     // 0.0 .. 1.0
    float handbrake{0.0f}; // 0.0 .. 1.0
};

struct VehicleState {
    Vec3 position{};
    Quat rotation{};
    Vec3 linear_velocity{};
    Vec3 angular_velocity{};
    std::array<float, kWheelCount> wheel_rotation_speed{};
    std::array<float, kWheelCount> suspension_length{};
    std::array<bool, kWheelCount> wheel_grounded{};
    float forward_speed{0.0f};
    float engine_rpm{0.0f};
    bool grounded{false};
};

struct RaycastRequest {
    Vec3 origin{};
    Vec3 direction{0.0f, -1.0f, 0.0f};
    float max_distance{100.0f};
};

struct RaycastHit {
    BodyId body{kInvalidBodyId};
    Vec3 point{};
    Vec3 normal{};
    float distance{0.0f};
};

struct VehicleSnapshot {
    VehicleId vehicle_id{kInvalidVehicleId};
    VehicleInput input{};
    VehicleState state{};
};

/// Bounded snapshot for reconciliation-support seams. It intentionally does
/// not expose Jolt internal memory or promise cross-platform bitwise replay.
struct WorldSnapshot {
    std::uint64_t tick{0};
    std::uint32_t vehicle_count{0};
    std::array<VehicleSnapshot, kMaxVehicles> vehicles{};
};

struct WorldConfig {
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    std::uint32_t max_bodies{1024};
    std::uint32_t max_vehicles{kMaxVehicles};
};

/// Coarse-grained, engine-facing physics contract.
///
/// The interface intentionally models world/vehicle lifecycle, fixed-step
/// updates, contact events, raycasts and bounded snapshots. It is not a thin
/// wrapper around every Jolt body, shape or wheel operation.
///
/// Implementations are not thread-safe. All calls must happen on the owning
/// simulation thread, except where a future implementation documents
/// otherwise.
class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /// Creates a static ground box. Returns kInvalidBodyId on failure.
    virtual BodyId create_ground(const GroundConfig& config) = 0;

    /// Returns kInvalidVehicleId on failure.
    virtual VehicleId create_vehicle(const VehicleConfig& config) = 0;
    virtual bool destroy_vehicle(VehicleId vehicle) = 0;

    virtual bool set_vehicle_input(VehicleId vehicle, const VehicleInput& input) = 0;

    /// Advances the world by exactly fixed_dt seconds. The caller owns the
    /// fixed-step schedule; physics must not use render frame duration.
    virtual void step(double fixed_dt) = 0;

    [[nodiscard]] virtual std::optional<VehicleState> vehicle_state(VehicleId vehicle) const = 0;

    /// Moves all pending normalized contact events to the caller. Events are
    /// cleared from the world.
    virtual std::vector<ContactEvent> consume_contact_events() = 0;

    [[nodiscard]] virtual std::optional<RaycastHit> raycast(const RaycastRequest& request) const = 0;

    /// Bounded state capture/restore. Returns false when the operation is not
    /// supported or the input is out of range.
    virtual bool capture_snapshot(WorldSnapshot& out) const = 0;
    virtual bool restore_snapshot(const WorldSnapshot& snapshot) = 0;

    [[nodiscard]] virtual std::uint32_t body_count() const = 0;

protected:
    PhysicsWorld() = default;
};

/// Factory implemented by the concrete backend target (rr_physics_jolt).
[[nodiscard]] std::unique_ptr<PhysicsWorld> create_physics_world(const WorldConfig& config);

} // namespace rr::physics
