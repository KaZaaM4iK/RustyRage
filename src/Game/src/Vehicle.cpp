#include "Game/Vehicle.hpp"
#include "Core/Logging.hpp"
#include "Core/Physics.hpp"

namespace Game {

Vehicle::Vehicle() 
    : m_playerId(0),
      m_type(VehicleType::RustySedan),
      m_name("Rusty Sedan"),
      m_position(0.0f),
      m_rotation(1.0f, 0.0f, 0.0f, 0.0f),
      m_velocity(0.0f),
      m_angularVelocity(0.0f),
      m_respawnPosition(0.0f),
      m_respawnRotation(1.0f, 0.0f, 0.0f, 0.0f),
      m_antiCampMode(false),
      m_antiCampTimer(0.0f),
      m_antiCampDuration(3.0f),
      m_physicsBody(nullptr),
      m_physics(nullptr) {
    
    initializeVehicleType();
}

Vehicle::Vehicle(VehicleType type, uint32_t playerId)
    : m_playerId(playerId),
      m_type(type),
      m_position(0.0f),
      m_rotation(1.0f, 0.0f, 0.0f, 0.0f),
      m_velocity(0.0f),
      m_angularVelocity(0.0f),
      m_respawnPosition(0.0f),
      m_respawnRotation(1.0f, 0.0f, 0.0f, 0.0f),
      m_antiCampMode(false),
      m_antiCampTimer(0.0f),
      m_antiCampDuration(3.0f),
      m_physicsBody(nullptr),
      m_physics(nullptr) {
    
    initializeVehicleType();
}

Vehicle::~Vehicle() {
}

void Vehicle::accelerate(float force) {
    if (!m_physics || !m_physicsBody || m_antiCampMode) {
        LOG_WARNING("Cannot accelerate: physics or body not set, or in anti-camp mode");
        return;
    }

    Core::Physics::VehiclePhysics* vehiclePhysics = static_cast<Core::Physics::VehiclePhysics*>(m_physicsBody);
    float adjustedForce = force * m_stats.acceleration;
    if (adjustedForce > vehiclePhysics->maxEngineForce) {
        adjustedForce = vehiclePhysics->maxEngineForce;
    }

    m_physics->applyEngineForce(vehiclePhysics, adjustedForce);
    LOG_DEBUGF("Vehicle %u accelerating with force %.2f (adjusted: %.2f)", m_playerId, force, adjustedForce);
}

void Vehicle::brake(float force) {
    if (!m_physics || !m_physicsBody || m_antiCampMode) {
        LOG_WARNING("Cannot brake: physics or body not set, or in anti-camp mode");
        return;
    }

    Core::Physics::VehiclePhysics* vehiclePhysics = static_cast<Core::Physics::VehiclePhysics*>(m_physicsBody);
    float adjustedForce = force * m_stats.acceleration * 0.5f; // Торможение менее интенсивное
    if (adjustedForce > vehiclePhysics->maxBrakeForce) {
        adjustedForce = vehiclePhysics->maxBrakeForce;
    }

    m_physics->applyBrakeForce(vehiclePhysics, adjustedForce);
    LOG_DEBUGF("Vehicle %u braking with force %.2f (adjusted: %.2f)", m_playerId, force, adjustedForce);
}

void Vehicle::steer(float angle) {
    if (!m_physics || !m_physicsBody || m_antiCampMode) {
        LOG_WARNING("Cannot steer: physics or body not set, or in anti-camp mode");
        return;
    }

    Core::Physics::VehiclePhysics* vehiclePhysics = static_cast<Core::Physics::VehiclePhysics*>(m_physicsBody);
    float adjustedAngle = angle * m_stats.handling;
    if (std::abs(adjustedAngle) > vehiclePhysics->maxSteeringAngle) {
        adjustedAngle = (adjustedAngle > 0 ? 1.0f : -1.0f) * vehiclePhysics->maxSteeringAngle;
    }

    m_physics->setSteeringAngle(vehiclePhysics, adjustedAngle);
    LOG_DEBUGF("Vehicle %u steering angle %.2f (adjusted: %.2f)", m_playerId, angle, adjustedAngle);
}

void Vehicle::update(float deltaTime) {
    updateMovement(deltaTime);
    updateWeapons(deltaTime);
    updatePowerUps(deltaTime);
    updateAntiCampTimer(deltaTime);
}

bool Vehicle::canFire() const {
    return m_weaponSystem.currentWeapon != WeaponType::None &&
           m_weaponSystem.ammo > 0 &&
           !m_weaponSystem.isReloading &&
           !m_antiCampMode;
}

bool Vehicle::fire() {
    if (!canFire()) {
        return false;
    }
    
    m_weaponSystem.ammo--;
    m_weaponSystem.lastFireTime = 0.0f; // Reset fire timer
    
    LOG_DEBUGF("Vehicle %u fired weapon type %d", m_playerId, (int)m_weaponSystem.currentWeapon);
    return true;
}

void Vehicle::reload() {
    if (m_weaponSystem.isReloading) {
        return;
    }
    
    m_weaponSystem.isReloading = true;
    m_weaponSystem.ammo = m_weaponSystem.maxAmmo;
    
    LOG_DEBUGF("Vehicle %u reloading weapon", m_playerId);
}

void Vehicle::switchWeapon(WeaponType weapon) {
    if (weapon == m_weaponSystem.currentWeapon) {
        return;
    }
    
    m_weaponSystem.currentWeapon = weapon;
    initializeWeapon(weapon);
    
    LOG_DEBUGF("Vehicle %u switched to weapon type %d", m_playerId, (int)weapon);
}

void Vehicle::activatePowerUp(PowerUpType powerUp, float duration) {
    m_powerUpSystem.activePowerUp = powerUp;
    m_powerUpSystem.powerUpTime = duration;
    m_powerUpSystem.maxPowerUpTime = duration;
    m_powerUpSystem.isActive = true;
    
    applyPowerUpEffects();
    
    LOG_DEBUGF("Vehicle %u activated power-up type %d for %.1f seconds", 
               m_playerId, (int)powerUp, duration);
}

void Vehicle::updatePowerUps(float deltaTime) {
    if (m_powerUpSystem.isActive) {
        m_powerUpSystem.powerUpTime -= deltaTime;
        
        if (m_powerUpSystem.powerUpTime <= 0.0f) {
            removePowerUpEffects();
            m_powerUpSystem.isActive = false;
            m_powerUpSystem.activePowerUp = PowerUpType::None;
        }
    }
}

void Vehicle::takeDamage(float damage) {
    m_stats.health -= damage;
    if (m_stats.health < 0.0f) {
        m_stats.health = 0.0f;
    }
    
    LOG_DEBUGF("Vehicle %u took %.2f damage, health: %.2f", 
               m_playerId, damage, m_stats.health);
}

void Vehicle::repair(float amount) {
    m_stats.health += amount;
    if (m_stats.health > m_stats.maxHealth) {
        m_stats.health = m_stats.maxHealth;
    }
    
    LOG_DEBUGF("Vehicle %u repaired %.2f, health: %.2f", 
               m_playerId, amount, m_stats.health);
}

void Vehicle::respawn(const glm::vec3& position, const glm::quat& rotation) {
    if (!m_physics || !m_physicsBody) {
        LOG_WARNING("Cannot respawn: physics or body not set");
        return;
    }

    m_position = position;
    m_rotation = rotation;
    m_velocity = glm::vec3(0.0f);
    m_angularVelocity = glm::vec3(0.0f);
    m_stats.health = m_stats.maxHealth;
    m_antiCampMode = true;
    m_antiCampTimer = m_antiCampDuration;

    Core::Physics::VehiclePhysics* vehiclePhysics = static_cast<Core::Physics::VehiclePhysics*>(m_physicsBody);
    m_physics->setRespawnPosition(vehiclePhysics->chassis, position, rotation);
    m_physics->respawnBody(vehiclePhysics->chassis);

    LOG_DEBUGF("Vehicle %u respawned at (%.2f, %.2f, %.2f)", m_playerId, position.x, position.y, position.z);
}

void Vehicle::updateAntiCampTimer(float deltaTime) {
    if (m_antiCampMode) {
        m_antiCampTimer -= deltaTime;
        if (m_antiCampTimer <= 0.0f) {
            m_antiCampMode = false;
            LOG_DEBUGF("Vehicle %u exited anti-camp mode", m_playerId);
        }
    }
}

bool Vehicle::canAttack() const {
    return !m_antiCampMode && m_stats.health > 0.0f;
}

bool Vehicle::canBeAttacked() const {
    return !m_antiCampMode && m_stats.health > 0.0f;
}

bool Vehicle::isValid() const {
    return m_playerId != 0 && m_stats.health > 0.0f;
}

std::string Vehicle::serialize() const {
    // TODO: Implement JSON serialization
    return "{}";
}

bool Vehicle::deserialize(const std::string& data) {
    // TODO: Implement JSON deserialization
    return true;
}

void Vehicle::initializeVehicleType() {
    switch (m_type) {
        case VehicleType::RustySedan:
            setRustySedanStats();
            m_name = "Rusty Sedan";
            break;
        case VehicleType::WastelandTruck:
            setWastelandTruckStats();
            m_name = "Wasteland Truck";
            break;
        case VehicleType::ScrapRacer:
            setScrapRacerStats();
            m_name = "Scrap Racer";
            break;
        case VehicleType::ArmoredCar:
            setArmoredCarStats();
            m_name = "Armored Car";
            break;
        case VehicleType::SpeedDemon:
            setSpeedDemonStats();
            m_name = "Speed Demon";
            break;
        case VehicleType::DeathWagon:
            setDeathWagonStats();
            m_name = "Death Wagon";
            break;
    }
}

void Vehicle::updateMovement(float deltaTime) {
    if (!m_physics || !m_physicsBody) {
        LOG_WARNING("Cannot update movement: physics or body not set");
        return;
    }

    Core::Physics::VehiclePhysics* vehiclePhysics = static_cast<Core::Physics::VehiclePhysics*>(m_physicsBody);
    m_physics->updateVehicle(vehiclePhysics, deltaTime);

    if (vehiclePhysics->chassis && vehiclePhysics->chassis->body) {
        physx::PxRigidDynamic* chassis = vehiclePhysics->chassis->body;
        physx::PxTransform transform = chassis->getGlobalPose();
        m_position = m_physics->pxToGlm(transform.p);
        m_rotation = m_physics->pxToGlm(transform.q);
        m_velocity = m_physics->pxToGlm(chassis->getLinearVelocity());
        m_angularVelocity = m_physics->pxToGlm(chassis->getAngularVelocity());

        float speed = glm::length(m_velocity);
        if (speed > m_stats.maxSpeed) {
            m_velocity = glm::normalize(m_velocity) * m_stats.maxSpeed;
            chassis->setLinearVelocity(m_physics->glmToPx(m_velocity, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)).p);
        }
    }

