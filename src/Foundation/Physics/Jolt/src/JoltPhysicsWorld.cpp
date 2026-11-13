#include "rr/physics/PhysicsApi.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace Layers {
constexpr JPH::ObjectLayer kNonMoving = 0;
constexpr JPH::ObjectLayer kMoving = 1;
} // namespace Layers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer kNonMoving(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
} // namespace BroadPhaseLayers

class JoltRuntime {
public:
    JoltRuntime() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }

    ~JoltRuntime() {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    JoltRuntime(const JoltRuntime&) = delete;
    JoltRuntime& operator=(const JoltRuntime&) = delete;
};

JoltRuntime& jolt_runtime() {
    static JoltRuntime runtime;
    return runtime;
}

struct JoltRuntimeGuard {
    JoltRuntimeGuard() {
        static_cast<void>(jolt_runtime());
    }
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        object_to_broad_phase_[Layers::kNonMoving] = BroadPhaseLayers::kNonMoving;
        object_to_broad_phase_[Layers::kMoving] = BroadPhaseLayers::kMoving;
    }

    JPH::uint GetNumBroadPhaseLayers() const override {
        return 2;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        if (layer >= 2) {
            return BroadPhaseLayers::kNonMoving;
        }
        return object_to_broad_phase_[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == BroadPhaseLayers::kNonMoving ? "NON_MOVING" : "MOVING";
    }
#endif

private:
    JPH::BroadPhaseLayer object_to_broad_phase_[2]{BroadPhaseLayers::kNonMoving,
                                                   BroadPhaseLayers::kMoving};
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object_layer,
                       JPH::BroadPhaseLayer broad_phase_layer) const override {
        if (object_layer == Layers::kNonMoving) {
            return broad_phase_layer == BroadPhaseLayers::kMoving;
        }
        return broad_phase_layer == BroadPhaseLayers::kNonMoving ||
               broad_phase_layer == BroadPhaseLayers::kMoving;
    }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override {
        return !(first == Layers::kNonMoving && second == Layers::kNonMoving);
    }
};

JPH::Vec3 to_jolt(rr::physics::Vec3 value) {
    return JPH::Vec3(value.x, value.y, value.z);
}

JPH::RVec3 to_jolt_position(rr::physics::Vec3 value) {
    return JPH::RVec3(value.x, value.y, value.z);
}

JPH::Quat to_jolt(rr::physics::Quat value) {
    return JPH::Quat(value.x, value.y, value.z, value.w);
}

rr::physics::Vec3 from_jolt(JPH::Vec3Arg value) {
    return rr::physics::Vec3{value.GetX(), value.GetY(), value.GetZ()};
}

rr::physics::Vec3 from_jolt_position(JPH::RVec3Arg value) {
    return rr::physics::Vec3{static_cast<float>(value.GetX()),
                             static_cast<float>(value.GetY()),
                             static_cast<float>(value.GetZ())};
}

