#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace Editor {

enum class StructureCategory {
    Defense,      // Стены, ворота, баррикады
    Turret,       // Авто-турели, ПВО, огнеметы, пулеметные вышки
    Utility,      // Радарные маяки, станции ремонта, ускорители, заправки
    Industrial,   // Генераторы, очистные вышки, склады металла, верстаки
    Trap          // Нажимные мины, огненные ловушки, шипы
};

enum class StructureType {
    // Defense
    FortifiedWall,
    BlastGate,
    ReinforcedBarricade,
    WatchTower,
    
    // Turret
    AutoMinigunTurret,
    HeavyPlasmaTurret,
    SAMRocketPod,
    FlamethrowerTower,
    TeslaShockPylon,
    
    // Utility
    RadarBeacon,
    RepairBay,
    NitroRefuelStation,
    ScrapSynthesizer,
    LaunchPad,
    
    // Industrial
    PowerGenerator,
    FuelSilo,
    WorkbenchStation,
    AirdropRadioBeacon,
    
    // Traps
    ProximityMinefield,
    SpikeStrip,
    FireTrap
};

struct StructureDef {
    std::string id;
    std::string name;
    StructureCategory category = StructureCategory::Defense;
    StructureType type = StructureType::FortifiedWall;
    
    // 3D Transform & Physics
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 boundingBoxSize{4.0f, 3.0f, 4.0f};
    
    // Combat & Survivability stats
    float maxHealth = 500.0f;
    float currentHealth = 500.0f;
    float armorRating = 25.0f;        // Уменьшение урона в %
    bool isDestructible = true;
    bool isPassable = false;          // Для открытых ворот и зон ремонта
    
    // Turret / Offensive properties
    float attackRange = 35.0f;
    float damagePerSecond = 45.0f;
    float fireRate = 8.0f;            // Выстрелов в секунду
    float rotationSpeed = 3.14f;      // Рад/сек слежения за целью
    std::string ammoType = "7.62mm";
    int ammoCapacity = 500;
    int currentAmmo = 500;
    
    // Economy / Base requirements
    int scrapCost = 100;
    int energyDrain = 5;              // Потребление энергии генератора
    int teamId = 0;                   // 0 = Нейтрал, 1-8 = Банды / Игроки
    
    // Visual & Assets
    std::string modelPath;
    std::string texturePath;
    glm::vec4 tintColor{1.0f, 1.0f, 1.0f, 1.0f};
    
    // UGC Logic Tag for event triggers
    std::string triggerTag;
    
    StructureDef() = default;
    
    static StructureDef createPreset(StructureType type, const glm::vec3& pos = glm::vec3(0.0f));
    static std::string categoryToString(StructureCategory cat);
    static std::string typeToString(StructureType type);
    static StructureType typeFromString(const std::string& str);
};

} // namespace Editor
