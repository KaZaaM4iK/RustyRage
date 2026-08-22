#pragma once

#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Game {

/**
 * Game modes implementation for Rusty Rage
 * Handles Speed Arena and Road Avengers game logic
 */
class GameModes {
public:
    enum class Mode {
        SpeedArena,      // Арена Скорости - круговые гонки с оружием
        RoadAvengers     // Мстители Дороги - выживание с разрушением
    };
    
    enum class PlayType {
        Solo,           // Одиночная игра
        Multi2,         // Мультиплеер 2 игрока
        Multi4          // Мультиплеер 4 игрока
    };
    
    struct GameSettings {
        Mode mode;
        PlayType playType;
        std::string mapName;
        float timeLimit;        // Default 10 minutes
        int maxPlayers;         // Always 8 except custom games
        bool weaponsEnabled;
        bool powerUpsEnabled;
        bool destructionEnabled;
        bool respawnEnabled;
        float respawnTime;      // 2 seconds default
        float antiCampTime;     // 3 seconds default
    };
    
    struct PlayerStats {
        uint32_t playerId;
        int position;
        int lapsCompleted;
        float raceTime;
        int kills;
        int deaths;
        bool isAlive;
        bool hasFinished;
        float respawnTimer;
        float antiCampTimer;
        glm::vec3 lastPosition;
    };
    
    struct SpawnPoint {
        glm::vec3 position;
        glm::quat rotation;
        bool isOccupied;
        float occupationTime;
    };

    GameModes();
    ~GameModes();

    // Game mode management
    void setMode(Mode mode) { m_currentMode = mode; }
    Mode getMode() const { return m_currentMode; }
    
    void setPlayType(PlayType type) { m_currentPlayType = type; }
    PlayType getPlayType() const { return m_currentPlayType; }
    
    void setSettings(const GameSettings& settings) { m_settings = settings; }
    const GameSettings& getSettings() const { return m_settings; }
    
    // Game initialization
    bool initializeGame();
    void startGame();
    void endGame();
    void resetGame();
    
    // Game loop
    void update(float deltaTime);
    bool isGameActive() const { return m_gameActive; }
    bool isGameEnded() const { return m_gameEnded; }
    
    // Player management
    void addPlayer(uint32_t playerId, const std::string& name);
    void removePlayer(uint32_t playerId);
    void updatePlayerStats(uint32_t playerId, const PlayerStats& stats);
    PlayerStats* getPlayerStats(uint32_t playerId);
    const std::vector<PlayerStats>& getAllPlayerStats() const { return m_playerStats; }
    
    // Spawn system
    void setSpawnPoints(const std::vector<SpawnPoint>& points);
    glm::vec3 getSpawnPosition(uint32_t playerId);
    void freeSpawnPoint(const glm::vec3& position);
    
    // Speed Arena specific
    void updateSpeedArena(float deltaTime);
    void checkLapCompletion(uint32_t playerId);
    void handlePowerUp(uint32_t playerId, int powerUpType);
    void updateRacePositions();
    
    // Road Avengers specific
    void updateRoadAvengers(float deltaTime);
    void handlePlayerDeath(uint32_t playerId);
    void handlePlayerKill(uint32_t killerId, uint32_t victimId);
    void updateSurvivalTimer(float deltaTime);
    
    // Scoring system
    struct ScoreResult {
        uint32_t playerId;
        int position;
        int points;
        bool isGroup;
        std::vector<uint32_t> groupMembers;
    };
    
    std::vector<ScoreResult> calculateScores() const;
    int getPointsForPosition(int position, PlayType playType) const;
    
    // Timer management
    void setGameTimer(float time) { m_gameTimer = time; }
    float getGameTimer() const { return m_gameTimer; }
    float getTimeRemaining() const;
    bool isTimeUp() const;
    
    // Respawn system with anti-camping
    void respawnPlayer(uint32_t playerId);
    bool canPlayerAttack(uint32_t playerId) const;
    bool canPlayerBeAttacked(uint32_t playerId) const;
    void updateRespawnTimers(float deltaTime);
    
    // Game state queries
    int getPlayerCount() const { return m_playerStats.size(); }
    int getAlivePlayerCount() const;
    int getFinishedPlayerCount() const;
    std::vector<uint32_t> getAlivePlayers() const;
    std::vector<uint32_t> getFinishedPlayers() const;
    
    // Map and environment
    void setMapName(const std::string& mapName) { m_mapName = mapName; }
    const std::string& getMapName() const { return m_mapName; }
    
    // Power-ups and weapons
    void spawnPowerUp(const glm::vec3& position, int type);
    void collectPowerUp(uint32_t playerId, int powerUpType);
    bool canUseWeapon(uint32_t playerId) const;
    bool canUsePowerUp(uint32_t playerId) const;

private:
    Mode m_currentMode;
    PlayType m_currentPlayType;
    GameSettings m_settings;
    
    // Game state
    bool m_gameActive;
    bool m_gameEnded;
    float m_gameTimer;
    float m_maxGameTime;
    std::string m_mapName;
    
    // Players
    std::vector<PlayerStats> m_playerStats;
    std::map<uint32_t, size_t> m_playerIndexMap; // playerId -> index in m_playerStats
    
    // Spawn system
    std::vector<SpawnPoint> m_spawnPoints;
    std::map<uint32_t, glm::vec3> m_playerSpawnPositions;
    
    // Speed Arena specific
    struct Checkpoint {
        glm::vec3 position;
        float radius;
        bool isFinishLine;
    };
    std::vector<Checkpoint> m_checkpoints;
    std::map<uint32_t, int> m_playerCheckpoints; // playerId -> last checkpoint
    
    // Road Avengers specific
    float m_survivalTimer;
    float m_survivalDamageInterval; // 3 seconds default
    float m_survivalDamageAmount;   // -1 HP default
    
    // Power-ups and weapons
    struct PowerUp {
        glm::vec3 position;
        int type;
        float lifetime;
        bool isActive;
    };
    std::vector<PowerUp> m_powerUps;
    
    // Helper methods
    void initializeScoreTable();
    void updateGameTimer(float deltaTime);
    void processGameEnd();
    glm::vec3 findBestSpawnPoint(uint32_t playerId) const;
    bool isPositionOccupied(const glm::vec3& position, float radius = 5.0f) const;
    void updateSpawnPointOccupancy(float deltaTime);
    
    // Mode-specific initialization
    void initializeSpeedArena();
    void initializeRoadAvengers();
    
    // Scoring tables
    std::map<int, int> m_soloScoreTable;
    std::map<int, int> m_multi2ScoreTable;
    std::map<int, int> m_multi4ScoreTable;
};

} // namespace Game
