#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace Game {

/**
 * Game hosting system for Rusty Rage
 * Handles local server hosting and game library management
 */
class Host {
public:
    enum class ServerStatus {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error
    };
    
    enum class GameMode {
        SpeedArena,      // Арена Скорости
        RoadAvengers     // Мстители Дороги
    };
    
    enum class PlayMode {
        Solo,           // Одиночная игра
        Multi2,         // Мультиплеер 2 игрока
        Multi4          // Мультиплеер 4 игрока
    };
    
    struct GameServer {
        std::string id;
        std::string name;
        std::string description;
        std::string ipAddress;
        int port;
        std::string mapName;
        GameMode gameMode;
        PlayMode playMode;
        int currentPlayers;
        int maxPlayers;
        int ping;
        std::string region;
        std::string hash;           // SHA-256 hash for validation
        std::string modderProfile;  // Modder profile for approval
        bool isApproved;            // Admin approval status
        bool isOfficial;            // Official game mode
        float uptime;
        std::string lastHeartbeat;
        
        GameServer() : port(0), gameMode(GameMode::SpeedArena), playMode(PlayMode::Solo),
                      currentPlayers(0), maxPlayers(8), ping(0), isApproved(false), isOfficial(false),
                      uptime(0.0f) {}
    };
    
    struct CustomGame {
        std::string name;
        std::string description;
        std::string mapFile;
        std::string modFile;
        std::string hash;
        std::string modderProfile;
        bool isApproved;
        int downloadCount;
        int playCount;
        float rating;
        std::string lastUpdated;
        
        CustomGame() : isApproved(false), downloadCount(0), playCount(0), rating(0.0f) {}
    };
    
    struct ServerConfig {
        std::string serverName;
        int maxPlayers;
        int port;
        std::string mapName;
        GameMode gameMode;
        PlayMode playMode;
        float timeLimit;
        bool weaponsEnabled;
        bool powerUpsEnabled;
        bool destructionEnabled;
        bool respawnEnabled;
        float respawnTime;
        bool antiCampEnabled;
        float antiCampTime;
        std::string region;
        bool isPublic;
        std::string password;
        
        ServerConfig() : maxPlayers(8), port(7777), gameMode(GameMode::SpeedArena),
                        playMode(PlayMode::Solo), timeLimit(600.0f), weaponsEnabled(true),
                        powerUpsEnabled(true), destructionEnabled(true), respawnEnabled(true),
                        respawnTime(2.0f), antiCampEnabled(true), antiCampTime(3.0f),
                        region("RU"), isPublic(true) {}
    };

    Host();
    ~Host();

    // Server management
    bool startServer(const ServerConfig& config);
    void stopServer();
    void restartServer();
    
    ServerStatus getStatus() const { return m_status; }
    bool isRunning() const { return m_status == ServerStatus::Running; }
    
    // Configuration
    void setConfig(const ServerConfig& config) { m_config = config; }
    const ServerConfig& getConfig() const { return m_config; }
    
    // Game library management
    void refreshGameLibrary();
    void searchGames(const std::string& query);
    void filterGamesByRegion(const std::string& region);
    void filterGamesByMode(GameMode mode);
    void sortGamesByPing();
    void sortGamesByRating();
    
    const std::vector<GameServer>& getAvailableGames() const { return m_availableGames; }
    const std::vector<CustomGame>& getCustomGames() const { return m_customGames; }
    
    // Custom game management
    void addCustomGame(const CustomGame& game);
    void removeCustomGame(const std::string& gameId);
    void updateCustomGame(const std::string& gameId, const CustomGame& game);
    CustomGame* getCustomGame(const std::string& gameId);
    
    // Game validation
    bool validateGameHash(const std::string& filePath, const std::string& expectedHash);
    bool validateGameFile(const std::string& filePath);
    std::string calculateFileHash(const std::string& filePath);
    
    // Modder approval system
    void submitForApproval(const std::string& gameId, const std::string& modderProfile);
    void approveGame(const std::string& gameId, bool approved);
    void reportGame(const std::string& gameId, const std::string& reason);
    void blockModder(const std::string& modderProfile);
    