    updateAntiCampTimer(deltaTime);

    LOG_DEBUGF("Vehicle %u updated: position (%.2f, %.2f, %.2f), speed %.2f",
               m_playerId, m_position.x, m_position.y, m_position.z, glm::length(m_velocity));
}

void Vehicle::updateWeapons(float deltaTime) {
    if (m_weaponSystem.isReloading) {
        m_weaponSystem.lastFireTime += deltaTime;
        if (m_weaponSystem.lastFireTime >= m_weaponSystem.reloadTime) {
            m_weaponSystem.isReloading = false;
            m_weaponSystem.lastFireTime = 0.0f;
        }
    } else {
        m_weaponSystem.lastFireTime += deltaTime;
    }
}

void Vehicle::applyPowerUpEffects() {
    switch (m_powerUpSystem.activePowerUp) {
        case PowerUpType::SpeedBoost:
            applySpeedBoost();
            break;
        case PowerUpType::Shield:
            applyShield();
            break;
        case PowerUpType::Repair:
            applyRepair();
            break;
        case PowerUpType::Ammo:
            applyAmmo();
            break;
        case PowerUpType::Invisibility:
            applyInvisibility();
            break;
        case PowerUpType::TimeSlow:
            applyTimeSlow();
            break;
        default:
            break;
    }
}

