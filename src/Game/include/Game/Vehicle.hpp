#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Core {
    class Model;
    class Physics;
}

namespace Game {

/**
 * Vehicle class for Rusty Rage
 * Handles vehicle physics, weapons, and post-apocalyptic customization
 */
class Vehicle {
public:
    enum class VehicleType {
        RustySedan,     // Ржавый седан
        WastelandTruck, // Грузовик пустоши
        ScrapRacer,     // Гоночный хлам
        ArmoredCar,     // Бронированная машина
        SpeedDemon,     // Демон скорости
        DeathWagon      // Фургон смерти
    };
    
    enum class WeaponType {
        None,
        MachineGun,     // Пулемёт
        Rocket,         // Ракета
        Flamethrower,   // Огнемёт
        Mine,           // Мина
        EMP             // ЭМИ
    };
    
    enum class PowerUpType {
        None,
        SpeedBoost,     // Ускорение
        Shield,         // Щит
        Repair,         // Ремонт
        Ammo,           // Боеприпасы
        Invisibility,   // Невидимость
        TimeSlow        // Замедление времени
    };
    
    struct VehicleStats {
        float maxSpeed;
        float acceleration;
        float handling;
        float armor;
        float weaponDamage;
        float health;
        float maxHealth;
        
        VehicleStats() : maxSpeed(100.0f), acceleration(50.0f), handling(0.8f),
                        armor(50.0f), weaponDamage(25.0f), health(100.0f), maxHealth(100.0f) {}
    };
    
    struct WeaponSystem {
        WeaponType currentWeapon;
        int ammo;
        int maxAmmo;
        float fireRate;
        float lastFireTime;
        float range;
        float damage;
        bool isReloading;
        float reloadTime;
        
        WeaponSystem() : currentWeapon(WeaponType::None), ammo(0), maxAmmo(100),
                        fireRate(0.1f), lastFireTime(0.0f), range(200.0f), damage(25.0f),
                        isReloading(false), reloadTime(2.0f) {}
    };
    
    struct PowerUpSystem {
        PowerUpType activePowerUp;
        float powerUpTime;
        float maxPowerUpTime;
        bool isActive;
        
        PowerUpSystem() : activePowerUp(PowerUpType::None), powerUpTime(0.0f),
                         maxPowerUpTime(10.0f), isActive(false) {}
    };

    Vehicle();
    Vehicle(VehicleType type, uint32_t playerId);
    ~Vehicle();

    // Basic properties
    uint32_t getPlayerId() const { return m_playerId; }
    VehicleType getType() const { return m_type; }
    const std::string& getName() const { return m_name; }
    
    // Position and physics
    const glm::vec3& getPosition() const { return m_position; }
    const glm::quat& getRotation() const { return m_rotation; }
    const glm::vec3& getVelocity() const { return m_velocity; }
    
    void setPosition(const glm::vec3& position) { m_position = position; }
    void setRotation(const glm::quat& rotation) { m_rotation = rotation; }
    void setVelocity(const glm::vec3& velocity) { m_velocity = velocity; }
    
    // Vehicle stats
    const VehicleStats& getStats() const { return m_stats; }
    VehicleStats& getStats() { return m_stats; }
    
    float getHealth() const { return m_stats.health; }
    float getMaxHealth() const { return m_stats.maxHealth; }
    float getHealthPercentage() const { return m_stats.health / m_stats.maxHealth; }
    
    // Movement and control
    void accelerate(float force);
    void brake(float force);
    void steer(float angle);
    void update(float deltaTime);
    
    // Combat
    const WeaponSystem& getWeaponSystem() const { return m_weaponSystem; }
    WeaponSystem& getWeaponSystem() { return m_weaponSystem; }
    
    bool canFire() const;
    bool fire();
    void reload();
    void switchWeapon(WeaponType weapon);
    
    // Power-ups
    const PowerUpSystem& getPowerUpSystem() const { return m_powerUpSystem; }
    PowerUpSystem& getPowerUpSystem() { return m_powerUpSystem; }
    
    void activatePowerUp(PowerUpType powerUp, float duration = 10.0f);
    void updatePowerUps(float deltaTime);
    bool hasActivePowerUp() const { return m_powerUpSystem.isActive; }
    
    // Damage and destruction
    void takeDamage(float damage);
    void repair(float amount);
    bool isDestroyed() const { return m_stats.health <= 0.0f; }
    bool isAlive() const { return m_stats.health > 0.0f; }
    
    // Respawn system
    void respawn(const glm::vec3& position, const glm::quat& rotation);
    void setRespawnPosition(const glm::vec3& position) { m_respawnPosition = position; }
    const glm::vec3& getRespawnPosition() const { return m_respawnPosition; }
    
    // Anti-camping system
    void setAntiCampMode(bool enabled) { m_antiCampMode = enabled; }
    bool isInAntiCampMode() const { return m_antiCampMode; }
    void updateAntiCampTimer(float deltaTime);
    bool canAttack() const;
    bool canBeAttacked() const;
    
    // Model and rendering
    void setModel(std::shared_ptr<Core::Model> model) { m_model = model; }
    std::shared_ptr<Core::Model> getModel() const { return m_model; }
    
    // Physics integration
    void setPhysicsBody(void* body) { m_physicsBody = body; }
    void* getPhysicsBody() const { return m_physicsBody; }
    void setPhysics(Core::Physics* physics) { m_physics = physics; }
    
    // Validation
    bool isValid() const;
    
    // Serialization for network
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    uint32_t m_playerId;
    VehicleType m_type;
    std::string m_name;
    
    // Transform
    glm::vec3 m_position;
    glm::quat m_rotation;
    glm::vec3 m_velocity;
    glm::vec3 m_angularVelocity;
    
    // Stats and systems
    VehicleStats m_stats;
    WeaponSystem m_weaponSystem;
    PowerUpSystem m_powerUpSystem;
    
    // Respawn system
    glm::vec3 m_respawnPosition;
    glm::quat m_respawnRotation;
    bool m_antiCampMode;
    float m_antiCampTimer;
    float m_antiCampDuration;
    
    // Model and physics
    std::shared_ptr<Core::Model> m_model;
    void* m_physicsBody;
    Core::Physics* m_physics;
    
    // Helper methods
    void initializeVehicleType();
    void updateMovement(float deltaTime);
    void updateWeapons(float deltaTime);
    void applyPowerUpEffects();
    void removePowerUpEffects();
    
    // Vehicle type specific stats
    void setRustySedanStats();
    void setWastelandTruckStats();
    void setScrapRacerStats();
    void setArmoredCarStats();
    void setSpeedDemonStats();
    void setDeathWagonStats();
    
    // Weapon specific methods
    void initializeWeapon(WeaponType weapon);
    float getWeaponDamage(WeaponType weapon) const;
    float getWeaponRange(WeaponType weapon) const;
    float getWeaponFireRate(WeaponType weapon) const;
    
    // Power-up specific methods
    void applySpeedBoost();
    void applyShield();
    void applyRepair();
    void applyAmmo();
    void applyInvisibility();
    void applyTimeSlow();
    
    void removeSpeedBoost();
    void removeShield();
    void removeRepair();
    void removeAmmo();
    void removeInvisibility();
    void removeTimeSlow();
};

} // namespace Game