    // Network communication
    void setMasterServerUrl(const std::string& url) { m_masterServerUrl = url; }
    const std::string& getMasterServerUrl() const { return m_masterServerUrl; }
    
    void sendHeartbeat();
    void registerServer();
    void unregisterServer();
    void updateServerInfo();
    
    // Player management
    void addPlayer(uint32_t playerId, const std::string& name);
    void removePlayer(uint32_t playerId);
    void kickPlayer(uint32_t playerId, const std::string& reason);
    void banPlayer(uint32_t playerId, const std::string& reason, float duration = 0.0f);
    
    const std::map<uint32_t, std::string>& getConnectedPlayers() const { return m_connectedPlayers; }
    
    // Statistics
    struct ServerStats {
        int totalConnections;
        int currentConnections;
        float uptime;
        int gamesPlayed;
        int playersKicked;
        int playersBanned;
        float averagePing;
        
        ServerStats() : totalConnections(0), currentConnections(0), uptime(0.0f),
                       gamesPlayed(0), playersKicked(0), playersBanned(0), averagePing(0.0f) {}
    };
    
    const ServerStats& getStats() const { return m_stats; }
    void updateStats();
    
    // Callbacks
    void setOnPlayerConnected(std::function<void(uint32_t, const std::string&)> callback) { m_onPlayerConnected = callback; }
    void setOnPlayerDisconnected(std::function<void(uint32_t)> callback) { m_onPlayerDisconnected = callback; }
    void setOnGameStarted(std::function<void()> callback) { m_onGameStarted = callback; }
    void setOnGameEnded(std::function<void()> callback) { m_onGameEnded = callback; }
    
    // File management
    bool downloadGame(const std::string& gameId, const std::string& downloadPath);
    bool uploadGame(const std::string& gameId, const std::string& filePath);
    void deleteGame(const std::string& gameId);
    
    // Preview system
    void generateGamePreview(const std::string& gameId);
    std::string getGamePreview(const std::string& gameId) const;

private:
    // Server state
    ServerStatus m_status;
    ServerConfig m_config;
    ServerStats m_stats;
    
    // Network
    std::string m_masterServerUrl;
    std::thread m_heartbeatThread;
    std::atomic<bool> m_heartbeatRunning;
    
    // Game library
    std::vector<GameServer> m_availableGames;
    std::vector<CustomGame> m_customGames;
    std::mutex m_gamesMutex;
    
    // Players
    std::map<uint32_t, std::string> m_connectedPlayers;
    std::map<uint32_t, float> m_playerPings;
    std::mutex m_playersMutex;
    
    // Callbacks
    std::function<void(uint32_t, const std::string&)> m_onPlayerConnected;
    std::function<void(uint32_t)> m_onPlayerDisconnected;
    std::function<void()> m_onGameStarted;
    std::function<void()> m_onGameEnded;
    
    // Server thread
    std::thread m_serverThread;
    std::atomic<bool> m_serverRunning;
    
    // Helper methods
    void serverLoop();
    void heartbeatLoop();
    void updateServerStatus();
    void processIncomingConnections();
    void handlePlayerMessage(uint32_t playerId, const std::string& message);
    
    // Game library helpers
    void loadGameLibrary();
    void saveGameLibrary();
    void updateGameList();
    void validateAllGames();
    
    // Network helpers
    bool sendToMasterServer(const std::string& endpoint, const std::string& data);
    std::string receiveFromMasterServer();
    void handleMasterServerResponse(const std::string& response);
    
    // File helpers
    bool createGameDirectory(const std::string& gameId);
    bool deleteGameDirectory(const std::string& gameId);
    std::string getGamePath(const std::string& gameId) const;
    
    // Validation helpers
    bool validateGameMode(GameMode mode) const;
    bool validatePlayMode(PlayMode mode) const;
    bool validateMapFile(const std::string& mapFile) const;
    bool validateModFile(const std::string& modFile) const;
    
    // Statistics helpers
    void updateConnectionStats();
    void updateGameStats();
    void updatePlayerStats();
};

} // namespace Game
