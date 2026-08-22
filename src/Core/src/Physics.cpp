#include "Core/Physics.hpp"
#include "Core/Logging.hpp"
#include <PxPhysicsAPI.h>
#include <cooking/PxCooking.h>
#include <foundation/PxSimpleTypes.h>
#include <foundation/PxVec3.h>
#include <foundation/PxMemory.h>
#include <vehicle2/PxVehicleAPI.h>
#include <vehicle2/PxVehicleComponentSequence.h>
#include <vehicle2/commands/PxVehicleCommandStates.h>
#include <vehicle2/rigidBody/PxVehicleRigidBodyComponents.h>
#include <vehicle2/steering/PxVehicleSteeringFunctions.h>
#include <vehicle2/suspension/PxVehicleSuspensionParams.h>
#include <vehicle2/suspension/PxVehicleSuspensionComponents.h>
#include <vehicle2/tire/PxVehicleTireParams.h>
#include <vehicle2/tire/PxVehicleTireComponents.h>
#include <vehicle2/wheel/PxVehicleWheelComponents.h>
#include <vehicle2/wheel/PxVehicleWheelParams.h>
#include <vehicle2/wheel/PxVehicleWheelStates.h>

namespace Core {

Physics::Physics()
    : m_foundation(nullptr), m_physics(nullptr), m_scene(nullptr), m_pvd(nullptr),
      m_gravity(0.0f, -9.81f, 0.0f), m_fixedTimeStep(1.0f / 60.0f), m_antiCampDuration(3.0f),
      m_destructionEnabled(true),
      m_cookingParams(physx::PxTolerancesScale()) {
}

Physics::~Physics() {
    shutdown();
}

bool Physics::initialize() {
    LOG_INFO("Инициализация PhysX для Rusty Rush");

    m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback);
    if (!m_foundation) {
        LOG_ERROR("Не удалось создать фундамент PhysX");
        return false;
    }

    m_pvd = PxCreatePvd(*m_foundation);
    m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale());
    if (!m_physics) {
        LOG_ERROR("Не удалось создать физику PhysX");
        return false;
    }

    m_cookingParams = physx::PxCookingParams(m_physics->getTolerancesScale());
    m_cookingParams.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
    m_cookingParams.meshWeldTolerance = 0.01f;
    LOG_INFO("PxCookingParams успешно инициализированы");

    physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(m_gravity.x, m_gravity.y, m_gravity.z);
    sceneDesc.cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(2);
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    m_scene = m_physics->createScene(sceneDesc);
    if (!m_scene) {
        LOG_ERROR("Не удалось создать сцену PhysX");
        return false;
    }

    LOG_INFO("PhysX успешно инициализирован");
    return true;
}

void Physics::shutdown() {
    LOG_INFO("Завершение работы PhysX");
    if (m_scene) m_scene->release();
    if (m_physics) m_physics->release();
    if (m_pvd) m_pvd->release();
    if (m_foundation) m_foundation->release();
    m_respawnInfo.clear();
}

void Physics::step(float deltaTime) {
    updateRespawnSystem(deltaTime);
    m_scene->simulate(deltaTime, nullptr, nullptr, 0);
    m_scene->fetchResults(true);
}

void Physics::setGravity(const glm::vec3& gravity) {
    m_gravity = gravity;
    if (m_scene) m_scene->setGravity(physx::PxVec3(gravity.x, gravity.y, gravity.z));
}

Physics::RigidBody* Physics::createRigidBody(const glm::vec3& position, const glm::quat& rotation, float mass, physx::PxShape* shape) {
    auto* rigidBody = new RigidBody();
    rigidBody->mass = mass;
    rigidBody->position = position;
    rigidBody->rotation = rotation;

    physx::PxRigidDynamic* body = m_physics->createRigidDynamic(glmToPx(position, rotation));
    body->attachShape(*shape);
    physx::PxRigidBodyExt::updateMassAndInertia(*body, mass);
    m_scene->addActor(*body);

    // PR1 fix: was missing — raycast() reads userData to identify the hit body.
    body->userData = rigidBody;

    rigidBody->body = body;
    rigidBody->shape = shape;
    return rigidBody;
}