void Vehicle::removePowerUpEffects() {
    switch (m_powerUpSystem.activePowerUp) {
        case PowerUpType::SpeedBoost:
            removeSpeedBoost();
            break;
        case PowerUpType::Shield:
            removeShield();
            break;
        case PowerUpType::Repair:
            removeRepair();
            break;
        case PowerUpType::Ammo:
            removeAmmo();
            break;
        case PowerUpType::Invisibility:
            removeInvisibility();
            break;
        case PowerUpType::TimeSlow:
            removeTimeSlow();
            break;
        default:
            break;
    }
}

void Vehicle::setRustySedanStats() {
    m_stats.maxSpeed = 80.0f;
    m_stats.acceleration = 40.0f;
    m_stats.handling = 0.7f;
    m_stats.armor = 60.0f;
    m_stats.weaponDamage = 20.0f;
    m_stats.health = 100.0f;
    m_stats.maxHealth = 100.0f;
}

void Vehicle::setWastelandTruckStats() {
    m_stats.maxSpeed = 60.0f;
    m_stats.acceleration = 30.0f;
    m_stats.handling = 0.5f;
    m_stats.armor = 100.0f;
    m_stats.weaponDamage = 30.0f;
    m_stats.health = 150.0f;
    m_stats.maxHealth = 150.0f;
}

