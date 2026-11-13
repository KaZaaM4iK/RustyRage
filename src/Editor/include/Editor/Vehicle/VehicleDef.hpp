#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Editor {

enum class VehicleChassisClass {
  LightBuggy,
  MuscleInterceptor,
  ArmoredPickup,
  HeavyWarRig,
  HoverCraft
};

enum class EngineType {
  V6TwinTurbo,
  V8Supercharged,
  DieselMonster,
  ElectricHyperdrive,
  JetTurbine
};

enum class WheelType {
  RacingSlicks,
  OffroadChunky,
  SpikedTires,
  HeavyTracks,
  HoverRepulsors
};

struct WeaponHardpoint {
  std::string slotId; // "primary_roof", "secondary_hood", "rear_mines", "side_sponson"
  std::string slotLabel;
  glm::vec3 localPosition{0.0f};
  glm::vec3 localForward{0.0f, 0.0f, 1.0f};
  std::string defaultWeaponId; // "minigun_20mm", "rocket_pod", "flamethrower", "oil_dropper"
  float ammoMultiplier = 1.0f;
  float fireRateMultiplier = 1.0f;
};

struct VehicleArmorPlate {
  std::string name;
  std::string zone; // "front_ram", "left_skirt", "right_skirt", "roof", "rear"
  float armorHp = 150.0f;
  float ramDamageBonus = 35.0f;
  float weight = 80.0f;
  glm::vec3 localPosition{0.0f};
  glm::vec3 boundingBox{1.5f, 0.8f, 0.2f};
};

struct VehicleVisuals {
  glm::vec4 primaryColor{0.85f, 0.25f, 0.15f, 1.0f};
  glm::vec4 secondaryColor{0.15f, 0.15f, 0.15f, 1.0f};
  float rustWeathering = 0.35f; // 0.0 = Factory fresh, 1.0 = Max post-apocalyptic rust
  float dirtMud = 0.25f;
  float metalness = 0.65f;
  float roughness = 0.45f;
  std::string liveryId = "flames_classic";
  bool neonUnderglow = false;
  glm::vec4 neonColor{0.2f, 0.8f, 1.0f, 1.0f};
};

struct VehicleDef {
  static constexpr int CURRENT_VERSION = 3;

  std::string id = "war_rig_01";
  std::string name = "Громобой Постапокалипсиса";
  int version = CURRENT_VERSION;
  std::string author = "Mad Mechanic";

  VehicleChassisClass chassis = VehicleChassisClass::ArmoredPickup;
  EngineType engine = EngineType::V8Supercharged;
  WheelType wheels = WheelType::OffroadChunky;

  std::string meshPath;
  std::string collisionMeshPath;

  // Динамические физические характеристики
  float mass = 1650.0f;           // кг
  float maxSpeed = 48.0f;          // м/с (~172 км/ч)
  float acceleration = 18.5f;      // м/с^2
  float turnRate = 2.2f;           // рад/с
  float grip = 1.15f;              // Коэффициент сцепления
  float driftFactor = 0.45f;       // Склонность к управляемому заносу
  float maxHealth = 650.0f;        // HP корпуса
  float nitroCapacity = 100.0f;    // Запас закиси азота
  float nitroBurnRate = 25.0f;     // Расход в сек
  float ramDamageMultiplier = 1.4f;// Множитель таранного урона

  // Модули
  std::vector<WeaponHardpoint> hardpoints;
  std::vector<VehicleArmorPlate> armorPlates;
  VehicleVisuals visuals;

  bool isApproved = false;
  std::string moderationStatus = "pending";

  void createDefault(const std::string &newId);
  void applyChassisPreset(VehicleChassisClass newChassis);
  void recalculateStats();
  
  bool saveToFile(const std::string &path) const;
  bool loadFromFile(const std::string &path);

  static std::string chassisToString(VehicleChassisClass chassis);
  static VehicleChassisClass chassisFromString(const std::string &str);
  static std::string engineToString(EngineType engine);
  static EngineType engineFromString(const std::string &str);
  static std::string wheelsToString(WheelType wheels);
  static WheelType wheelsFromString(const std::string &str);
};

} // namespace Editor