void Physics::destroyRigidBody(RigidBody* body) {
    if (!body || !body->body) return;
    m_scene->removeActor(*static_cast<physx::PxRigidDynamic*>(body->body));
    static_cast<physx::PxRigidDynamic*>(body->body)->release();
    static_cast<physx::PxShape*>(body->shape)->release();
    delete body;
}

physx::PxShape* Physics::createBoxShape(const glm::vec3& halfExtents)
{
    physx::PxBoxGeometry geo(halfExtents.x, halfExtents.y, halfExtents.z);
    physx::PxMaterial* mat = m_physics->createMaterial(0.5f, 0.5f, 0.1f);
    physx::PxShape* shape = m_physics->createShape(geo, *mat, true);
    mat->release();
    return shape;
}

physx::PxShape* Physics::createSphereShape(float radius) {
    return m_physics->createShape(physx::PxSphereGeometry(radius), *m_physics->createMaterial(0.5f, 0.5f, 0.1f));
}

physx::PxShape* Physics::createCylinderShape(float radius, float halfWidth)
{
    const int N = 16;
    physx::PxVec3 verts[2 * N];
    for (int i = 0; i < N; ++i) {
        const float a = 2.0f * physx::PxPi * i / N;
        const float x = radius * physx::PxCos(a);
        const float z = radius * physx::PxSin(a);
        verts[i]      = physx::PxVec3(x, -halfWidth, z);
        verts[i + N]  = physx::PxVec3(x, +halfWidth, z);
    }

    physx::PxConvexMeshDesc desc;
    desc.points.count  = 2 * N;
    desc.points.stride = sizeof(physx::PxVec3);
    desc.points.data   = verts;
    desc.flags         = physx::PxConvexFlag::eCOMPUTE_CONVEX;

    physx::PxDefaultMemoryOutputStream buf;
    physx::PxConvexMeshCookingResult::Enum res;
    if (!PxCookConvexMesh(m_cookingParams, desc, buf, &res)) {
        LOG_ERROR("PxCookConvexMesh failed");
        return nullptr;
    }

    physx::PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
    physx::PxConvexMesh* mesh = m_physics->createConvexMesh(input);
    physx::PxConvexMeshGeometry geo(mesh);
    physx::PxMaterial* mat = m_physics->createMaterial(0.5f, 0.5f, 0.1f);
    physx::PxShape* shape = m_physics->createShape(geo, *mat, true);
    mat->release();
    mesh->release();
    return shape;
}

void Physics::cleanupVehicleCreation(VehiclePhysics* vehicle, int wheelsCreated)
{
    if (vehicle->chassis) destroyRigidBody(vehicle->chassis);
    for (int i = 0; i < wheelsCreated; ++i) {
        if (vehicle->wheels[i]) destroyRigidBody(vehicle->wheels[i]);
    }
    delete vehicle;
}

