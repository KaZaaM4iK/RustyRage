#include "Editor/Vehicle/VehicleDef.hpp"
#include "Core/Logging.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Editor {

namespace {

std::string trim(const std::string &value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string escapeJson(const std::string &value) {
  std::string result;
  for (char ch : value) {
    if (ch == '\\') result += "\\\\";
    else if (ch == '"') result += "\\\"";
    else if (ch == '\n') result += "\\n";
    else if (ch == '\r') result += "\\r";
    else if (ch == '\t') result += "\\t";
    else result += ch;
  }
  return result;
}

std::string extractBlock(const std::string &body, const std::string &key) {
  const std::string pattern = "\"" + key + "\"";
  size_t pos = body.find(pattern);
  if (pos == std::string::npos) return {};
  pos = body.find(':', pos);
  if (pos == std::string::npos) return {};
  ++pos;
  while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
  if (pos >= body.size()) return {};
  
  char open = body[pos];
  if (open == '[' || open == '{') {
    char close = (open == '[') ? ']' : '}';
    int depth = 0;
    bool inString = false, escape = false;
    size_t start = pos;
    for (size_t i = pos; i < body.size(); ++i) {
      char ch = body[i];
      if (inString) {
        if (escape) { escape = false; continue; }
        if (ch == '\\') { escape = true; continue; }
        if (ch == '"') inString = false;
      } else {
        if (ch == '"') inString = true;
        else if (ch == open) ++depth;
        else if (ch == close) {
          --depth;
          if (depth == 0) return body.substr(start, i - start + 1);
        }
      }
    }
    return body.substr(start);
  }
  if (open == '"') {
    std::string result;
    ++pos;
    bool escape = false;
    while (pos < body.size()) {
      char ch = body[pos++];
      if (escape) {
        if (ch == 'n') result += '\n';
        else if (ch == 't') result += '\t';
        else if (ch == 'r') result += '\r';
        else result += ch;
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        return result;
      } else {
        result += ch;
      }
    }
  }
  size_t start = pos;
  while (pos < body.size() && body[pos] != ',' && body[pos] != '}' && body[pos] != ']' && !std::isspace(static_cast<unsigned char>(body[pos]))) {
    ++pos;
  }
  return body.substr(start, pos - start);
}

std::string extractStringField(const std::string &body, const std::string &key) {
  return extractBlock(body, key);
}

int extractIntField(const std::string &body, const std::string &key, int fallback = 0) {
  const std::string value = extractBlock(body, key);
  if (value.empty()) return fallback;
  try { return std::stoi(trim(value)); } catch (...) { return fallback; }
}

float extractFloatField(const std::string &body, const std::string &key, float fallback = 0.0f) {
  const std::string value = extractBlock(body, key);
  if (value.empty()) return fallback;
  try { return std::stof(trim(value)); } catch (...) { return fallback; }
}

bool extractBoolField(const std::string &body, const std::string &key, bool fallback = false) {
  const std::string value = trim(extractBlock(body, key));
  if (value == "true") return true;
  if (value == "false") return false;
  return fallback;
}

glm::vec3 extractVec3Field(const std::string &body, const std::string &key, const glm::vec3 &fallback = {}) {
  const std::string value = extractBlock(body, key);
  if (value.empty() || value.front() != '[') return fallback;
  std::string stripped = value.substr(1, value.size() - 2);
  std::replace(stripped.begin(), stripped.end(), ',', ' ');
  std::istringstream stream(stripped);
  glm::vec3 result = fallback;
  if (!(stream >> result.x >> result.y >> result.z)) return fallback;
  return result;
}

glm::vec4 extractVec4Field(const std::string &body, const std::string &key, const glm::vec4 &fallback = {}) {
  const std::string value = extractBlock(body, key);
  if (value.empty() || value.front() != '[') return fallback;
  std::string stripped = value.substr(1, value.size() - 2);
  std::replace(stripped.begin(), stripped.end(), ',', ' ');
  std::istringstream stream(stripped);
  glm::vec4 result = fallback;
  if (!(stream >> result.x >> result.y >> result.z >> result.w)) return fallback;
  return result;
}

} // namespace

void VehicleDef::createDefault(const std::string &newId) {
  id = newId.empty() ? "war_rig_01" : newId;
  name = "Громобой Постапокалипсиса";
  version = CURRENT_VERSION;
  author = "Mad Mechanic";
  applyChassisPreset(VehicleChassisClass::ArmoredPickup);
}

void VehicleDef::applyChassisPreset(VehicleChassisClass newChassis) {
  chassis = newChassis;
  hardpoints.clear();
  armorPlates.clear();

  switch (newChassis) {
    case VehicleChassisClass::LightBuggy:
      name = "Песчаная Блоха";
      engine = EngineType::V6TwinTurbo;
      wheels = WheelType::RacingSlicks;
      mass = 950.0f;
      maxSpeed = 58.0f;
      acceleration = 24.0f;
      turnRate = 3.2f;
      grip = 1.30f;
      driftFactor = 0.65f;
      maxHealth = 400.0f;
      nitroCapacity = 120.0f;
      ramDamageMultiplier = 0.8f;
      break;

    case VehicleChassisClass::MuscleInterceptor:
      name = "Черный Перехватчик V8";
      engine = EngineType::V8Supercharged;
      wheels = WheelType::OffroadChunky;
      mass = 1450.0f;
      maxSpeed = 52.0f;
      acceleration = 21.0f;
      turnRate = 2.4f;
      grip = 1.15f;
      driftFactor = 0.50f;
      maxHealth = 550.0f;
      nitroCapacity = 100.0f;
      ramDamageMultiplier = 1.3f;
      break;

    case VehicleChassisClass::ArmoredPickup:
      name = "Пустынный Рейдер 4x4";
      engine = EngineType::DieselMonster;
      wheels = WheelType::OffroadChunky;
      mass = 1850.0f;
      maxSpeed = 44.0f;
      acceleration = 16.5f;
      turnRate = 1.9f;
      grip = 1.10f;
      driftFactor = 0.35f;
      maxHealth = 850.0f;
      nitroCapacity = 90.0f;
      ramDamageMultiplier = 1.7f;
      break;

    case VehicleChassisClass::HeavyWarRig:
      name = "Джаггернаут Пустошей";
      engine = EngineType::JetTurbine;
      wheels = WheelType::HeavyTracks;
      mass = 3400.0f;
      maxSpeed = 36.0f;
      acceleration = 12.0f;
      turnRate = 1.3f;
      grip = 1.45f;
      driftFactor = 0.15f;
      maxHealth = 1600.0f;
      nitroCapacity = 150.0f;
      ramDamageMultiplier = 3.2f;
      break;

    case VehicleChassisClass::HoverCraft:
      name = "Плазменный Левитатор";
      engine = EngineType::ElectricHyperdrive;
      wheels = WheelType::HoverRepulsors;
      mass = 1100.0f;
      maxSpeed = 62.0f;
      acceleration = 26.0f;
      turnRate = 3.5f;
      grip = 0.85f;
      driftFactor = 0.90f;
      maxHealth = 480.0f;
      nitroCapacity = 140.0f;
      ramDamageMultiplier = 1.0f;
      break;
  }

  // Add default weapon hardpoints
  WeaponHardpoint roof;
  roof.slotId = "roof_primary";
  roof.slotLabel = "Крыша (Основное)";
  roof.localPosition = glm::vec3(0.0f, 1.35f, -0.2f);
  roof.defaultWeaponId = "minigun_20mm";
  hardpoints.push_back(roof);

  WeaponHardpoint hood;
  hood.slotId = "hood_secondary";
  hood.slotLabel = "Капот (Тяжелое)";
  hood.localPosition = glm::vec3(0.0f, 0.9f, 1.2f);
  hood.defaultWeaponId = "rocket_pod";
  hardpoints.push_back(hood);

  WeaponHardpoint rear;
  rear.slotId = "rear_mines";
  rear.slotLabel = "Задний Сбрасыватель";
  rear.localPosition = glm::vec3(0.0f, 0.4f, -1.4f);
  rear.defaultWeaponId = "mine_layer";
  hardpoints.push_back(rear);

  // Add default ram armor
  VehicleArmorPlate ram;
  ram.name = "Шипованный Отвал";
  ram.zone = "front_ram";
  ram.armorHp = 250.0f;
  ram.ramDamageBonus = 40.0f;
  ram.weight = 120.0f;
  ram.localPosition = glm::vec3(0.0f, 0.4f, 1.8f);
  armorPlates.push_back(ram);

  recalculateStats();
}

void VehicleDef::recalculateStats() {
  float totalWeight = mass;
  float extraHp = 0.0f;
  float extraRam = 0.0f;

  for (const auto& plate : armorPlates) {
    totalWeight += plate.weight;
    extraHp += plate.armorHp;
    extraRam += plate.ramDamageBonus;
  }

  // Performance dynamics based on weight ratio
  float weightRatio = totalWeight / std::max(mass, 500.0f);
  acceleration = std::max(8.0f, acceleration / (weightRatio * 0.9f));
}

std::string VehicleDef::chassisToString(VehicleChassisClass chassis) {
  switch (chassis) {
    case VehicleChassisClass::LightBuggy: return "light_buggy";
    case VehicleChassisClass::MuscleInterceptor: return "muscle_interceptor";
    case VehicleChassisClass::ArmoredPickup: return "armored_pickup";
    case VehicleChassisClass::HeavyWarRig: return "heavy_war_rig";
    case VehicleChassisClass::HoverCraft: return "hover_craft";
  }
  return "armored_pickup";
}

VehicleChassisClass VehicleDef::chassisFromString(const std::string &str) {
  if (str == "light_buggy") return VehicleChassisClass::LightBuggy;
  if (str == "muscle_interceptor") return VehicleChassisClass::MuscleInterceptor;
  if (str == "heavy_war_rig") return VehicleChassisClass::HeavyWarRig;
  if (str == "hover_craft") return VehicleChassisClass::HoverCraft;
  return VehicleChassisClass::ArmoredPickup;
}

std::string VehicleDef::engineToString(EngineType engine) {
  switch (engine) {
    case EngineType::V6TwinTurbo: return "v6_twinturbo";
    case EngineType::V8Supercharged: return "v8_supercharged";
    case EngineType::DieselMonster: return "diesel_monster";
    case EngineType::ElectricHyperdrive: return "electric_hyperdrive";
    case EngineType::JetTurbine: return "jet_turbine";
  }
  return "v8_supercharged";
}

EngineType VehicleDef::engineFromString(const std::string &str) {
  if (str == "v6_twinturbo") return EngineType::V6TwinTurbo;
  if (str == "diesel_monster") return EngineType::DieselMonster;
  if (str == "electric_hyperdrive") return EngineType::ElectricHyperdrive;
  if (str == "jet_turbine") return EngineType::JetTurbine;
  return EngineType::V8Supercharged;
}

std::string VehicleDef::wheelsToString(WheelType wheels) {
  switch (wheels) {
    case WheelType::RacingSlicks: return "racing_slicks";
    case WheelType::OffroadChunky: return "offroad_chunky";
    case WheelType::SpikedTires: return "spiked_tires";
    case WheelType::HeavyTracks: return "heavy_tracks";
    case WheelType::HoverRepulsors: return "hover_repulsors";
  }
  return "offroad_chunky";
}

WheelType VehicleDef::wheelsFromString(const std::string &str) {
  if (str == "racing_slicks") return WheelType::RacingSlicks;
  if (str == "spiked_tires") return WheelType::SpikedTires;
  if (str == "heavy_tracks") return WheelType::HeavyTracks;
  if (str == "hover_repulsors") return WheelType::HoverRepulsors;
  return WheelType::OffroadChunky;
}

bool VehicleDef::saveToFile(const std::string &path) const {
  std::ostringstream out;
  out << "{\n";
  out << "  \"id\": \"" << escapeJson(id) << "\",\n";
  out << "  \"name\": \"" << escapeJson(name) << "\",\n";
  out << "  \"author\": \"" << escapeJson(author) << "\",\n";
  out << "  \"version\": " << version << ",\n";
  out << "  \"chassis\": \"" << chassisToString(chassis) << "\",\n";
  out << "  \"engine\": \"" << engineToString(engine) << "\",\n";
  out << "  \"wheels\": \"" << wheelsToString(wheels) << "\",\n";
  out << "  \"stats\": {\n";
  out << "    \"mass\": " << mass << ",\n";
  out << "    \"max_speed\": " << maxSpeed << ",\n";
  out << "    \"acceleration\": " << acceleration << ",\n";
  out << "    \"turn_rate\": " << turnRate << ",\n";
  out << "    \"grip\": " << grip << ",\n";
  out << "    \"drift_factor\": " << driftFactor << ",\n";
  out << "    \"max_health\": " << maxHealth << ",\n";
  out << "    \"nitro_capacity\": " << nitroCapacity << ",\n";
  out << "    \"ram_damage_multiplier\": " << ramDamageMultiplier << "\n";
  out << "  },\n";
  out << "  \"visuals\": {\n";
  out << "    \"rust_weathering\": " << visuals.rustWeathering << ",\n";
  out << "    \"dirt_mud\": " << visuals.dirtMud << ",\n";
  out << "    \"neon\": " << (visuals.neonUnderglow ? "true" : "false") << "\n";
  out << "  }\n";
  out << "}\n";

  std::filesystem::path filePath(path);
  if (filePath.has_parent_path()) {
    std::filesystem::create_directories(filePath.parent_path());
  }

  std::ofstream file(path);
  if (!file.is_open()) return false;
  file << out.str();
  LOG_INFOF("Saved vehicle preset to %s", path.c_str());
  return true;
}

bool VehicleDef::loadFromFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) return false;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string json = buffer.str();

  id = extractStringField(json, "id");
  name = extractStringField(json, "name");
  author = extractStringField(json, "author");
  chassis = chassisFromString(extractStringField(json, "chassis"));
  engine = engineFromString(extractStringField(json, "engine"));
  wheels = wheelsFromString(extractStringField(json, "wheels"));

  std::string statsBlock = extractBlock(json, "stats");
  if (!statsBlock.empty()) {
    mass = extractFloatField(statsBlock, "mass", mass);
    maxSpeed = extractFloatField(statsBlock, "max_speed", maxSpeed);
    acceleration = extractFloatField(statsBlock, "acceleration", acceleration);
    turnRate = extractFloatField(statsBlock, "turn_rate", turnRate);
    grip = extractFloatField(statsBlock, "grip", grip);
    driftFactor = extractFloatField(statsBlock, "drift_factor", driftFactor);
    maxHealth = extractFloatField(statsBlock, "max_health", maxHealth);
    nitroCapacity = extractFloatField(statsBlock, "nitro_capacity", nitroCapacity);
    ramDamageMultiplier = extractFloatField(statsBlock, "ram_damage_multiplier", ramDamageMultiplier);
  }

  LOG_INFOF("Loaded vehicle preset '%s' from %s", name.c_str(), path.c_str());
  return true;
}

} // namespace Editor
