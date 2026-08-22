#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Editor {

struct WeaponHardpoint {
  std::string slotId; // "primary", "secondary", …
  glm::vec3 localPosition{0.0f};
  glm::vec3 localForward{0.0f, 0.0f, 1.0f};
  std::string defaultWeaponId; // id из будущего weapons/
};

struct VehicleDef {
  static constexpr int CURRENT_VERSION = 1;

  std::string id; // "hotrod"
  std::string name = "New Vehicle";
  int version = CURRENT_VERSION;
  std::string author = "unknown";

  std::string meshPath;
  std::string collisionMeshPath;

  // Общие для Arena Speed и Vigilante
  float mass = 1200.0f;
  float maxSpeed = 40.0f; // м/с
  float acceleration = 15.0f;
  float turnRate = 1.8f; // рад/с
  float grip = 1.0f;
  float maxHealth = 100.0f;

  std::vector<WeaponHardpoint> hardpoints;

  bool isApproved = false;
  std::string moderationStatus = "pending";

  void createDefault(const std::string &newId);
  bool saveToFile(const std::string &path) const;
  bool loadFromFile(const std::string &path);
};

} // namespace Editor