Physics::VehiclePhysics* Physics::createVehicle(const glm::vec3& position, const glm::quat& rotation)
{
    LOG_INFO("Создание автомобиля в позиции (" +
             std::to_string(position.x) + ", " +
             std::to_string(position.y) + ", " +
             std::to_string(position.z) + ")");

    auto* vehicle = new VehiclePhysics();

    // -----------------------------------------------------------------
    // 1. Базовые параметры
    // -----------------------------------------------------------------
    const float wheelRadius = 0.5f;
    const float wheelHalfWidth = 0.2f;
    const float wheelMass = 20.0f;
    const float wheelDampingRate = 0.25f;
    const float suspensionTravel = 0.4f;
    const float suspensionStiffness = 35000.0f;
    const float suspensionDamping = 4500.0f;
    const float enginePeakTorque = 4000.0f;
    const float engineMaxOmega = 600.0f;
    const float maxBrakeTorque = 1000.0f;
    const float maxSteerAngle = 0.6f;

    // -----------------------------------------------------------------
    // 2. Шасси
    // -----------------------------------------------------------------
    physx::PxShape* chassisShape = createBoxShape({1.5f, 0.5f, 0.75f});
    vehicle->chassis = createRigidBody(position, rotation, 1000.0f, chassisShape);
    if (!vehicle->chassis) {
        LOG_ERROR("Не удалось создать шасси");
        delete vehicle;
        return nullptr;
    }

    // -----------------------------------------------------------------
    // 3. Колёса
    // -----------------------------------------------------------------
    vehicle->wheels.resize(4);
    vehicle->wheelParams.resize(4);
    const glm::vec3 wheelOffsets[4] = {
        { 1.0f, -0.5f, 1.0f},  // FL
        {-1.0f, -0.5f, 1.0f},  // FR
        { 1.0f, -0.5f, -1.0f}, // RL
        {-1.0f, -0.5f, -1.0f}  // RR
    };

    for (int i = 0; i < 4; ++i) {
        physx::PxShape* wheelShape = createCylinderShape(wheelRadius, wheelHalfWidth);
        vehicle->wheels[i] = createRigidBody(position + wheelOffsets[i], rotation, wheelMass, wheelShape);
        if (!vehicle->wheels[i]) {
            LOG_ERROR("Не удалось создать колесо " + std::to_string(i));
            cleanupVehicleCreation(vehicle, i);
            return nullptr;
        }

        auto& wp = vehicle->wheelParams[i];
        wp.radius = wheelRadius;
        wp.halfWidth = wheelHalfWidth;
        wp.mass = wheelMass;
        wp.moi = 0.5f * wheelMass * wheelRadius * wheelRadius;
        wp.dampingRate = wheelDampingRate;

        if (!wp.isValid()) {
            LOG_ERROR("Invalid wheelParams[" + std::to_string(i) + "]");
            cleanupVehicleCreation(vehicle, i);
            return nullptr;
        }
    }

    // -----------------------------------------------------------------
    // 4. Шины
    // -----------------------------------------------------------------
    vehicle->tireParams.resize(4);
    for (int i = 0; i < 4; ++i) {
        auto& tp = vehicle->tireParams[i];
        const float friction = 1.0f;
        tp.latStiffX = friction;
        tp.latStiffY = friction * 1000.0f;
        tp.longStiff = friction * 1000.0f;
        tp.camberStiff = 0.0f;
        tp.restLoad = wheelMass * 9.81f + 1000.0f * 9.81f / 4.0f;

        tp.frictionVsSlip[0][0] = 0.0f; tp.frictionVsSlip[0][1] = friction;
        tp.frictionVsSlip[1][0] = 0.1f; tp.frictionVsSlip[1][1] = friction * 0.9f;
        tp.frictionVsSlip[2][0] = 1.0f; tp.frictionVsSlip[2][1] = friction * 0.7f;

        tp.loadFilter[0][0] = 0.0f; tp.loadFilter[0][1] = 0.0f;
        tp.loadFilter[1][0] = 2.0f; tp.loadFilter[1][1] = 2.0f;

        if (!tp.isValid()) {
            LOG_ERROR("Invalid tireParams[" + std::to_string(i) + "]");
            cleanupVehicleCreation(vehicle, 4);
            return nullptr;
        }
    }

    // -----------------------------------------------------------------
    // 5. Подвеска — ПОЛНОСТЬЮ ИСПРАВЛЕННЫЙ БЛОК
    // -----------------------------------------------------------------
    vehicle->suspensionParams.resize(4);
    vehicle->suspensionForceParams.resize(4);
    vehicle->complianceParams.resize(4);
    vehicle->complianceStates.resize(4);
    vehicle->suspensionStates.resize(4);
    vehicle->suspensionForces.resize(4);
    vehicle->roadGeometryStates.resize(4);

    for (int i = 0; i < 4; ++i) {
        auto& sp = vehicle->suspensionParams[i];
        sp.suspensionAttachment = physx::PxTransform(
            physx::PxVec3(wheelOffsets[i].x, wheelOffsets[i].y, wheelOffsets[i].z),
            physx::PxQuat(physx::PxIdentity)
        );
        sp.suspensionTravelDir = physx::PxVec3(0, -1, 0);
        sp.suspensionTravelDist = suspensionTravel;
        sp.wheelAttachment = physx::PxTransform(physx::PxVec3(0,0,0), physx::PxQuat(physx::PxIdentity));

        auto& sfp = vehicle->suspensionForceParams[i];
        sfp.stiffness = suspensionStiffness;
        sfp.damping = suspensionDamping;
        sfp.sprungMass = 1000.0f / 4.0f;

        auto& cp = vehicle->complianceParams[i];
        cp.wheelToeAngle.addPair(0.0f, 0.0f);
        cp.wheelToeAngle.addPair(1.0f, 0.0f);
        cp.wheelCamberAngle.addPair(0.0f, 0.0f);
        cp.wheelCamberAngle.addPair(1.0f, 0.0f);
        cp.suspForceAppPoint.addPair(0.0f, physx::PxVec3(0,0,0));
        cp.suspForceAppPoint.addPair(1.0f, physx::PxVec3(0,0,0));
        cp.tireForceAppPoint.addPair(0.0f, physx::PxVec3(0,0,0));
        cp.tireForceAppPoint.addPair(1.0f, physx::PxVec3(0,0,0));
    }

    // 5.4 Компонент подвески — ИСПРАВЛЕННАЯ СИГНАТУРА
    class SuspensionComponent : public physx::vehicle2::PxVehicleSuspensionComponent
    {
    public:
        VehiclePhysics* m_vehicle; // ссылка на родительский объект

        const physx::vehicle2::PxVehicleAxleDescription* axleDesc = nullptr;
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelParams> wheelParams;
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleSuspensionParams> suspParams;
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleSuspensionComplianceParams> complianceParams;
        physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleRoadGeometryState> roadGeom;
        physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleSuspensionState> suspStates;
        physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleSuspensionComplianceState> complianceStates;
        physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleSuspensionForce> suspForces;
        const physx::vehicle2::PxVehicleRigidBodyState* rigidBodyState = nullptr;
        physx::vehicle2::PxVehicleArrayData<const physx::PxReal> steerResponseStates;

        SuspensionComponent(VehiclePhysics* v) : m_vehicle(v) {}

        virtual void getDataForSuspensionComponent(
            const physx::vehicle2::PxVehicleAxleDescription*& _axleDesc,
            const physx::vehicle2::PxVehicleRigidBodyParams*&,
            const physx::vehicle2::PxVehicleSuspensionStateCalculationParams*&,
            physx::vehicle2::PxVehicleArrayData<const physx::PxReal>& _steerResponseStates,
            const physx::vehicle2::PxVehicleRigidBodyState*& _rigidBodyState,
            physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelParams>& _wheelParams,
            physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleSuspensionParams>& _suspParams,
            physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleSuspensionComplianceParams>& _complianceParams,
            physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleSuspensionForceParams>& _suspForceParams,
            physx::vehicle2::PxVehicleSizedArrayData<const physx::vehicle2::PxVehicleAntiRollForceParams>&,
            physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleRoadGeometryState>& _roadGeom,
            physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleSuspensionState>& _suspStates,
            physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleSuspensionComplianceState>& _complianceStates,
            physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleSuspensionForce>& _suspForces,
            physx::vehicle2::PxVehicleAntiRollTorque*&) override
        {
            _axleDesc = axleDesc;
            _steerResponseStates = steerResponseStates;
            _rigidBodyState = rigidBodyState;
            _wheelParams = wheelParams;
            _suspParams = suspParams;
            _complianceParams = complianceParams;
            _suspForceParams.setData(m_vehicle->suspensionForceParams.data());
            _roadGeom = roadGeom;
            _suspStates = suspStates;
            _complianceStates = complianceStates;
            _suspForces = suspForces;
        }
    };

    auto* suspComp = new SuspensionComponent(vehicle);
    suspComp->axleDesc = vehicle->axleDescription;
    suspComp->wheelParams.setData(vehicle->wheelParams.data());
    suspComp->suspParams.setData(vehicle->suspensionParams.data());
    suspComp->complianceParams.setData(vehicle->complianceParams.data());
    suspComp->roadGeom.setData(vehicle->roadGeometryStates.data());
    suspComp->suspStates.setData(vehicle->suspensionStates.data());
    suspComp->complianceStates.setData(vehicle->complianceStates.data());
    suspComp->suspForces.setData(vehicle->suspensionForces.data());
    suspComp->rigidBodyState = &vehicle->rigidBodyState;
    suspComp->steerResponseStates.setData(vehicle->steerResponseStates.data());

    vehicle->suspensionComponent = suspComp;

    // -----------------------------------------------------------------
    // 6. Двигатель / коробка
    // -----------------------------------------------------------------
    vehicle->engineParams = new physx::vehicle2::PxVehicleEngineParams();
    vehicle->engineParams->peakTorque = enginePeakTorque;
    vehicle->engineParams->maxOmega = engineMaxOmega;

    vehicle->clutchParams = new physx::vehicle2::PxVehicleClutchParams();
    vehicle->clutchParams->accuracyMode = physx::vehicle2::PxVehicleClutchAccuracyMode::eBEST_POSSIBLE;

    vehicle->gearboxParams = new physx::vehicle2::PxVehicleGearboxParams();
    vehicle->gearboxParams->neutralGear = 0;
    vehicle->gearboxParams->nbRatios = 6;

    // -----------------------------------------------------------------
    // 7. Управление
    // -----------------------------------------------------------------
    vehicle->steerCommandState = new physx::vehicle2::PxVehicleCommandState();

    // -----------------------------------------------------------------
    // 8. Состояния
    // -----------------------------------------------------------------
    vehicle->wheelRigidBody1dStates.resize(4);
    vehicle->actuationStates.resize(4);
    vehicle->tireForces.resize(4);
    vehicle->brakeResponseStates.resize(4);

    // -----------------------------------------------------------------
    // 9. Описание осей
    // -----------------------------------------------------------------
    vehicle->axleDescription = new physx::vehicle2::PxVehicleAxleDescription();
    vehicle->axleDescription->nbWheels = 4;
    vehicle->axleDescription->wheelIdsInAxleOrder[0] = 0;
    vehicle->axleDescription->wheelIdsInAxleOrder[1] = 1;
    vehicle->axleDescription->wheelIdsInAxleOrder[2] = 2;
    vehicle->axleDescription->wheelIdsInAxleOrder[3] = 3;

    // -----------------------------------------------------------------
    // 10. Дифференциал
    // -----------------------------------------------------------------
    vehicle->differentialState = new physx::vehicle2::PxVehicleDifferentialState();

    // -----------------------------------------------------------------
    // 11. CustomEngineDrivetrainComponent
    // -----------------------------------------------------------------
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelParams> wheelParamsArray;
    wheelParamsArray.setData(vehicle->wheelParams.data());
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleTireForceParams> tireParamsArray;
    tireParamsArray.setData(vehicle->tireParams.data());
    physx::vehicle2::PxVehicleArrayData<const physx::PxReal> brakeResponseData;
    brakeResponseData.setData(vehicle->brakeResponseStates.data());
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleWheelActuationState> actuationData;
    actuationData.setData(vehicle->actuationStates.data());
    physx::vehicle2::PxVehicleArrayData<const physx::vehicle2::PxVehicleTireForce> tireForceData;
    tireForceData.setData(vehicle->tireForces.data());
    physx::vehicle2::PxVehicleArrayData<physx::vehicle2::PxVehicleWheelRigidBody1dState> wheelRigidBodyData;
    wheelRigidBodyData.setData(vehicle->wheelRigidBody1dStates.data());

    vehicle->drivetrainComponent = new CustomEngineDrivetrainComponent();
    vehicle->drivetrainComponent->setData(
        vehicle->axleDescription,
        wheelParamsArray,
        vehicle->engineParams,
        vehicle->clutchParams,
        vehicle->gearboxParams,
        brakeResponseData,
        actuationData,
        tireForceData,
        vehicle->throttleResponseState,
        vehicle->clutchResponseState,
        vehicle->differentialState,
        vehicle->constraintGroupState,
        wheelRigidBodyData,
        vehicle->engineState,
        vehicle->gearboxState,
        vehicle->clutchState
    );

    // -----------------------------------------------------------------
    // 12. Последовательность компонентов
    // -----------------------------------------------------------------
    vehicle->sequence = new physx::vehicle2::PxVehicleComponentSequence();
    vehicle->sequence->add(vehicle->rigidBodyComponent);
    vehicle->sequence->add(vehicle->wheelComponent);
    vehicle->sequence->add(vehicle->suspensionComponent);
    vehicle->sequence->add(vehicle->tireComponent);
    vehicle->sequence->add(vehicle->drivetrainComponent);

    // -----------------------------------------------------------------
    // 13. Анти-кампинг + рулевое
    // -----------------------------------------------------------------
    setRespawnPosition(vehicle->chassis, position, rotation);

    vehicle->steerResponseParams.resize(4);
    for (auto& p : vehicle->steerResponseParams) {
        p.maxResponse = physx::PxPi / 4;
        p.wheelResponseMultipliers[0] = 1.0f;
        p.wheelResponseMultipliers[1] = 1.0f;
        p.wheelResponseMultipliers[2] = 0.0f;
        p.wheelResponseMultipliers[3] = 0.0f;
    }
    vehicle->steerResponseStates.resize(4, 0.0f);

    LOG_INFO("Автомобиль создан успешно");
    return vehicle;
}

