#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Editor {

enum class UgcEventType {
    OnMatchStart,
    OnCheckpointPassed,
    OnTriggerZoneEnter,
    OnTriggerZoneExit,
    OnVehicleDestroyed,
    OnStructureDestroyed,
    OnTimerTick,
    OnAirdropLanded,
    OnAirdropCaptured,
    OnCustomSignal
};

enum class UgcActionType {
    SpawnAirdrop,
    SetGateState,
    SetTurretAlert,
    TriggerExplosion,
    GiveReward,
    TeleportEntity,
    BroadcastAlert,
    ChangeTrackHazardState,
    EndGameVictory
};

struct UgcTriggerZone {
    std::string id;
    std::string name;
    glm::vec3 center{0.0f};
    glm::vec3 extents{10.0f, 5.0f, 10.0f};
    bool isSpherical = false;
    float sphereRadius = 10.0f;
    int filterTeam = -1; // -1 = any team
    bool triggerOnce = false;
    bool triggered = false;
};

struct UgcRule {
    std::string id;
    std::string name;
    bool enabled = true;
    
    UgcEventType eventType = UgcEventType::OnTriggerZoneEnter;
    std::string sourceTargetId; // ID триггер-зоны, структуры или чекпоинта
    float eventParameter = 0.0f; // e.g. delay seconds or threshold
    
    UgcActionType actionType = UgcActionType::BroadcastAlert;
    std::string actionTargetId; // ID ворот, спавнера или структуры
    std::string actionPayload;   // Сообщение, тип лута или параметр
    float actionFloatValue = 1.0f;
};

struct UgcGameRulesDef {
    std::string modeName = "Пользовательский Режим";
    std::string author = "Автор Карты";
    std::string description = "Уникальные правила, созданные в UGC Редакторе";
    
    // Core Modifiers
    bool pvpEnabled = true;
    bool vehicleCollisionDamage = true;
    float gravityMultiplier = 1.0f;
    float vehicleSpeedMultiplier = 1.0f;
    float weaponDamageMultiplier = 1.0f;
    float respawnTimeSeconds = 3.0f;
    int maxPlayers = 8;
    
    // Win conditions
    int targetLaps = 3;
    int targetKills = 15;
    int targetScore = 1000;
    float matchTimeLimit = 300.0f; // 5 минут
    
    // Triggers and event graph
    std::vector<UgcTriggerZone> triggerZones;
    std::vector<UgcRule> rules;
    
    void createDefaultRules();
    static std::string eventTypeToString(UgcEventType type);
    static UgcEventType eventTypeFromString(const std::string& str);
    static std::string actionTypeToString(UgcActionType type);
    static UgcActionType actionTypeFromString(const std::string& str);
};

} // namespace Editor
