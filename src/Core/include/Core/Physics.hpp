#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <PxPhysicsAPI.h>
#include <cooking/PxCooking.h>
#include <vehicle2/PxVehicleAPI.h>
#include <vehicle2/commands/PxVehicleCommandStates.h>
#include <vehicle2/wheel/PxVehicleWheelParams.h>
#include <vehicle2/wheel/PxVehicleWheelStates.h>
#include <vehicle2/wheel/PxVehicleWheelComponents.h>
#include <vehicle2/suspension/PxVehicleSuspensionParams.h>
#include <vehicle2/suspension/PxVehicleSuspensionComponents.h>
#include <vehicle2/suspension/PxVehicleSuspensionStates.h>
#include <vehicle2/tire/PxVehicleTireParams.h>
#include <vehicle2/tire/PxVehicleTireStates.h>
#include <vehicle2/tire/PxVehicleTireComponents.h>

namespace Core {

class CustomEngineDrivetrainComponent;

class Physics {
public:
    struct RigidBody {
        physx::PxRigidDynamic* body;
        physx::PxShape* shape;
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 velocity;
        glm::vec3 angularVelocity;
        float mass;
        bool isStatic;

        RigidBody() : body(nullptr), shape(nullptr), mass(1.0f), isStatic(false) {}
    };

    struct CollisionInfo {
        RigidBody* bodyA;
        RigidBody* bodyB;
        glm::vec3 contactPoint;
        glm::vec3 contactNormal;
        float impulse;
    };

    struct VehiclePhysics {
        physx::vehicle2::PxVehicleSuspensionComponent* suspensionComponent;
        std::vector<physx::vehicle2::PxVehicleSuspensionForceParams> suspensionForceParams;
        std::vector<physx::vehicle2::PxVehicleRoadGeometryState> roadGeometryStates;
        physx::vehicle2::PxVehicleRigidBodyState rigidBodyState;
        physx::vehicle2::PxVehicleComponentSequence* sequence;
        physx::vehicle2::PxVehicleCommandState* steerCommandState;
        // --- Параметры ---
        std::vector<physx::vehicle2::PxVehicleWheelParams> wheelParams;
        std::vector<physx::vehicle2::PxVehicleTireForceParams> tireParams;
        std::vector<physx::vehicle2::PxVehicleSuspensionParams> suspensionParams;
        std::vector<physx::vehicle2::PxVehicleSteerCommandResponseParams> steerResponseParams;
        // --- Компоненты (только те, что действительно существуют) ---
        physx::vehicle2::PxVehicleRigidBodyComponent*   rigidBodyComponent;
        physx::vehicle2::PxVehicleWheelComponent*      wheelComponent;
        physx::vehicle2::PxVehicleTireComponent*       tireComponent;
        CustomEngineDrivetrainComponent* drivetrainComponent;
        // --- Состояния ---
        std::vector<physx::PxReal> steerResponseStates;
        // --- Compliance (toe/camber/force-app-point) ---
        std::vector<physx::vehicle2::PxVehicleSuspensionComplianceParams>   complianceParams;
        std::vector<physx::vehicle2::PxVehicleSuspensionComplianceState>   complianceStates;
        // --- Двигатель ---
        physx::vehicle2::PxVehicleEngineParams* engineParams;
        physx::vehicle2::PxVehicleClutchParams* clutchParams;
        physx::vehicle2::PxVehicleGearboxParams* gearboxParams;

        // --- Описание осей ---
        physx::vehicle2::PxVehicleAxleDescription* axleDescription;

        // --- Другие состояния (массивы) ---
        std::vector<physx::vehicle2::PxVehicleWheelActuationState> actuationStates;
        std::vector<physx::vehicle2::PxVehicleWheelRigidBody1dState> wheelRigidBody1dStates;

        physx::vehicle2::PxVehicleDifferentialState* differentialState;
        physx::vehicle2::PxVehicleEngineState* engineState;
        physx::vehicle2::PxVehicleGearboxState* gearboxState;
        physx::vehicle2::PxVehicleClutchSlipState* clutchState;
        physx::vehicle2::PxVehicleEngineDriveThrottleCommandResponseState* throttleResponseState;
        physx::vehicle2::PxVehicleClutchCommandResponseState* clutchResponseState;
        physx::vehicle2::PxVehicleWheelConstraintGroupState* constraintGroupState;
        std::vector<physx::vehicle2::PxVehicleTireForce> tireForces;
        std::vector<physx::PxReal> brakeResponseStates;
        std::vector<physx::vehicle2::PxVehicleSuspensionState> suspensionStates;
        std::vector<physx::vehicle2::PxVehicleSuspensionComplianceState> suspensionComplianceStates;
        std::vector<physx::vehicle2::PxVehicleSuspensionForce> suspensionForces;
        RigidBody* chassis;
        std::vector<RigidBody*> wheels;
        glm::vec3 engineForce;
        glm::vec3 brakeForce;
        glm::vec3 steeringAngle;
        float maxEngineForce;
        float maxBrakeForce;
        float maxSteeringAngle;
        float wheelRadius;
        float wheelHalfWidth;
        float wheelMass;
        float wheelDampingRate;
        float suspensionSpringStrength;
        float suspensionSpringDamperRate;
        float suspensionMaxCompression;
        float suspensionMaxDroop;
        float longitudinalFriction;
        float rollInfluence;