void Physics::destroyVehicle(VehiclePhysics* vehicle) {
    if (!vehicle) return;

    if (vehicle->sequence) { delete vehicle->sequence; vehicle->sequence = nullptr; }
    if (vehicle->rigidBodyComponent) { delete vehicle->rigidBodyComponent; vehicle->rigidBodyComponent = nullptr; }
    if (vehicle->wheelComponent) { delete vehicle->wheelComponent; vehicle->wheelComponent = nullptr; }
    if (vehicle->suspensionComponent) { delete vehicle->suspensionComponent; vehicle->suspensionComponent = nullptr; }
    if (vehicle->tireComponent) { delete vehicle->tireComponent; vehicle->tireComponent = nullptr; }
    if (vehicle->drivetrainComponent) { delete vehicle->drivetrainComponent; vehicle->drivetrainComponent = nullptr; }

    if (vehicle->engineParams) { delete vehicle->engineParams; vehicle->engineParams = nullptr; }
    if (vehicle->clutchParams) { delete vehicle->clutchParams; vehicle->clutchParams = nullptr; }
    if (vehicle->gearboxParams) { delete vehicle->gearboxParams; vehicle->gearboxParams = nullptr; }
    if (vehicle->axleDescription) { delete vehicle->axleDescription; vehicle->axleDescription = nullptr; }
    if (vehicle->differentialState) { delete vehicle->differentialState; vehicle->differentialState = nullptr; }
    if (vehicle->engineState) { delete vehicle->engineState; vehicle->engineState = nullptr; }
    if (vehicle->gearboxState) { delete vehicle->gearboxState; vehicle->gearboxState = nullptr; }
    if (vehicle->clutchState) { delete vehicle->clutchState; vehicle->clutchState = nullptr; }
    if (vehicle->throttleResponseState) { delete vehicle->throttleResponseState; vehicle->throttleResponseState = nullptr; }
    if (vehicle->clutchResponseState) { delete vehicle->clutchResponseState; vehicle->clutchResponseState = nullptr; }
    if (vehicle->constraintGroupState) { delete vehicle->constraintGroupState; vehicle->constraintGroupState = nullptr; }
    if (vehicle->steerCommandState) { delete vehicle->steerCommandState; vehicle->steerCommandState = nullptr; }

    if (vehicle->chassis) destroyRigidBody(vehicle->chassis);
    for (auto* wheel : vehicle->wheels) {
        if (wheel) destroyRigidBody(wheel);
    }

    delete vehicle;
}

