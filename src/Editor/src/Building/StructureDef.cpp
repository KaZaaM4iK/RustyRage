#include "Editor/Building/StructureDef.hpp"
#include <algorithm>

namespace Editor {

StructureDef StructureDef::createPreset(StructureType type, const glm::vec3& pos) {
    StructureDef def;
    def.position = pos;
    def.type = type;
    
    switch (type) {
        case StructureType::FortifiedWall:
            def.name = "Бронированная Стена";
            def.category = StructureCategory::Defense;
            def.maxHealth = 1200.0f;
            def.currentHealth = 1200.0f;
            def.armorRating = 45.0f;
            def.boundingBoxSize = glm::vec3(6.0f, 4.0f, 1.5f);
            def.scrapCost = 75;
            def.energyDrain = 0;
            def.tintColor = glm::vec4(0.45f, 0.45f, 0.50f, 1.0f);
            break;
            
        case StructureType::BlastGate:
            def.name = "Тяжелые Гермоворота";
            def.category = StructureCategory::Defense;
            def.maxHealth = 2000.0f;
            def.currentHealth = 2000.0f;
            def.armorRating = 50.0f;
            def.boundingBoxSize = glm::vec3(8.0f, 4.5f, 1.8f);
            def.scrapCost = 250;
            def.energyDrain = 10;
            def.tintColor = glm::vec4(0.70f, 0.55f, 0.20f, 1.0f);
            break;
            
        case StructureType::AutoMinigunTurret:
            def.name = "Авто-пулемет 20мм";
            def.category = StructureCategory::Turret;
            def.maxHealth = 650.0f;
            def.currentHealth = 650.0f;
            def.armorRating = 20.0f;
            def.attackRange = 45.0f;
            def.damagePerSecond = 85.0f;
            def.fireRate = 15.0f;
            def.rotationSpeed = 4.2f;
            def.ammoCapacity = 800;
            def.currentAmmo = 800;
            def.scrapCost = 350;
            def.energyDrain = 15;
            def.boundingBoxSize = glm::vec3(2.5f, 3.5f, 2.5f);
            def.tintColor = glm::vec4(0.85f, 0.25f, 0.25f, 1.0f);
            break;
            
        case StructureType::SAMRocketPod:
            def.name = "Ракетная Установка ПВО";
            def.category = StructureCategory::Turret;
            def.maxHealth = 500.0f;
            def.currentHealth = 500.0f;
            def.armorRating = 15.0f;
            def.attackRange = 75.0f;
            def.damagePerSecond = 140.0f;
            def.fireRate = 1.2f;
            def.ammoType = "Rockets";
            def.ammoCapacity = 24;
            def.currentAmmo = 24;
            def.scrapCost = 500;
            def.energyDrain = 25;
            def.boundingBoxSize = glm::vec3(3.0f, 4.0f, 3.0f);
            def.tintColor = glm::vec4(0.95f, 0.40f, 0.10f, 1.0f);
            break;
            
        case StructureType::RepairBay:
            def.name = "Ремонтная Эстакада";
            def.category = StructureCategory::Utility;
            def.maxHealth = 800.0f;
            def.currentHealth = 800.0f;
            def.isPassable = true;
            def.scrapCost = 400;
            def.energyDrain = 20;
            def.boundingBoxSize = glm::vec3(6.0f, 1.0f, 6.0f);
            def.tintColor = glm::vec4(0.20f, 0.75f, 0.90f, 1.0f);
            break;
            
        case StructureType::PowerGenerator:
            def.name = "Дизель-Генератор 50кВт";
            def.category = StructureCategory::Industrial;
            def.maxHealth = 900.0f;
            def.currentHealth = 900.0f;
            def.energyDrain = -50; // Генерирует 50 энергии!
            def.scrapCost = 300;
            def.boundingBoxSize = glm::vec3(4.0f, 3.0f, 4.0f);
            def.tintColor = glm::vec4(0.30f, 0.85f, 0.35f, 1.0f);
            break;
            
        case StructureType::AirdropRadioBeacon:
            def.name = "Радиомаяк Сброса Аирдропа";
            def.category = StructureCategory::Industrial;
            def.maxHealth = 400.0f;
            def.currentHealth = 400.0f;
            def.scrapCost = 600;
            def.energyDrain = 35;
            def.boundingBoxSize = glm::vec3(2.0f, 6.0f, 2.0f);
            def.tintColor = glm::vec4(0.90f, 0.85f, 0.15f, 1.0f);
            break;
            
        default:
            def.name = "Базовое Строение";
            def.category = StructureCategory::Defense;
            def.boundingBoxSize = glm::vec3(3.0f, 3.0f, 3.0f);
            break;
    }
    
    return def;
}

std::string StructureDef::categoryToString(StructureCategory cat) {
    switch (cat) {
        case StructureCategory::Defense: return "Defense";
        case StructureCategory::Turret: return "Turret";
        case StructureCategory::Utility: return "Utility";
        case StructureCategory::Industrial: return "Industrial";
        case StructureCategory::Trap: return "Trap";
    }
    return "Defense";
}

std::string StructureDef::typeToString(StructureType type) {
    switch (type) {
        case StructureType::FortifiedWall: return "FortifiedWall";
        case StructureType::BlastGate: return "BlastGate";
        case StructureType::ReinforcedBarricade: return "ReinforcedBarricade";
        case StructureType::WatchTower: return "WatchTower";
        case StructureType::AutoMinigunTurret: return "AutoMinigunTurret";
        case StructureType::HeavyPlasmaTurret: return "HeavyPlasmaTurret";
        case StructureType::SAMRocketPod: return "SAMRocketPod";
        case StructureType::FlamethrowerTower: return "FlamethrowerTower";
        case StructureType::TeslaShockPylon: return "TeslaShockPylon";
        case StructureType::RadarBeacon: return "RadarBeacon";
        case StructureType::RepairBay: return "RepairBay";
        case StructureType::NitroRefuelStation: return "NitroRefuelStation";
        case StructureType::ScrapSynthesizer: return "ScrapSynthesizer";
        case StructureType::LaunchPad: return "LaunchPad";
        case StructureType::PowerGenerator: return "PowerGenerator";
        case StructureType::FuelSilo: return "FuelSilo";
        case StructureType::WorkbenchStation: return "WorkbenchStation";
        case StructureType::AirdropRadioBeacon: return "AirdropRadioBeacon";
        case StructureType::ProximityMinefield: return "ProximityMinefield";
        case StructureType::SpikeStrip: return "SpikeStrip";
        case StructureType::FireTrap: return "FireTrap";
    }
    return "FortifiedWall";
}

StructureType StructureDef::typeFromString(const std::string& str) {
    if (str == "BlastGate") return StructureType::BlastGate;
    if (str == "ReinforcedBarricade") return StructureType::ReinforcedBarricade;
    if (str == "WatchTower") return StructureType::WatchTower;
    if (str == "AutoMinigunTurret") return StructureType::AutoMinigunTurret;
    if (str == "HeavyPlasmaTurret") return StructureType::HeavyPlasmaTurret;
    if (str == "SAMRocketPod") return StructureType::SAMRocketPod;
    if (str == "FlamethrowerTower") return StructureType::FlamethrowerTower;
    if (str == "TeslaShockPylon") return StructureType::TeslaShockPylon;
    if (str == "RadarBeacon") return StructureType::RadarBeacon;
    if (str == "RepairBay") return StructureType::RepairBay;
    if (str == "NitroRefuelStation") return StructureType::NitroRefuelStation;
    if (str == "ScrapSynthesizer") return StructureType::ScrapSynthesizer;
    if (str == "LaunchPad") return StructureType::LaunchPad;
    if (str == "PowerGenerator") return StructureType::PowerGenerator;
    if (str == "FuelSilo") return StructureType::FuelSilo;
    if (str == "WorkbenchStation") return StructureType::WorkbenchStation;
    if (str == "AirdropRadioBeacon") return StructureType::AirdropRadioBeacon;
    if (str == "ProximityMinefield") return StructureType::ProximityMinefield;
    if (str == "SpikeStrip") return StructureType::SpikeStrip;
    if (str == "FireTrap") return StructureType::FireTrap;
    return StructureType::FortifiedWall;
}

} // namespace Editor