        VehiclePhysics()
                : sequence(nullptr),
                steerCommandState(nullptr),
                rigidBodyComponent(nullptr),
                wheelComponent(nullptr),
                suspensionComponent(nullptr),
                tireComponent(nullptr),
                drivetrainComponent(nullptr),
                engineParams(nullptr),
                clutchParams(nullptr),
                gearboxParams(nullptr),
                axleDescription(nullptr),
                differentialState(nullptr),
                engineState(nullptr),
                gearboxState(nullptr),
                clutchState(nullptr),
                throttleResponseState(nullptr),
                clutchResponseState(nullptr),
                constraintGroupState(nullptr),
                chassis(nullptr)
            {
                wheelParams.resize(4);
                tireParams.resize(4);
                suspensionParams.resize(4);
                suspensionForceParams.resize(4);
                complianceParams.resize(4);
                complianceStates.resize(4);
                suspensionStates.resize(4);
                suspensionForces.resize(4);
                roadGeometryStates.resize(4);
                steerResponseParams.resize(4);
                steerResponseStates.resize(4, 0.0f);
                actuationStates.resize(4);
                wheelRigidBody1dStates.resize(4);
                tireForces.resize(4);
                brakeResponseStates.resize(4);
                wheels.resize(4);
            }
    };

    Physics();
    ~Physics();

    bool initialize();
    void shutdown();
    void step(float deltaTime);
    void setGravity(const glm::vec3& gravity);

    RigidBody* createRigidBody(const glm::vec3& position, const glm::quat& rotation, float mass, physx::PxShape* shape);
    void destroyRigidBody(RigidBody* body);

    physx::PxShape* createSphereShape(float radius);
    physx::PxShape* createCylinderShape(const glm::vec3& halfExtents);
    physx::PxShape* createCapsuleShape(float radius, float height);
    physx::PxShape* createMeshShape(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices);
    physx::PxShape* createConvexHullShape(const std::vector<glm::vec3>& vertices);

    VehiclePhysics* createVehicle(const glm::vec3& position, const glm::quat& rotation);
    void destroyVehicle(VehiclePhysics* vehicle);
    void updateVehicle(VehiclePhysics* vehicle, float deltaTime);
    void applyEngineForce(VehiclePhysics* vehicle, float force);
    void applyBrakeForce(VehiclePhysics* vehicle, float force);
    void setSteeringAngle(VehiclePhysics* vehicle, float angle);

    void setCollisionCallback(std::function<void(const CollisionInfo&)> callback);
    bool raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal, RigidBody*& hitBody);

    void enableDestruction(bool enable) { m_destructionEnabled = enable; }
    bool isDestructionEnabled() const { return m_destructionEnabled; }

    struct RespawnInfo {
        glm::vec3 position;
        glm::quat rotation;
        float antiCampTime;
        bool isRespawning;
    };

    void setRespawnPosition(RigidBody* body, const glm::vec3& position, const glm::quat& rotation);
    void respawnBody(RigidBody* body);
    bool isInAntiCampMode(RigidBody* body) const;
    void updateRespawnSystem(float deltaTime);

    void* getWorld() const { return m_scene; }
    const glm::vec3& getGravity() const { return m_gravity; }

    physx::PxTransform glmToPx(const glm::vec3& pos, const glm::quat& rot) const;
    glm::vec3 pxToGlm(const physx::PxVec3& vec) const;
    glm::quat pxToGlm(const physx::PxQuat& quat) const;

private:
    physx::PxShape* createBoxShape(const glm::vec3& halfExtents);
    void cleanupVehicleCreation(VehiclePhysics* vehicle, int wheelsCreated);
    physx::PxFoundation* m_foundation;
    physx::PxPhysics* m_physics;
    physx::PxScene* m_scene;
    physx::PxPvd* m_pvd;
    physx::PxCookingParams m_cookingParams;
    glm::vec3 m_gravity;
    float m_fixedTimeStep;
    physx::PxDefaultAllocator m_allocator;
    physx::PxDefaultErrorCallback m_errorCallback;
    physx::PxShape* createCylinderShape(float radius, float halfWidth);
    bool m_destructionEnabled;
    std::unordered_map<RigidBody*, RespawnInfo> m_respawnInfo;
    float m_antiCampDuration;
    std::function<void(const CollisionInfo&)> m_collisionCallback;

    void processCollisions();
};

class CustomEngineDrivetrainComponent : public physx::vehicle2::PxVehicleEngineDrivetrainComponent {
public:
    CustomEngineDrivetrainComponent() : PxVehicleEngineDrivetrainComponent() {}
    virtual ~CustomEngineDrivetrainComponent() {}