void Vehicle::setScrapRacerStats() {
    m_stats.maxSpeed = 120.0f;
    m_stats.acceleration = 60.0f;
    m_stats.handling = 0.9f;
    m_stats.armor = 30.0f;
    m_stats.weaponDamage = 15.0f;
    m_stats.health = 80.0f;
    m_stats.maxHealth = 80.0f;
}

void Vehicle::setArmoredCarStats() {
    m_stats.maxSpeed = 70.0f;
    m_stats.acceleration = 35.0f;
    m_stats.handling = 0.6f;
    m_stats.armor = 120.0f;
    m_stats.weaponDamage = 25.0f;
    m_stats.health = 120.0f;
    m_stats.maxHealth = 120.0f;
}

void Vehicle::setSpeedDemonStats() {
    m_stats.maxSpeed = 140.0f;
    m_stats.acceleration = 70.0f;
    m_stats.handling = 0.8f;
    m_stats.armor = 20.0f;
    m_stats.weaponDamage = 10.0f;
    m_stats.health = 60.0f;
    m_stats.maxHealth = 60.0f;
}

void Vehicle::setDeathWagonStats() {
    m_stats.maxSpeed = 50.0f;
    m_stats.acceleration = 25.0f;
    m_stats.handling = 0.4f;
    m_stats.armor = 150.0f;
    m_stats.weaponDamage = 40.0f;
    m_stats.health = 200.0f;
    m_stats.maxHealth = 200.0f;
}

void Vehicle::initializeWeapon(WeaponType weapon) {
    m_weaponSystem.damage = getWeaponDamage(weapon);
    m_weaponSystem.range = getWeaponRange(weapon);
    m_weaponSystem.fireRate = getWeaponFireRate(weapon);
}

float Vehicle::getWeaponDamage(WeaponType weapon) const {
    switch (weapon) {
        case WeaponType::MachineGun: return 25.0f;
        case WeaponType::Rocket: return 50.0f;
        case WeaponType::Flamethrower: return 30.0f;
        case WeaponType::Mine: return 40.0f;
        case WeaponType::EMP: return 0.0f; // Special effect
        default: return 0.0f;
    }
}

float Vehicle::getWeaponRange(WeaponType weapon) const {
    switch (weapon) {
        case WeaponType::MachineGun: return 200.0f;
        case WeaponType::Rocket: return 300.0f;
        case WeaponType::Flamethrower: return 100.0f;
        case WeaponType::Mine: return 50.0f;
        case WeaponType::EMP: return 150.0f;
        default: return 0.0f;
    }
}

float Vehicle::getWeaponFireRate(WeaponType weapon) const {
    switch (weapon) {
        case WeaponType::MachineGun: return 0.1f;
        case WeaponType::Rocket: return 2.0f;
        case WeaponType::Flamethrower: return 0.05f;
        case WeaponType::Mine: return 1.0f;
        case WeaponType::EMP: return 3.0f;
        default: return 1.0f;
    }
}

void Vehicle::applySpeedBoost() {
    m_stats.maxSpeed *= 1.5f;
    m_stats.acceleration *= 1.3f;
}

void Vehicle::applyShield() {
    m_stats.armor *= 2.0f;
}

void Vehicle::applyRepair() {
    repair(50.0f);
}

void Vehicle::applyAmmo() {
    m_weaponSystem.ammo = m_weaponSystem.maxAmmo;
}

void Vehicle::applyInvisibility() {
    // TODO: Implement invisibility effect
}

void Vehicle::applyTimeSlow() {
    // TODO: Implement time slow effect
}

void Vehicle::removeSpeedBoost() {
    m_stats.maxSpeed /= 1.5f;
    m_stats.acceleration /= 1.3f;
}

void Vehicle::removeShield() {
    m_stats.armor /= 2.0f;
}

void Vehicle::removeRepair() {
    // Repair is instant, no removal needed
}

void Vehicle::removeAmmo() {
    // Ammo is consumed, no removal needed
}

void Vehicle::removeInvisibility() {
    // TODO: Remove invisibility effect
}

void Vehicle::removeTimeSlow() {
    // TODO: Remove time slow effect
}

} // namespace Game
