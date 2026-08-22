#include "Game/GameModes.hpp"
#include "Core/Logging.hpp"
#include <algorithm>

namespace Game {

GameModes::GameModes() 
    : m_currentMode(Mode::SpeedArena),
      m_currentPlayType(PlayType::Solo),
      m_gameActive(false),
      m_gameEnded(false),
      m_gameTimer(0.0f),
      m_maxGameTime(600.0f),
      m_survivalTimer(0.0f),
      m_survivalDamageInterval(3.0f),
      m_survivalDamageAmount(1.0f) {
    
    initializeScoreTable();
}

GameModes::~GameModes() {
}

bool GameModes::initializeGame() {
    LOG_INFOF("Initializing game: mode=%d, playType=%d", (int)m_currentMode, (int)m_currentPlayType);
    
    m_gameActive = true;
    m_gameEnded = false;
    m_gameTimer = 0.0f;
    
    if (m_currentMode == Mode::SpeedArena) {
        initializeSpeedArena();
    } else {
        initializeRoadAvengers();
    }
    
    return true;
}

void GameModes::startGame() {
    LOG_INFO("Starting game");
    m_gameActive = true;
    m_gameEnded = false;
    m_gameTimer = 0.0f;
}

void GameModes::endGame() {
    LOG_INFO("Ending game");
    m_gameActive = false;
    m_gameEnded = true;
}

void GameModes::resetGame() {
    LOG_INFO("Resetting game");
    m_gameActive = false;
    m_gameEnded = false;
    m_gameTimer = 0.0f;
    m_playerStats.clear();
    m_playerIndexMap.clear();
    m_powerUps.clear();
}

void GameModes::update(float deltaTime) {
    if (!m_gameActive || m_gameEnded) {
        return;
    }
    
    updateGameTimer(deltaTime);
    updateRespawnTimers(deltaTime);
    updateSpawnPointOccupancy(deltaTime);
    
    if (m_currentMode == Mode::SpeedArena) {
        updateSpeedArena(deltaTime);
    } else {
        updateRoadAvengers(deltaTime);
    }
    
    if (isTimeUp()) {
        processGameEnd();
    }
}

void GameModes::addPlayer(uint32_t playerId, const std::string& name) {
    PlayerStats stats;
    stats.playerId = playerId;
    stats.position = 0;
    stats.lapsCompleted = 0;
    stats.raceTime = 0.0f;
    stats.kills = 0;
    stats.deaths = 0;
    stats.isAlive = true;
    stats.hasFinished = false;
    stats.respawnTimer = 0.0f;
    stats.antiCampTimer = 0.0f;
    stats.lastPosition = glm::vec3(0.0f);
    
    m_playerStats.push_back(stats);
    m_playerIndexMap[playerId] = m_playerStats.size() - 1;
    
    LOG_INFOF("Added player %u to game", playerId);
}

void GameModes::removePlayer(uint32_t playerId) {
    auto it = m_playerIndexMap.find(playerId);
    if (it != m_playerIndexMap.end()) {
        size_t index = it->second;
        m_playerStats.erase(m_playerStats.begin() + index);
        m_playerIndexMap.erase(it);
        
        // Update indices
        for (auto& pair : m_playerIndexMap) {
            if (pair.second > index) {
                pair.second--;
            }
        }
        
        LOG_INFOF("Removed player %u from game", playerId);
    }
}

void GameModes::updatePlayerStats(uint32_t playerId, const PlayerStats& stats) {
    auto it = m_playerIndexMap.find(playerId);
    if (it != m_playerIndexMap.end()) {
        m_playerStats[it->second] = stats;
    }
}

// PlayerStats* GameModes::getPlayerStats(uint32_t playerId) {
//     auto it = m_playerIndexMap.find(playerId);
//     if (it != m_playerIndexMap.end()) {
//         return &m_playerStats[it->second];
//     }
//     return nullptr;
// }

void GameModes::setSpawnPoints(const std::vector<SpawnPoint>& points) {
    m_spawnPoints = points;
    LOG_INFOF("Set %zu spawn points", points.size());
}

glm::vec3 GameModes::getSpawnPosition(uint32_t playerId) {
    return findBestSpawnPoint(playerId);
}

void GameModes::freeSpawnPoint(const glm::vec3& position) {
    for (auto& point : m_spawnPoints) {
        if (glm::distance(point.position, position) < 1.0f) {
            point.isOccupied = false;
            point.occupationTime = 0.0f;
            break;
        }
    }
}

void GameModes::updateSpeedArena(float deltaTime) {
    // Update race positions
    updateRacePositions();
    
    // Update power-ups
    for (auto& powerUp : m_powerUps) {
        if (powerUp.isActive) {
            powerUp.lifetime -= deltaTime;
            if (powerUp.lifetime <= 0.0f) {
                powerUp.isActive = false;
            }
        }
    }
}

void GameModes::updateRoadAvengers(float deltaTime) {
    updateSurvivalTimer(deltaTime);
}

void GameModes::checkLapCompletion(uint32_t playerId) {
    // TODO: Implement lap completion checking
}

void GameModes::handlePowerUp(uint32_t playerId, int powerUpType) {
    // TODO: Implement power-up handling
}

void GameModes::updateRacePositions() {
    // TODO: Implement race position updates
}

void GameModes::handlePlayerDeath(uint32_t playerId) {
    PlayerStats* stats = getPlayerStats(playerId);
    if (stats) {
        stats->isAlive = false;
        stats->deaths++;
        LOG_INFOF("Player %u died", playerId);
    }
}

void GameModes::handlePlayerKill(uint32_t killerId, uint32_t victimId) {
    PlayerStats* killerStats = getPlayerStats(killerId);
    PlayerStats* victimStats = getPlayerStats(victimId);
    
    if (killerStats) {
        killerStats->kills++;
    }
    
    if (victimStats) {
        victimStats->deaths++;
        victimStats->isAlive = false;
    }
    
    LOG_INFOF("Player %u killed player %u", killerId, victimId);
}

void GameModes::updateSurvivalTimer(float deltaTime) {
    m_survivalTimer += deltaTime;
    
    if (m_survivalTimer >= m_survivalDamageInterval) {
        m_survivalTimer = 0.0f;
        
        // Apply damage to all alive players
        for (auto& stats : m_playerStats) {
            if (stats.isAlive) {
                // TODO: Apply damage to player
                LOG_DEBUGF("Applied survival damage to player %u", stats.playerId);
            }
        }
    }
}

std::vector<GameModes::ScoreResult> GameModes::calculateScores() const {
    std::vector<ScoreResult> results;
    
    // Sort players by position
    std::vector<PlayerStats> sortedStats = m_playerStats;
    std::sort(sortedStats.begin(), sortedStats.end(),
              [](const PlayerStats& a, const PlayerStats& b) {
                  return a.position < b.position;
              });
    
    // Calculate scores
    for (const auto& stats : sortedStats) {
        ScoreResult result;
        result.playerId = stats.playerId;
        result.position = stats.position;
        result.points = getPointsForPosition(stats.position, m_currentPlayType);
        result.isGroup = false;
        
        results.push_back(result);
    }
    
    return results;
}

int GameModes::getPointsForPosition(int position, PlayType playType) const {
    switch (playType) {
        case PlayType::Solo: {
            auto it = m_soloScoreTable.find(position);
            return (it != m_soloScoreTable.end()) ? it->second : 0;
        }
        case PlayType::Multi2: {
            auto it = m_multi2ScoreTable.find(position);
            return (it != m_multi2ScoreTable.end()) ? it->second : 0;
        }
        case PlayType::Multi4: {
            auto it = m_multi4ScoreTable.find(position);
            return (it != m_multi4ScoreTable.end()) ? it->second : 0;
        }
        default:
            return 0;
    }
}

// setGameTimer and getGameTimer are inline in header

float GameModes::getTimeRemaining() const {
    return std::max(0.0f, m_maxGameTime - m_gameTimer);
}

bool GameModes::isTimeUp() const {
    return m_gameTimer >= m_maxGameTime;
}

void GameModes::respawnPlayer(uint32_t playerId) {
    PlayerStats* stats = getPlayerStats(playerId);
    if (stats) {
        stats->isAlive = true;
        stats->respawnTimer = 2.0f; // 2 seconds respawn time
        stats->antiCampTimer = 3.0f; // 3 seconds anti-camp
        LOG_INFOF("Respawning player %u", playerId);
    }
}

bool GameModes::canPlayerAttack(uint32_t playerId) const {
    const PlayerStats* stats = const_cast<GameModes*>(this)->getPlayerStats(playerId);
    if (!stats) return false;
    
    return stats->isAlive && stats->antiCampTimer <= 0.0f;
}

bool GameModes::canPlayerBeAttacked(uint32_t playerId) const {
    const PlayerStats* stats = const_cast<GameModes*>(this)->getPlayerStats(playerId);
    if (!stats) return false;
    
    return stats->isAlive && stats->antiCampTimer <= 0.0f;
}

void GameModes::updateRespawnTimers(float deltaTime) {
    for (auto& stats : m_playerStats) {
        if (stats.respawnTimer > 0.0f) {
            stats.respawnTimer -= deltaTime;
            if (stats.respawnTimer <= 0.0f) {
                respawnPlayer(stats.playerId);
            }
        }
        
        if (stats.antiCampTimer > 0.0f) {
            stats.antiCampTimer -= deltaTime;
        }
    }
}

int GameModes::getAlivePlayerCount() const {
    int count = 0;
    for (const auto& stats : m_playerStats) {
        if (stats.isAlive) {
            count++;
        }
    }
    return count;
}

int GameModes::getFinishedPlayerCount() const {
    int count = 0;
    for (const auto& stats : m_playerStats) {
        if (stats.hasFinished) {
            count++;
        }
    }
    return count;
}

std::vector<uint32_t> GameModes::getAlivePlayers() const {
    std::vector<uint32_t> alive;
    for (const auto& stats : m_playerStats) {
        if (stats.isAlive) {
            alive.push_back(stats.playerId);
        }
    }
    return alive;
}

std::vector<uint32_t> GameModes::getFinishedPlayers() const {
    std::vector<uint32_t> finished;
    for (const auto& stats : m_playerStats) {
        if (stats.hasFinished) {
            finished.push_back(stats.playerId);
        }
    }
    return finished;
}

void GameModes::spawnPowerUp(const glm::vec3& position, int type) {
    PowerUp powerUp;
    powerUp.position = position;
    powerUp.type = type;
    powerUp.lifetime = 30.0f; // 30 seconds
    powerUp.isActive = true;
    
    m_powerUps.push_back(powerUp);
}

void GameModes::collectPowerUp(uint32_t playerId, int powerUpType) {
    // TODO: Implement power-up collection
    LOG_INFOF("Player %u collected power-up type %d", playerId, powerUpType);
}

bool GameModes::canUseWeapon(uint32_t playerId) const {
    return canPlayerAttack(playerId);
}

bool GameModes::canUsePowerUp(uint32_t playerId) const {
    return canPlayerAttack(playerId);
}

void GameModes::initializeScoreTable() {
    // Solo scoring (both modes)
    m_soloScoreTable[1] = 25;   // 1st place
    m_soloScoreTable[2] = 17;   // 2nd place
    m_soloScoreTable[3] = 10;   // 3rd place
    m_soloScoreTable[4] = 5;    // 4th place
    m_soloScoreTable[5] = -4;   // 5th place
    m_soloScoreTable[6] = -8;   // 6th place
    m_soloScoreTable[7] = -14;  // 7th place
    m_soloScoreTable[8] = -22;  // 8th place
    
    // Multi 2-player scoring
    m_multi2ScoreTable[1] = 20;  // 1st place
    m_multi2ScoreTable[2] = 10;  // 2nd place
    m_multi2ScoreTable[3] = -8;  // 3rd place
    m_multi2ScoreTable[4] = -17; // 4th place
    
    // Multi 4-player scoring
    m_multi4ScoreTable[1] = 20;  // 1st place
    m_multi4ScoreTable[2] = -15; // 2nd place
}

void GameModes::updateGameTimer(float deltaTime) {
    m_gameTimer += deltaTime;
}

void GameModes::processGameEnd() {
    endGame();
}

glm::vec3 GameModes::findBestSpawnPoint(uint32_t playerId) const {
    // Find the first available spawn point
    for (const auto& point : m_spawnPoints) {
        if (!point.isOccupied) {
            return point.position;
        }
    }
    
    // If all occupied, return the first one
    if (!m_spawnPoints.empty()) {
        return m_spawnPoints[0].position;
    }
    
    // Default spawn position
    return glm::vec3(0.0f, 0.0f, 0.0f);
}

bool GameModes::isPositionOccupied(const glm::vec3& position, float radius) const {
    for (const auto& point : m_spawnPoints) {
        if (point.isOccupied && glm::distance(point.position, position) < radius) {
            return true;
        }
    }
    return false;
}

void GameModes::updateSpawnPointOccupancy(float deltaTime) {
    for (auto& point : m_spawnPoints) {
        if (point.isOccupied) {
            point.occupationTime += deltaTime;
            // Free spawn point after some time
            if (point.occupationTime > 10.0f) {
                point.isOccupied = false;
                point.occupationTime = 0.0f;
            }
        }
    }
}

void GameModes::initializeSpeedArena() {
    LOG_INFO("Initializing Speed Arena mode");
    // TODO: Initialize Speed Arena specific systems
}

void GameModes::initializeRoadAvengers() {
    LOG_INFO("Initializing Road Avengers mode");
    // TODO: Initialize Road Avengers specific systems
}

} // namespace Game