rr::physics::Quat from_jolt(JPH::QuatArg value) {
    return rr::physics::Quat{value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}

bool finite(rr::physics::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool valid_wheel(const rr::physics::WheelConfig& wheel) {
    return finite(wheel.position) &&
           std::isfinite(wheel.radius) && wheel.radius > 0.0f &&
           std::isfinite(wheel.width) && wheel.width > 0.0f &&
           std::isfinite(wheel.suspension_min_length) && wheel.suspension_min_length >= 0.0f &&
           std::isfinite(wheel.suspension_max_length) &&
           wheel.suspension_max_length >= wheel.suspension_min_length &&
           std::isfinite(wheel.suspension_frequency) && wheel.suspension_frequency > 0.0f &&
           std::isfinite(wheel.suspension_damping) && wheel.suspension_damping >= 0.0f;
}

bool valid_vehicle_config(const rr::physics::VehicleConfig& config) {
    if (!finite(config.initial_position) || !finite(config.chassis_half_extent) ||
        !finite(config.center_of_mass)) {
        return false;
    }
    if (config.chassis_half_extent.x <= 0.0f ||
        config.chassis_half_extent.y <= 0.0f ||
        config.chassis_half_extent.z <= 0.0f) {
        return false;
    }
    if (!std::isfinite(config.chassis_mass) || config.chassis_mass <= 0.0f) {
        return false;
    }
    if (!std::isfinite(config.max_engine_torque) || config.max_engine_torque <= 0.0f) {
        return false;
    }
    if (!std::isfinite(config.max_steer_angle) || config.max_steer_angle <= 0.0f) {
        return false;
    }
    if (!std::isfinite(config.max_brake_torque) || config.max_brake_torque < 0.0f ||
        !std::isfinite(config.max_handbrake_torque) || config.max_handbrake_torque < 0.0f) {
        return false;
    }
    if (!std::isfinite(config.engine_min_rpm) || config.engine_min_rpm <= 0.0f ||
        !std::isfinite(config.engine_max_rpm) ||
        config.engine_max_rpm <= config.engine_min_rpm) {
        return false;
    }

    return std::all_of(config.wheels.begin(), config.wheels.end(), valid_wheel);
}

bool valid_input(const rr::physics::VehicleInput& input) {
    const auto in_range = [](float value, float minimum, float maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    };
    return in_range(input.throttle, -1.0f, 1.0f) &&
           in_range(input.steering, -1.0f, 1.0f) &&
           in_range(input.brake, 0.0f, 1.0f) &&
           in_range(input.handbrake, 0.0f, 1.0f);
}

int worker_thread_count() {
    const unsigned hardware = std::thread::hardware_concurrency();
    if (hardware <= 1) {
        return 0;
    }
    return static_cast<int>(hardware - 1);
}

class JoltContactListener final : public JPH::ContactListener {
public:
    void remember_body(const JPH::BodyID& body, rr::physics::BodyId game_id) {
        body_ids_[body.GetIndexAndSequenceNumber()] = game_id;
    }

    void OnContactAdded(const JPH::Body& body1,
                        const JPH::Body& body2,
                        const JPH::ContactManifold& manifold,
                        JPH::ContactSettings& settings) override {
        record(body1, body2, manifold, settings, rr::physics::ContactPhase::Began);
    }

    void OnContactPersisted(const JPH::Body& body1,
                            const JPH::Body& body2,
                            const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override {
        record(body1, body2, manifold, settings, rr::physics::ContactPhase::Persisted);
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
        rr::physics::ContactEvent event;
        event.body_a = lookup(pair.GetBody1ID());
        event.body_b = lookup(pair.GetBody2ID());
        event.phase = rr::physics::ContactPhase::Ended;

        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(event);
    }

    std::vector<rr::physics::ContactEvent> consume_events() {
        std::vector<rr::physics::ContactEvent> result;
        std::lock_guard<std::mutex> lock(mutex_);
        result.swap(events_);
        return result;
    }

private:
    rr::physics::BodyId lookup(const JPH::BodyID& body) const {
        const auto iterator = body_ids_.find(body.GetIndexAndSequenceNumber());
        return iterator == body_ids_.end() ? rr::physics::kInvalidBodyId : iterator->second;
    }

    rr::physics::BodyId game_id(const JPH::Body& body) const {
        const auto from_user_data = static_cast<rr::physics::BodyId>(body.GetUserData());
        return from_user_data != rr::physics::kInvalidBodyId
                   ? from_user_data
                   : lookup(body.GetID());
    }

    void record(const JPH::Body& body1,
                const JPH::Body& body2,
                const JPH::ContactManifold& manifold,
                const JPH::ContactSettings& settings,
                rr::physics::ContactPhase phase) {
        rr::physics::ContactEvent event;
        event.body_a = game_id(body1);
        event.body_b = game_id(body2);
        event.phase = phase;
        event.is_sensor = settings.mIsSensor;
        event.normal = from_jolt(manifold.mWorldSpaceNormal);

        if (manifold.mRelativeContactPointsOn1.size() > 0) {
            event.point = from_jolt(manifold.GetWorldSpaceContactPointOn1(0));
        }

        // Jolt does not expose the solved impulse in the contact callback; the
        // impulse remains zero here and is not used by the Stage B smoke test.
        event.normal_impulse = 0.0f;

        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(event);
    }

    mutable std::unordered_map<std::uint32_t, rr::physics::BodyId> body_ids_;
    mutable std::mutex mutex_;
    std::vector<rr::physics::ContactEvent> events_;
};

} // namespace

namespace rr::physics {
namespace {

struct VehicleRecord {
    VehicleId id{kInvalidVehicleId};
    JPH::BodyID chassis_body{};
    JPH::Ref<JPH::VehicleConstraint> constraint;
    JPH::Ref<JPH::VehicleCollisionTester> collision_tester;
    VehicleInput input{};
};

class JoltPhysicsWorld final : public PhysicsWorld {
public:
    explicit JoltPhysicsWorld(const WorldConfig& config)
        : config_(config),
          temp_allocator_(16u * 1024u * 1024u),
          job_system_(static_cast<JPH::uint>(JPH::cMaxPhysicsJobs),
                      static_cast<JPH::uint>(JPH::cMaxPhysicsBarriers),
                      worker_thread_count()),
          collision_tester_(new JPH::VehicleCollisionTesterRay(Layers::kMoving)) {
        if (!finite(config_.gravity)) {
            throw std::invalid_argument("JoltPhysicsWorld: gravity must be finite");
        }
        if (config_.max_bodies < 16) {
            throw std::invalid_argument("JoltPhysicsWorld: max_bodies must be at least 16");
        }
        if (config_.max_vehicles == 0 || config_.max_vehicles > kMaxVehicles) {
            throw std::invalid_argument("JoltPhysicsWorld: max_vehicles is outside 1..8");
        }

        physics_system_.SetGravity(to_jolt(config_.gravity));
        physics_system_.SetContactListener(&contact_listener_);

        const JPH::uint max_bodies = config_.max_bodies;
        physics_system_.Init(max_bodies,
                             0,
                             std::max<JPH::uint>(max_bodies * 2u, 1024u),
                             std::max<JPH::uint>(max_bodies * 2u, 1024u),
                             broad_phase_layer_interface_,
                             object_vs_broad_phase_filter_,
                             object_layer_pair_filter_);
    }

    ~JoltPhysicsWorld() override {
        for (const auto& vehicle : vehicles_) {
            remove_vehicle_resources(*vehicle);
        }
        vehicles_.clear();
        physics_system_.SetContactListener(nullptr);
    }

    BodyId create_ground(const GroundConfig& config) override {
        if (!finite(config.position) || !finite(config.half_extent) ||
            config.half_extent.x <= 0.0f || config.half_extent.y <= 0.0f ||
            config.half_extent.z <= 0.0f || !std::isfinite(config.friction) ||
            config.friction < 0.0f) {
            throw std::invalid_argument("JoltPhysicsWorld::create_ground: invalid config");
        }

        const BodyId body_id = next_body_id_++;
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape(to_jolt(config.half_extent)),
            to_jolt_position(config.position),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            Layers::kNonMoving);
        settings.mFriction = config.friction;
        settings.mRestitution = 0.0f;
        settings.mUserData = body_id;

        const JPH::BodyID jolt_body =
            physics_system_.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        if (jolt_body.IsInvalid()) {
            throw std::runtime_error("JoltPhysicsWorld::create_ground: body creation failed");
        }
        contact_listener_.remember_body(jolt_body, body_id);
        return body_id;
    }

    VehicleId create_vehicle(const VehicleConfig& config) override {
        if (!valid_vehicle_config(config)) {
            throw std::invalid_argument("JoltPhysicsWorld::create_vehicle: invalid config");
        }
        if (vehicles_.size() >= config_.max_vehicles) {
            throw std::runtime_error("JoltPhysicsWorld::create_vehicle: vehicle limit reached");
        }

        const VehicleId vehicle_id = next_vehicle_id_++;
        const BodyId body_id = next_body_id_++;

        JPH::RefConst<JPH::Shape> chassis_box = new JPH::BoxShape(to_jolt(config.chassis_half_extent));
        JPH::RefConst<JPH::Shape> chassis_shape =
            new JPH::OffsetCenterOfMassShape(chassis_box, to_jolt(config.center_of_mass));

        JPH::BodyCreationSettings body_settings(
            chassis_shape,
            to_jolt_position(config.initial_position),
            to_jolt(config.initial_rotation),
            JPH::EMotionType::Dynamic,
            Layers::kMoving);
        body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        body_settings.mMassPropertiesOverride.mMass = config.chassis_mass;
        body_settings.mFriction = 0.5f;
        body_settings.mRestitution = 0.0f;
        body_settings.mAllowSleeping = false;
        body_settings.mUserData = body_id;

        JPH::Body* chassis = physics_system_.GetBodyInterface().CreateBody(body_settings);
        if (chassis == nullptr) {
            throw std::runtime_error("JoltPhysicsWorld::create_vehicle: chassis creation failed");
        }
        contact_listener_.remember_body(chassis->GetID(), body_id);
        physics_system_.GetBodyInterface().AddBody(chassis->GetID(), JPH::EActivation::Activate);

        JPH::VehicleConstraintSettings vehicle_settings;
        vehicle_settings.mUp = JPH::Vec3(0.0f, 1.0f, 0.0f);
        vehicle_settings.mForward = JPH::Vec3(0.0f, 0.0f, 1.0f);
        vehicle_settings.mMaxPitchRollAngle = JPH::DegreesToRadians(70.0f);

        for (std::size_t index = 0; index < config.wheels.size(); ++index) {
            const WheelConfig& wheel_config = config.wheels[index];
            auto* wheel = new JPH::WheelSettingsWV();
            wheel->mPosition = to_jolt(wheel_config.position);
            wheel->mSuspensionDirection = JPH::Vec3(0.0f, -1.0f, 0.0f);
            wheel->mSteeringAxis = JPH::Vec3(0.0f, 1.0f, 0.0f);
            wheel->mWheelUp = JPH::Vec3(0.0f, 1.0f, 0.0f);
            wheel->mWheelForward = JPH::Vec3(0.0f, 0.0f, 1.0f);
            wheel->mSuspensionMinLength = wheel_config.suspension_min_length;
            wheel->mSuspensionMaxLength = wheel_config.suspension_max_length;
            wheel->mSuspensionSpring.mFrequency = wheel_config.suspension_frequency;
            wheel->mSuspensionSpring.mDamping = wheel_config.suspension_damping;
            wheel->mRadius = wheel_config.radius;
            wheel->mWidth = wheel_config.width;
            wheel->mMaxSteerAngle = wheel_config.steerable ? config.max_steer_angle : 0.0f;
            wheel->mMaxBrakeTorque = config.max_brake_torque;
            wheel->mMaxHandBrakeTorque = index >= 2 ? config.max_handbrake_torque : 0.0f;
            vehicle_settings.mWheels.push_back(wheel);
        }

        auto* controller = new JPH::WheeledVehicleControllerSettings();
        controller->mEngine.mMaxTorque = config.max_engine_torque;
        controller->mEngine.mMinRPM = config.engine_min_rpm;
        controller->mEngine.mMaxRPM = config.engine_max_rpm;
        controller->mDifferentials.resize(2);
        controller->mDifferentials[0].mLeftWheel = 0;
        controller->mDifferentials[0].mRightWheel = 1;
        controller->mDifferentials[0].mEngineTorqueRatio = 0.5f;
        controller->mDifferentials[1].mLeftWheel = 2;
        controller->mDifferentials[1].mRightWheel = 3;
        controller->mDifferentials[1].mEngineTorqueRatio = 0.5f;
        vehicle_settings.mController = controller;

        JPH::Ref<JPH::VehicleConstraint> constraint =
            new JPH::VehicleConstraint(*chassis, vehicle_settings);
        constraint->SetVehicleCollisionTester(collision_tester_);
        physics_system_.AddConstraint(constraint);
        physics_system_.AddStepListener(constraint);

        auto record = std::make_unique<VehicleRecord>();
        record->id = vehicle_id;
        record->chassis_body = chassis->GetID();
        record->constraint = constraint;
        record->collision_tester = collision_tester_;
        record->input = VehicleInput{};
        vehicles_.push_back(std::move(record));

        return vehicle_id;
    }

    bool destroy_vehicle(VehicleId vehicle) override {
        const auto iterator = std::find_if(
            vehicles_.begin(), vehicles_.end(),
            [vehicle](const std::unique_ptr<VehicleRecord>& record) {
                return record->id == vehicle;
            });
        if (iterator == vehicles_.end()) {
            return false;
        }

        remove_vehicle_resources(**iterator);
        vehicles_.erase(iterator);
        return true;
    }

    bool set_vehicle_input(VehicleId vehicle, const VehicleInput& input) override {
        VehicleRecord* record = find_vehicle(vehicle);
        if (record == nullptr || !valid_input(input)) {
            return false;
        }

        record->input = input;
        auto* controller = static_cast<JPH::WheeledVehicleController*>(
            record->constraint->GetController());
        controller->SetDriverInput(input.throttle, input.steering, input.brake, input.handbrake);
        return true;
    }

    void step(double fixed_dt) override {
        if (!std::isfinite(fixed_dt) || fixed_dt <= 0.0) {
            throw std::invalid_argument("JoltPhysicsWorld::step: fixed_dt must be finite and positive");
        }

        const JPH::EPhysicsUpdateError error = physics_system_.Update(
            static_cast<float>(fixed_dt), 1, &temp_allocator_, &job_system_);
        if (error != JPH::EPhysicsUpdateError::None) {
            throw std::runtime_error("JoltPhysicsWorld::step: physics update reported an error");
        }
        ++tick_;
    }

    std::optional<VehicleState> vehicle_state(VehicleId vehicle) const override {
        const VehicleRecord* record = find_vehicle(vehicle);
        if (record == nullptr) {
            return std::nullopt;
        }

        const JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();
        const JPH::RVec3 position = body_interface.GetPosition(record->chassis_body);
        const JPH::Quat rotation = body_interface.GetRotation(record->chassis_body);
        const JPH::Vec3 linear_velocity = body_interface.GetLinearVelocity(record->chassis_body);
        const JPH::Vec3 angular_velocity = body_interface.GetAngularVelocity(record->chassis_body);

        VehicleState state;
        state.position = from_jolt_position(position);
        state.rotation = from_jolt(rotation);
        state.linear_velocity = from_jolt(linear_velocity);
        state.angular_velocity = from_jolt(angular_velocity);

        const JPH::Vec3 local_velocity = rotation.Conjugated() * linear_velocity;
        state.forward_speed = local_velocity.GetZ();

        const auto* controller = static_cast<const JPH::WheeledVehicleController*>(
            record->constraint->GetController());
        state.engine_rpm = controller->GetEngine().GetCurrentRPM();

        for (std::uint32_t index = 0; index < kWheelCount; ++index) {
            const JPH::Wheel* wheel = record->constraint->GetWheel(index);
            state.wheel_rotation_speed[index] = wheel->GetAngularVelocity();
            state.suspension_length[index] = wheel->GetSuspensionLength();
            state.wheel_grounded[index] = wheel->HasContact();
            state.grounded = state.grounded || state.wheel_grounded[index];
        }

        return state;
    }

    std::vector<ContactEvent> consume_contact_events() override {
        return contact_listener_.consume_events();
    }

    std::optional<RaycastHit> raycast(const RaycastRequest& request) const override {
        if (!finite(request.origin) || !finite(request.direction) ||
            !std::isfinite(request.max_distance) || request.max_distance <= 0.0f) {
            return std::nullopt;
        }

        JPH::Vec3 direction = to_jolt(request.direction);
        const float direction_length = direction.Length();
        if (!std::isfinite(direction_length) || direction_length <= 1.0e-6f) {
            return std::nullopt;
        }
        direction /= direction_length;

        const JPH::RRayCast ray(to_jolt_position(request.origin),
                                direction * request.max_distance);
        JPH::RayCastResult hit;
        if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit)) {
            return std::nullopt;
        }

        const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);
        RaycastHit result;
        result.point = from_jolt_position(point);
        result.distance = hit.mFraction * request.max_distance;

        JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            result.body = static_cast<BodyId>(body.GetUserData());
            result.normal = from_jolt(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point));
        }
        return result;
    }

    bool capture_snapshot(WorldSnapshot& out) const override {
        if (vehicles_.size() > kMaxVehicles) {
            return false;
        }

        out = WorldSnapshot{};
        out.tick = tick_;
        out.vehicle_count = static_cast<std::uint32_t>(vehicles_.size());

        for (std::size_t index = 0; index < vehicles_.size(); ++index) {
            const VehicleRecord& record = *vehicles_[index];
            const auto state = vehicle_state(record.id);
            if (!state.has_value()) {
                return false;
            }
            out.vehicles[index].vehicle_id = record.id;
            out.vehicles[index].input = record.input;
            out.vehicles[index].state = *state;
        }
        return true;
    }

    bool restore_snapshot(const WorldSnapshot& snapshot) override {
        if (snapshot.vehicle_count > kMaxVehicles ||
            snapshot.vehicle_count > vehicles_.size()) {
            return false;
        }

        for (std::uint32_t index = 0; index < snapshot.vehicle_count; ++index) {
            const VehicleSnapshot& saved = snapshot.vehicles[index];
            VehicleRecord* record = find_vehicle(saved.vehicle_id);
            if (record == nullptr) {
                return false;
            }

            JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();
            body_interface.SetPositionAndRotation(
                record->chassis_body,
                to_jolt_position(saved.state.position),
                to_jolt(saved.state.rotation),
                JPH::EActivation::Activate);
            body_interface.SetLinearVelocity(record->chassis_body,
                                             to_jolt(saved.state.linear_velocity));
            body_interface.SetAngularVelocity(record->chassis_body,
                                              to_jolt(saved.state.angular_velocity));

            for (std::uint32_t wheel_index = 0; wheel_index < kWheelCount; ++wheel_index) {
                record->constraint->GetWheel(wheel_index)
                    ->SetAngularVelocity(saved.state.wheel_rotation_speed[wheel_index]);
            }

            record->input = saved.input;
            auto* controller = static_cast<JPH::WheeledVehicleController*>(
                record->constraint->GetController());
            controller->SetDriverInput(saved.input.throttle,
                                       saved.input.steering,
                                       saved.input.brake,
                                       saved.input.handbrake);
        }

        tick_ = snapshot.tick;
        return true;
    }

    std::uint32_t body_count() const override {
        return static_cast<std::uint32_t>(physics_system_.GetNumBodies());
    }