void Physics::updateVehicle(VehiclePhysics* vehicle, float deltaTime)
{
    if (!vehicle || !vehicle->sequence || !vehicle->axleDescription) return;

    physx::vehicle2::PxVehiclePhysXSimulationContext context;

    // Новый способ задания системы координат
    physx::vehicle2::PxVehicleFrame frame;
    frame.setToDefault(); // стандартная система: +Z вперёд, +X вправо, +Y вверх

    // Если нужно переопределить вручную:
    frame.lngAxis = physx::vehicle2::PxVehicleAxes::ePosZ; // продольная ось (вперёд)
    frame.latAxis = physx::vehicle2::PxVehicleAxes::ePosX; // боковая ось (вправо)
    frame.vrtAxis = physx::vehicle2::PxVehicleAxes::ePosY; // вертикальная ось (вверх)

    context.frame = frame;
    context.gravity = physx::PxVec3(m_gravity.x, m_gravity.y, m_gravity.z);
    context.physxScene = m_scene;

    vehicle->sequence->update(deltaTime, context);

    // Синхронизация шасси
    physx::PxRigidActor* actor = vehicle->chassis->body;
    physx::PxTransform pose = actor->getGlobalPose();
    vehicle->chassis->position = pxToGlm(pose.p);
    vehicle->chassis->rotation = pxToGlm(pose.q);
}