    void setData(
        const physx::vehicle2::PxVehicleAxleDescription* axleDesc,
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelParams> wheelParamsData,
        const physx::vehicle2::PxVehicleEngineParams* engineParamsData,
        const physx::vehicle2::PxVehicleClutchParams* clutchParamsData,
        const physx::vehicle2::PxVehicleGearboxParams* gearboxParamsData,
        physx::vehicle2::PxVehicleArrayData<const physx::PxReal> brakeResponseData,
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelActuationState> actuationStateData,
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleTireForce> tireForceData,
        const physx::vehicle2::PxVehicleEngineDriveThrottleCommandResponseState* throttleResponseData,
        const physx::vehicle2::PxVehicleClutchCommandResponseState* clutchResponseData,
        const physx::vehicle2::PxVehicleDifferentialState* differentialStateData,
        const physx::vehicle2::PxVehicleWheelConstraintGroupState* constraintGroupData,
        physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleWheelRigidBody1dState> wheelRigidBodyData,
        physx::vehicle2::PxVehicleEngineState* engineStateData,
        physx::vehicle2::PxVehicleGearboxState* gearboxStateData,
        physx::vehicle2::PxVehicleClutchSlipState* clutchSlipData) {
        axleDescription = axleDesc;
        wheelParams = wheelParamsData;
        engineParams = engineParamsData;
        clutchParams = clutchParamsData;
        gearboxParams = gearboxParamsData;
        brakeResponseStates = brakeResponseData;
        actuationStates = actuationStateData;
        tireForces = tireForceData;
        throttleResponseState = throttleResponseData;
        clutchResponseState = clutchResponseData;
        differentialState = differentialStateData;
        constraintGroupState = constraintGroupData;
        wheelRigidBody1dStates = wheelRigidBodyData;
        engineState = engineStateData;
        gearboxState = gearboxStateData;
        clutchState = clutchSlipData;
    }

    virtual void getDataForEngineDrivetrainComponent(
        const physx::vehicle2::PxVehicleAxleDescription*& axleDescription,
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelParams>& wheelParams,
        const physx::vehicle2::PxVehicleEngineParams*& engineParams,
        const physx::vehicle2::PxVehicleClutchParams*& clutchParams,
        const physx::vehicle2::PxVehicleGearboxParams*& gearboxParams,
        physx::vehicle2::PxVehicleArrayData<const physx::PxReal>& brakeResponseStates,
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelActuationState>& actuationStates,
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleTireForce>& tireForces,
        const physx::vehicle2::PxVehicleEngineDriveThrottleCommandResponseState*& throttleResponseState,
        const physx::vehicle2::PxVehicleClutchCommandResponseState*& clutchResponseState,
        const physx::vehicle2::PxVehicleDifferentialState*& differentialState,
        const physx::vehicle2::PxVehicleWheelConstraintGroupState*& constraintGroupState,
        physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleWheelRigidBody1dState>& wheelRigidBody1dStates,
        physx::vehicle2::PxVehicleEngineState*& engineState,
        physx::vehicle2::PxVehicleGearboxState*& gearboxState,
        physx::vehicle2::PxVehicleClutchSlipState*& clutchState) override {
        axleDescription = this->axleDescription;
        wheelParams = this->wheelParams;
        engineParams = this->engineParams;
        clutchParams = this->clutchParams;
        gearboxParams = this->gearboxParams;
        brakeResponseStates = this->brakeResponseStates;
        actuationStates = this->actuationStates;
        tireForces = this->tireForces;
        throttleResponseState = this->throttleResponseState;
        clutchResponseState = this->clutchResponseState;
        differentialState = this->differentialState;
        constraintGroupState = this->constraintGroupState;
        wheelRigidBody1dStates = this->wheelRigidBody1dStates;
        engineState = this->engineState;
        gearboxState = this->gearboxState;
        clutchState = this->clutchState;
    }

private:
    const physx::vehicle2::PxVehicleAxleDescription* axleDescription;
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelParams> wheelParams;
    const physx::vehicle2::PxVehicleEngineParams* engineParams;
    const physx::vehicle2::PxVehicleClutchParams* clutchParams;
    const physx::vehicle2::PxVehicleGearboxParams* gearboxParams;
    physx::vehicle2::PxVehicleArrayData<const physx::PxReal> brakeResponseStates;
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelActuationState> actuationStates;
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleTireForce> tireForces;
    const physx::vehicle2::PxVehicleEngineDriveThrottleCommandResponseState* throttleResponseState;
    const physx::vehicle2::PxVehicleClutchCommandResponseState* clutchResponseState;
    const physx::vehicle2::PxVehicleDifferentialState* differentialState;
    const physx::vehicle2::PxVehicleWheelConstraintGroupState* constraintGroupState;
    physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleWheelRigidBody1dState> wheelRigidBody1dStates;
    physx::vehicle2::PxVehicleEngineState* engineState;
    physx::vehicle2::PxVehicleGearboxState* gearboxState;
    physx::vehicle2::PxVehicleClutchSlipState* clutchState;
};

} // namespace Core