private:
    VehicleRecord* find_vehicle(VehicleId id) {
        for (const auto& record : vehicles_) {
            if (record->id == id) {
                return record.get();
            }
        }
        return nullptr;
    }

    const VehicleRecord* find_vehicle(VehicleId id) const {
        for (const auto& record : vehicles_) {
            if (record->id == id) {
                return record.get();
            }
        }
        return nullptr;
    }

    void remove_vehicle_resources(VehicleRecord& record) {
        if (record.constraint != nullptr) {
            physics_system_.RemoveStepListener(record.constraint);
            physics_system_.RemoveConstraint(record.constraint);
            record.constraint = nullptr;
        }
        if (!record.chassis_body.IsInvalid()) {
            JPH::BodyInterface& body_interface = physics_system_.GetBodyInterface();
            body_interface.RemoveBody(record.chassis_body);
            body_interface.DestroyBody(record.chassis_body);
            record.chassis_body = JPH::BodyID();
        }
    }

    WorldConfig config_;
    JoltRuntimeGuard runtime_guard_;
    JPH::TempAllocatorImpl temp_allocator_;
    JPH::JobSystemThreadPool job_system_;
    BPLayerInterfaceImpl broad_phase_layer_interface_;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_filter_;
    ObjectLayerPairFilterImpl object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    JoltContactListener contact_listener_;
    JPH::Ref<JPH::VehicleCollisionTester> collision_tester_;
    std::vector<std::unique_ptr<VehicleRecord>> vehicles_;
    std::uint32_t next_body_id_{1};
    std::uint32_t next_vehicle_id_{1};
    std::uint64_t tick_{0};
};

} // namespace

std::unique_ptr<PhysicsWorld> create_physics_world(const WorldConfig& config) {
    return std::make_unique<JoltPhysicsWorld>(config);
}

} // namespace rr::physics