void Physics::applyEngineForce(VehiclePhysics* vehicle, float force) {
    if (!vehicle || !vehicle->steerCommandState) return;
    vehicle->steerCommandState->throttle = force / vehicle->maxEngineForce;
    vehicle->steerCommandState->brakes[0] = 0.0f;
    vehicle->steerCommandState->brakes[1] = 0.0f;
}

void Physics::applyBrakeForce(VehiclePhysics* vehicle, float force) {
    if (!vehicle || !vehicle->steerCommandState) return;
    float brakeValue = force / vehicle->maxBrakeForce;
    vehicle->steerCommandState->brakes[0] = brakeValue;
    vehicle->steerCommandState->brakes[1] = brakeValue;
    vehicle->steerCommandState->throttle = 0.0f;
}

void Physics::setSteeringAngle(VehiclePhysics* vehicle, float angle) {
    if (!vehicle || !vehicle->steerCommandState) {
        LOG_WARNING("Невозможно установить угол поворота: автомобиль или состояние не заданы");
        return;
    }

    vehicle->steerCommandState->steer = angle / vehicle->maxSteeringAngle;
    vehicle->steeringAngle = glm::vec3(0.0f, angle, 0.0f);
    LOG_INFO("Угол поворота установлен: " + std::to_string(angle));
}

