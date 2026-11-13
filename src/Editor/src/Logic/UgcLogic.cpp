#include "Editor/Logic/UgcLogic.hpp"

namespace Editor {

void UgcGameRulesDef::createDefaultRules() {
    triggerZones.clear();
    rules.clear();
    
    // Default Trigger Zone 1: Center Ambush Zone
    UgcTriggerZone centerZone;
    centerZone.id = "zone_center_ambush";
    centerZone.name = "Центральная Зона Засады";
    centerZone.center = glm::vec3(0.0f, 0.0f, 0.0f);
    centerZone.extents = glm::vec3(25.0f, 8.0f, 25.0f);
    triggerZones.push_back(centerZone);
    
    // Default Rule 1: Announce when entering center
    UgcRule rule1;
    rule1.id = "rule_ambush_alert";
    rule1.name = "Оповещение о бое в центре";
    rule1.eventType = UgcEventType::OnTriggerZoneEnter;
    rule1.sourceTargetId = "zone_center_ambush";
    rule1.actionType = UgcActionType::BroadcastAlert;
    rule1.actionPayload = "ВНИМАНИЕ: Противник вошел в центральную зону!";
    rules.push_back(rule1);
    
    // Default Rule 2: Airdrop on 60 seconds
    UgcRule rule2;
    rule2.id = "rule_airdrop_timer";
    rule2.name = "Периодический сброс припасов";
    rule2.eventType = UgcEventType::OnTimerTick;
    rule2.eventParameter = 60.0f;
    rule2.actionType = UgcActionType::SpawnAirdrop;
    rule2.actionPayload = "military_crate";
    rules.push_back(rule2);
}

std::string UgcGameRulesDef::eventTypeToString(UgcEventType type) {
    switch (type) {
        case UgcEventType::OnMatchStart: return "OnMatchStart";
        case UgcEventType::OnCheckpointPassed: return "OnCheckpointPassed";
        case UgcEventType::OnTriggerZoneEnter: return "OnTriggerZoneEnter";
        case UgcEventType::OnTriggerZoneExit: return "OnTriggerZoneExit";
        case UgcEventType::OnVehicleDestroyed: return "OnVehicleDestroyed";
        case UgcEventType::OnStructureDestroyed: return "OnStructureDestroyed";
        case UgcEventType::OnTimerTick: return "OnTimerTick";
        case UgcEventType::OnAirdropLanded: return "OnAirdropLanded";
        case UgcEventType::OnAirdropCaptured: return "OnAirdropCaptured";
        case UgcEventType::OnCustomSignal: return "OnCustomSignal";
    }
    return "OnMatchStart";
}

UgcEventType UgcGameRulesDef::eventTypeFromString(const std::string& str) {
    if (str == "OnCheckpointPassed") return UgcEventType::OnCheckpointPassed;
    if (str == "OnTriggerZoneEnter") return UgcEventType::OnTriggerZoneEnter;
    if (str == "OnTriggerZoneExit") return UgcEventType::OnTriggerZoneExit;
    if (str == "OnVehicleDestroyed") return UgcEventType::OnVehicleDestroyed;
    if (str == "OnStructureDestroyed") return UgcEventType::OnStructureDestroyed;
    if (str == "OnTimerTick") return UgcEventType::OnTimerTick;
    if (str == "OnAirdropLanded") return UgcEventType::OnAirdropLanded;
    if (str == "OnAirdropCaptured") return UgcEventType::OnAirdropCaptured;
    if (str == "OnCustomSignal") return UgcEventType::OnCustomSignal;
    return UgcEventType::OnMatchStart;
}

std::string UgcGameRulesDef::actionTypeToString(UgcActionType type) {
    switch (type) {
        case UgcActionType::SpawnAirdrop: return "SpawnAirdrop";
        case UgcActionType::SetGateState: return "SetGateState";
        case UgcActionType::SetTurretAlert: return "SetTurretAlert";
        case UgcActionType::TriggerExplosion: return "TriggerExplosion";
        case UgcActionType::GiveReward: return "GiveReward";
        case UgcActionType::TeleportEntity: return "TeleportEntity";
        case UgcActionType::BroadcastAlert: return "BroadcastAlert";
        case UgcActionType::ChangeTrackHazardState: return "ChangeTrackHazardState";
        case UgcActionType::EndGameVictory: return "EndGameVictory";
    }
    return "BroadcastAlert";
}

UgcActionType UgcGameRulesDef::actionTypeFromString(const std::string& str) {
    if (str == "SpawnAirdrop") return UgcActionType::SpawnAirdrop;
    if (str == "SetGateState") return UgcActionType::SetGateState;
    if (str == "SetTurretAlert") return UgcActionType::SetTurretAlert;
    if (str == "TriggerExplosion") return UgcActionType::TriggerExplosion;
    if (str == "GiveReward") return UgcActionType::GiveReward;
    if (str == "TeleportEntity") return UgcActionType::TeleportEntity;
    if (str == "ChangeTrackHazardState") return UgcActionType::ChangeTrackHazardState;
    if (str == "EndGameVictory") return UgcActionType::EndGameVictory;
    return UgcActionType::BroadcastAlert;
}

} // namespace Editor
