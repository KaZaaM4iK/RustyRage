#pragma once

#include <string>
#include "GameModes.hpp"
#include "Core/Logging.hpp"
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace Game {

class Player;

/**
 * Matchmaking system for Rusty Rage
 * Handles regional matchmaking with ELO rating system
 */
class Matchmaking {
public:
    enum class GameMode {
        SpeedArena,      // Арена Скорости
        RoadAvengers     // Мстители Дороги
    };
    
    enum class PlayMode {
        Solo,           // Одиночная игра
        Multi2,         // Мультиплеер 2 игрока
        Multi4          // Мультиплеер 4 игрока
    };
    
    enum class Region {
        Russia,         // RU
        China           // CN
    };
    
    enum class MatchmakingStatus {
        Idle,
        Searching,
        Found,
        Failed,
        Cancelled
    };
    
    struct MatchmakingRequest {
        uint32_t playerId;
        std::string playerName;
        int rating;
        Region region;
        GameMode gameMode;
        PlayMode playMode;
        int averagePing;
        float searchTime;
        bool isGroup;
        std::vector<uint32_t> groupMembers;
        
        MatchmakingRequest() : playerId(0), rating(1000), region(Region::Russia),
                              gameMode(GameMode::SpeedArena), playMode(PlayMode::Solo),
                              averagePing(0), searchTime(0.0f), isGroup(false) {}
    };
    
    struct Match {
        std::string matchId;
        std::vector<MatchmakingRequest> players;
        GameMode gameMode;
        PlayMode playMode;
        Region region;
        std::string serverAddress;
        int serverPort;
        float averageRating;
        int averagePing;
        
        Match() : gameMode(GameMode::SpeedArena), playMode(PlayMode::Solo),
                 region(Region::Russia), serverPort(0), averageRating(1000), averagePing(0) {}
    };
    
    struct ServerInfo {
        std::string address;
        int port;
        Region region;
        int currentPlayers;
        int maxPlayers;
        int ping;
        bool isAvailable;
        std::string gameMode;
        std::string mapName;
        
        ServerInfo() : port(0), region(Region::Russia), currentPlayers(0), maxPlayers(8),
                      ping(0), isAvailable(false) {}
    };

    Matchmaking();
    ~Matchmaking();

    // Initialization
    bool initialize();
    void shutdown();
    
    // Matchmaking control
    void startSearch(GameMode gameMode, PlayMode playMode, Region region = Region::Russia);
    void stopSearch();
    void cancelSearch();
    
    // Status
    MatchmakingStatus getStatus() const { return m_status; }
    bool isSearching() const { return m_status == MatchmakingStatus::Searching; }
    float getSearchTime() const { return m_searchTime; }
    bool isSearchComplete() const { return m_status == MatchmakingStatus::Found; }

    // Player management
    void addPlayer(const Player& player);
    void removePlayer(uint32_t playerId);
    void updatePlayerRating(uint32_t playerId, int newRating);
    void updatePlayerPing(uint32_t playerId, int ping);
    
    // Group management
    void createGroup(uint32_t leaderId, const std::vector<uint32_t>& members);
    void joinGroup(uint32_t playerId, uint32_t groupId);
    void leaveGroup(uint32_t playerId);
    void disbandGroup(uint32_t groupId);
    
    // Server management
    void addServer(const ServerInfo& server);
    void removeServer(const std::string& address, int port);
    void updateServerInfo(const std::string& address, int port, const ServerInfo& info);
    std::vector<ServerInfo> getAvailableServers(Region region) const;
    
    // Match creation
    Match createMatch(const std::vector<MatchmakingRequest>& players, 
                     GameMode gameMode, PlayMode playMode, Region region);
    void assignServer(Match& match);
    
    // Callbacks
    void setOnMatchFound(std::function<void(const Match&)> callback) { m_onMatchFound = callback; }
    void setOnSearchFailed(std::function<void(const std::string&)> callback) { m_onSearchFailed = callback; }
    void setOnStatusChanged(std::function<void(MatchmakingStatus)> callback) { m_onStatusChanged = callback; }
    
    // ELO rating system
    int calculateELODifference(int rating1, int rating2) const;
    int calculateNewRating(int currentRating, int opponentRating, bool won) const;
    bool isRatingCompatible(int rating1, int rating2, float maxDifference = 200.0f) const;
    
    // Regional matchmaking
    Region getPlayerRegion(uint32_t playerId) const;
    int getPlayerPing(uint32_t playerId, Region region) const;
    bool isRegionCompatible(Region playerRegion, Region serverRegion) const;
    
    // Statistics
    struct MatchmakingStats {
        int totalSearches;
        int successfulMatches;
        float averageSearchTime;
        int averageRatingDifference;
        std::map<Region, int> regionStats;
        
        MatchmakingStats() : totalSearches(0), successfulMatches(0), averageSearchTime(0.0f),
                           averageRatingDifference(0) {}
    };
    
    const MatchmakingStats& getStats() const { return m_stats; }
    void updateStats(const Match& match, float searchTime);
    
    // Network communication
    void setMasterServerUrl(const std::string& url) { m_masterServerUrl = url; }
    const std::string& getMasterServerUrl() const { return m_masterServerUrl; }
    
    void sendHeartbeat();
    void requestServerList();
    void reportMatchResult(const std::string& matchId, const std::vector<uint32_t>& winners);

private:
    // Matchmaking state
    MatchmakingStatus m_status;
    MatchmakingRequest m_currentRequest;
    float m_searchTime;
    std::atomic<bool> m_searching;
    
    // Players and groups
    std::map<uint32_t, Player> m_players;
    std::map<uint32_t, std::vector<uint32_t>> m_groups; // groupId -> playerIds
    std::map<uint32_t, uint32_t> m_playerGroups; // playerId -> groupId
    
    // Servers
    std::vector<ServerInfo> m_servers;
    std::mutex m_serversMutex;
    
    // Matchmaking algorithm
    std::thread m_matchmakingThread;
    std::atomic<bool> m_threadRunning;
    
    // Callbacks
    std::function<void(const Match&)> m_onMatchFound;
    std::function<void(const std::string&)> m_onSearchFailed;
    std::function<void(MatchmakingStatus)> m_onStatusChanged;
    
    // Statistics
    MatchmakingStats m_stats;
    
    // Network
    std::string m_masterServerUrl;
    
    // Matchmaking algorithm
    void matchmakingLoop();
    std::vector<MatchmakingRequest> findCompatiblePlayers(const MatchmakingRequest& request);
    bool isPlayerCompatible(const MatchmakingRequest& player1, const MatchmakingRequest& player2);
    Match createMatchFromPlayers(const std::vector<MatchmakingRequest>& players);
    
    // Server selection
    ServerInfo selectBestServer(Region region, int playerCount) const;
    int calculateServerScore(const ServerInfo& server, Region region, int playerCount) const;
    
    // Helper methods
    void setStatus(MatchmakingStatus status);
    void updateSearchTime(float deltaTime);
    void cleanupExpiredSearches();
    std::string generateMatchId() const;
    
    // Network helpers
    bool sendRequest(const std::string& endpoint, const std::string& data);
    std::string receiveResponse();
    void handleServerResponse(const std::string& response);
    
    // Validation
    bool validateMatchmakingRequest(const MatchmakingRequest& request) const;
    bool validatePlayer(uint32_t playerId) const;
    bool validateGroup(uint32_t groupId) const;
};

} // namespace Game
