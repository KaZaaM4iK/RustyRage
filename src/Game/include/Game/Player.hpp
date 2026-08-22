#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <vector>

namespace Game {

/**
 * Player class for Rusty Rage
 * Handles player data, statistics, and matchmaking information
 */
class Player {
public:
    enum class Region {
        Russia,     // RU
        China       // CN
    };
    
    enum class Status {
        Online,
        InGame,
        InLobby,
        Offline,
        Banned
    };
    
    struct Statistics {
        int totalGames;
        int wins;
        int losses;
        int kills;
        int deaths;
        float winRate;
        int currentRating;
        int highestRating;
        int totalPlayTime; // in minutes
        int speedArenaGames;
        int roadAvengersGames;
        int customGames;
        
        Statistics() : totalGames(0), wins(0), losses(0), kills(0), deaths(0),
                      winRate(0.0f), currentRating(1000), highestRating(1000),
                      totalPlayTime(0), speedArenaGames(0), roadAvengersGames(0), customGames(0) {}
    };
    
    struct MatchmakingData {
        Region preferredRegion;
        int averagePing;
        bool autoRegion;
        std::string currentGameMode;
        bool isSearching;
        float searchTime;
        
        MatchmakingData() : preferredRegion(Region::Russia), averagePing(0),
                           autoRegion(true), isSearching(false), searchTime(0.0f) {}
    };
    
    struct PenaltySystem {
        int incompleteGames;        // Счётчик незавершённых гонок
        int maxIncompleteGames;     // Максимум в окне (20 игр)
        float blockTime;            // Время блокировки в секундах
        bool isBlocked;             // Заблокирован ли игрок
        float blockRemainingTime;   // Оставшееся время блокировки
        
        PenaltySystem() : incompleteGames(0), maxIncompleteGames(20),
                         blockTime(0.0f), isBlocked(false), blockRemainingTime(0.0f) {}
    };

    Player();
    Player(uint32_t id, const std::string& name, const std::string& displayName);
    ~Player();

    // Basic properties
    uint32_t getId() const { return m_id; }
    const std::string& getName() const { return m_name; }
    const std::string& getDisplayName() const { return m_displayName; }
    
    void setName(const std::string& name) { m_name = name; }
    void setDisplayName(const std::string& displayName) { m_displayName = displayName; }
    
    // Status and region
    Status getStatus() const { return m_status; }
    void setStatus(Status status) { m_status = status; }
    
    Region getRegion() const { return m_region; }
    void setRegion(Region region) { m_region = region; }
    
    // Statistics
    const Statistics& getStatistics() const { return m_statistics; }
    Statistics& getStatistics() { return m_statistics; }
    void updateStatistics();
    
    // Matchmaking
    const MatchmakingData& getMatchmakingData() const { return m_matchmakingData; }
    MatchmakingData& getMatchmakingData() { return m_matchmakingData; }
    
    // Penalty system
    const PenaltySystem& getPenaltySystem() const { return m_penaltySystem; }
    PenaltySystem& getPenaltySystem() { return m_penaltySystem; }
    
    // Penalty management
    void addIncompleteGame();
    void resetIncompleteGames();
    void updatePenaltySystem(float deltaTime);
    bool canPlay() const;
    float getBlockRemainingTime() const;
    
    // Rating system (ELO-based)
    void updateRating(int pointsChange);
    int getRating() const { return m_statistics.currentRating; }
    int getHighestRating() const { return m_statistics.highestRating; }
    
    // Game session data
    void setCurrentGameId(const std::string& gameId) { m_currentGameId = gameId; }
    const std::string& getCurrentGameId() const { return m_currentGameId; }
    
    void setCurrentTeamId(uint32_t teamId) { m_currentTeamId = teamId; }
    uint32_t getCurrentTeamId() const { return m_currentTeamId; }
    
    // Network information
    void setPing(int ping) { m_ping = ping; }
    int getPing() const { return m_ping; }
    
    void setLastSeen(float timestamp) { m_lastSeen = timestamp; }
    float getLastSeen() const { return m_lastSeen; }
    
    // Position and movement (for matchmaking compatibility)
    void setPosition(const glm::vec3& position) { m_position = position; }
    const glm::vec3& getPosition() const { return m_position; }
    
    void setRotation(const glm::quat& rotation) { m_rotation = rotation; }
    const glm::quat& getRotation() const { return m_rotation; }
    
    void setVelocity(const glm::vec3& velocity) { m_velocity = velocity; }
    const glm::vec3& getVelocity() const { return m_velocity; }
    
    void setHealth(float health) { m_health = health; }
    float getHealth() const { return m_health; }
    
    void setScore(int score) { m_score = score; }
    int getScore() const { return m_score; }
    
    void setAlive(bool alive) { m_isAlive = alive; }
    bool isAlive() const { return m_isAlive; }
    
    void setReady(bool ready) { m_isReady = ready; }
    bool isReady() const { return m_isReady; }
    
    // Validation
    bool isValid() const;
    bool isOnline() const { return m_status == Status::Online || m_status == Status::InGame || m_status == Status::InLobby; }
    
    // Serialization for network
    std::string serialize() const;
    bool deserialize(const std::string& data);
    
    // Comparison operators
    bool operator==(const Player& other) const { return m_id == other.m_id; }
    bool operator!=(const Player& other) const { return m_id != other.m_id; }
    bool operator<(const Player& other) const { return m_statistics.currentRating < other.m_statistics.currentRating; }

private:
    uint32_t m_id;
    std::string m_name;           // Unique username
    std::string m_displayName;    // Display name in game
    
    Status m_status;
    Region m_region;
    
    Statistics m_statistics;
    MatchmakingData m_matchmakingData;
    PenaltySystem m_penaltySystem;
    
    // Current session
    std::string m_currentGameId;
    uint32_t m_currentTeamId;
    
    // Network
    int m_ping;
    float m_lastSeen;
    
    // Position and movement
    glm::vec3 m_position;
    glm::quat m_rotation;
    glm::vec3 m_velocity;
    float m_health;
    int m_score;
    bool m_isAlive;
    bool m_isReady;
    
    // Helper methods
    void calculateWinRate();
    void updateHighestRating();
    void applyPenalty();
    std::string regionToString(Region region) const;
    Region stringToRegion(const std::string& region) const;
    std::string statusToString(Status status) const;
    Status stringToStatus(const std::string& status) const;
};

} // namespace Game