void Physics::setCollisionCallback(std::function<void(const CollisionInfo&)> callback) {
    m_collisionCallback = callback;
}

bool Physics::raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal, RigidBody*& hitBody) {
    physx::PxRaycastBuffer hit;
    physx::PxVec3 origin(from.x, from.y, from.z);
    physx::PxVec3 dir(to.x - from.x, to.y - from.y, to.z - from.z);
    float distance = dir.magnitude();
    dir.normalize();
    bool result = m_scene->raycast(origin, dir, distance, hit);
    if (result) {
        hitPoint = pxToGlm(hit.block.position);
        hitNormal = pxToGlm(hit.block.normal);
        hitBody = static_cast<RigidBody*>(hit.block.actor->userData);
    }
    return result;
}

void Physics::setRespawnPosition(RigidBody* body, const glm::vec3& position, const glm::quat& rotation) {
    m_respawnInfo[body] = {position, rotation, 0.0f, false};
}

void Physics::respawnBody(RigidBody* body) {
    if (!body) return;
    auto it = m_respawnInfo.find(body);
    if (it == m_respawnInfo.end()) return;

    auto* pxBody = static_cast<physx::PxRigidDynamic*>(body->body);
    pxBody->setGlobalPose(glmToPx(it->second.position, it->second.rotation));
    pxBody->setLinearVelocity({0, 0, 0});
    pxBody->setAngularVelocity({0, 0, 0});
    it->second.antiCampTime = m_antiCampDuration;
    it->second.isRespawning = true;
}

bool Physics::isInAntiCampMode(RigidBody* body) const {
    auto it = m_respawnInfo.find(body);
    return it != m_respawnInfo.end() && it->second.isRespawning;
}

void Physics::updateRespawnSystem(float deltaTime) {
    for (auto& pair : m_respawnInfo) {
        if (pair.second.antiCampTime > 0.0f) {
            pair.second.antiCampTime -= deltaTime;
            if (pair.second.antiCampTime <= 0.0f) {
                pair.second.isRespawning = false;
            }
        }
    }
}

physx::PxTransform Physics::glmToPx(const glm::vec3& pos, const glm::quat& rot) const {
    return physx::PxTransform({pos.x, pos.y, pos.z}, physx::PxQuat(rot.x, rot.y, rot.z, rot.w));
}

glm::vec3 Physics::pxToGlm(const physx::PxVec3& vec) const {
    return {vec.x, vec.y, vec.z};
}

glm::quat Physics::pxToGlm(const physx::PxQuat& quat) const {
    return {quat.w, quat.x, quat.y, quat.z};
}

} // namespace Core