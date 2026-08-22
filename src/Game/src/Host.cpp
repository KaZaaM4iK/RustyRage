#include "Game/Host.hpp"
#include "Core/Logging.hpp"
#include <algorithm>

namespace Game {

Host::Host() 
    : m_status(ServerStatus::Stopped),
      m_heartbeatRunning(false),
      m_serverRunning(false),
      m_masterServerUrl("https://master.rustyrage.com") {
}

Host::~Host() {
    // Don't call shutdown() from destructor
}

bool Host::startServer(const ServerConfig& config) {
    LOG_INFOF("Starting server: %s", config.serverName.c_str());
    
    m_config = config;
    m_status = ServerStatus::Starting;
    
    // Start server thread
    m_serverRunning = true;
    m_serverThread = std::thread(&Host::serverLoop, this);
    
    // Start heartbeat thread
    m_heartbeatRunning = true;
    m_heartbeatThread = std::thread(&Host::heartbeatLoop, this);
    
    m_status = ServerStatus::Running;
    LOG_INFO("Server started successfully");
    return true;
}

void Host::stopServer() {
    LOG_INFO("Stopping server");
    
    m_status = ServerStatus::Stopping;
    m_serverRunning = false;
    m_heartbeatRunning = false;
    
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    
    m_status = ServerStatus::Stopped;
    LOG_INFO("Server stopped");
}

void Host::restartServer() {
    LOG_INFO("Restarting server");
    stopServer();
    startServer(m_config);
}

void Host::refreshGameLibrary() {
    LOG_INFO("Refreshing game library");
    loadGameLibrary();
    updateGameList();
}

void Host::searchGames(const std::string& query) {
    LOG_INFOF("Searching games: %s", query.c_str());
    // TODO: Implement game search
}

void Host::filterGamesByRegion(const std::string& region) {
    LOG_INFOF("Filtering games by region: %s", region.c_str());
    // TODO: Implement region filtering
}

void Host::filterGamesByMode(GameMode mode) {
    LOG_INFOF("Filtering games by mode: %d", (int)mode);
    // TODO: Implement mode filtering
}

void Host::sortGamesByPing() {
    LOG_INFO("Sorting games by ping");
    // TODO: Implement ping sorting
}

void Host::sortGamesByRating() {
    LOG_INFO("Sorting games by rating");
    // TODO: Implement rating sorting
}

void Host::addCustomGame(const CustomGame& game) {
    m_customGames.push_back(game);
    LOG_INFOF("Added custom game: %s", game.name.c_str());
}

void Host::removeCustomGame(const std::string& gameId) {
    auto it = std::find_if(m_customGames.begin(), m_customGames.end(),
                          [&gameId](const CustomGame& game) { return game.name == gameId; });
    
    if (it != m_customGames.end()) {
        m_customGames.erase(it);
        LOG_INFOF("Removed custom game: %s", gameId.c_str());
    }
}

void Host::updateCustomGame(const std::string& gameId, const CustomGame& game) {
    CustomGame* existingGame = getCustomGame(gameId);
    if (existingGame) {
        *existingGame = game;
        LOG_INFOF("Updated custom game: %s", gameId.c_str());
    }
}

Host::CustomGame* Host::getCustomGame(const std::string& gameId) {
    auto it = std::find_if(m_customGames.begin(), m_customGames.end(),
                          [&gameId](const CustomGame& game) { return game.name == gameId; });
    
    return (it != m_customGames.end()) ? &(*it) : nullptr;
}

bool Host::validateGameHash(const std::string& filePath, const std::string& expectedHash) {
    std::string actualHash = calculateFileHash(filePath);
    bool isValid = (actualHash == expectedHash);
    
    LOG_INFOF("Game hash validation: %s (expected: %s, actual: %s)", 
              isValid ? "PASS" : "FAIL", expectedHash.c_str(), actualHash.c_str());
    
    return isValid;
}

bool Host::validateGameFile(const std::string& filePath) {
    // TODO: Implement game file validation
    LOG_INFOF("Validating game file: %s", filePath.c_str());
    return true;
}

std::string Host::calculateFileHash(const std::string& filePath) {
    // TODO: Implement SHA-256 hash calculation
    LOG_INFOF("Calculating hash for: %s", filePath.c_str());
    return "mock_hash_" + filePath;
}

void Host::submitForApproval(const std::string& gameId, const std::string& modderProfile) {
    CustomGame* game = getCustomGame(gameId);
    if (game) {
        game->modderProfile = modderProfile;
        LOG_INFOF("Submitted game %s for approval by %s", gameId.c_str(), modderProfile.c_str());
    }
}

void Host::approveGame(const std::string& gameId, bool approved) {
    CustomGame* game = getCustomGame(gameId);
    if (game) {
        game->isApproved = approved;
        LOG_INFOF("Game %s %s", gameId.c_str(), approved ? "approved" : "rejected");
    }
}

void Host::reportGame(const std::string& gameId, const std::string& reason) {
    LOG_WARNINGF("Game %s reported: %s", gameId.c_str(), reason.c_str());
    // TODO: Implement game reporting system
}

void Host::blockModder(const std::string& modderProfile) {
    LOG_WARNINGF("Modder %s blocked", modderProfile.c_str());
    // TODO: Implement modder blocking system
}

// setMasterServerUrl and getMasterServerUrl are inline in header

void Host::sendHeartbeat() {
    // TODO: Implement heartbeat to master server
    LOG_DEBUG("Sending heartbeat to master server");
}

void Host::registerServer() {
    LOG_INFO("Registering server with master server");
    // TODO: Implement server registration
}

void Host::unregisterServer() {
    LOG_INFO("Unregistering server from master server");
    // TODO: Implement server unregistration
}

void Host::updateServerInfo() {
    LOG_DEBUG("Updating server info");
    // TODO: Implement server info update
}

void Host::addPlayer(uint32_t playerId, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    m_connectedPlayers[playerId] = name;
    m_stats.currentConnections++;
    m_stats.totalConnections++;
    
    if (m_onPlayerConnected) {
        m_onPlayerConnected(playerId, name);
    }
    
    LOG_INFOF("Player %u (%s) connected", playerId, name.c_str());
}

void Host::removePlayer(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    auto it = m_connectedPlayers.find(playerId);
    if (it != m_connectedPlayers.end()) {
        std::string name = it->second;
        m_connectedPlayers.erase(it);
        m_playerPings.erase(playerId);
        m_stats.currentConnections--;
        
        if (m_onPlayerDisconnected) {
            m_onPlayerDisconnected(playerId);
        }
        
        LOG_INFOF("Player %u (%s) disconnected", playerId, name.c_str());
    }
}

void Host::kickPlayer(uint32_t playerId, const std::string& reason) {
    LOG_INFOF("Kicking player %u: %s", playerId, reason.c_str());
    m_stats.playersKicked++;
    removePlayer(playerId);
}

void Host::banPlayer(uint32_t playerId, const std::string& reason, float duration) {
    LOG_WARNINGF("Banning player %u for %.1f seconds: %s", playerId, duration, reason.c_str());
    m_stats.playersBanned++;
    removePlayer(playerId);
    // TODO: Implement ban system
}

// getConnectedPlayers is inline in header

void Host::updateStats() {
    updateConnectionStats();
    updateGameStats();
    updatePlayerStats();
}

// Callback setters are inline in header

bool Host::downloadGame(const std::string& gameId, const std::string& downloadPath) {
    LOG_INFOF("Downloading game %s to %s", gameId.c_str(), downloadPath.c_str());
    // TODO: Implement game download
    return true;
}

bool Host::uploadGame(const std::string& gameId, const std::string& filePath) {
    LOG_INFOF("Uploading game %s from %s", gameId.c_str(), filePath.c_str());
    // TODO: Implement game upload
    return true;
}

void Host::deleteGame(const std::string& gameId) {
    LOG_INFOF("Deleting game: %s", gameId.c_str());
    removeCustomGame(gameId);
    deleteGameDirectory(gameId);
}

void Host::generateGamePreview(const std::string& gameId) {
    LOG_INFOF("Generating preview for game: %s", gameId.c_str());
    // TODO: Implement preview generation
}

std::string Host::getGamePreview(const std::string& gameId) const {
    // TODO: Implement preview retrieval
    return "preview_" + gameId;
}

void Host::serverLoop() {
    LOG_INFO("Server thread started");
    
    while (m_serverRunning) {
        updateServerStatus();
        processIncomingConnections();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    LOG_INFO("Server thread ended");
}

void Host::heartbeatLoop() {
    LOG_INFO("Heartbeat thread started");
    
    while (m_heartbeatRunning) {
        sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(30)); // 30 second intervals
    }
    
    LOG_INFO("Heartbeat thread ended");
}

void Host::updateServerStatus() {
    // TODO: Update server status
}

void Host::processIncomingConnections() {
    // TODO: Process incoming connections
}

void Host::handlePlayerMessage(uint32_t playerId, const std::string& message) {
    LOG_DEBUGF("Player %u message: %s", playerId, message.c_str());
    // TODO: Handle player messages
}

void Host::loadGameLibrary() {
    LOG_INFO("Loading game library");
    // TODO: Load games from disk
}

void Host::saveGameLibrary() {
    LOG_INFO("Saving game library");
    // TODO: Save games to disk
}

void Host::updateGameList() {
    LOG_INFO("Updating game list");
    // TODO: Update available games list
}

void Host::validateAllGames() {
    LOG_INFO("Validating all games");
    // TODO: Validate all custom games
}

bool Host::sendToMasterServer(const std::string& endpoint, const std::string& data) {
    LOG_DEBUGF("Sending to master server: %s", endpoint.c_str());
    // TODO: Implement HTTP communication
    return true;
}

std::string Host::receiveFromMasterServer() {
    // TODO: Implement HTTP response receiving
    return "";
}

void Host::handleMasterServerResponse(const std::string& response) {
    LOG_DEBUGF("Master server response: %s", response.c_str());
    // TODO: Handle master server responses
}

bool Host::createGameDirectory(const std::string& gameId) {
    LOG_INFOF("Creating game directory: %s", gameId.c_str());
    // TODO: Create game directory
    return true;
}

bool Host::deleteGameDirectory(const std::string& gameId) {
    LOG_INFOF("Deleting game directory: %s", gameId.c_str());
    // TODO: Delete game directory
    return true;
}

std::string Host::getGamePath(const std::string& gameId) const {
    return "games/" + gameId;
}

bool Host::validateGameMode(GameMode mode) const {
    return mode == GameMode::SpeedArena || mode == GameMode::RoadAvengers;
}

bool Host::validatePlayMode(PlayMode mode) const {
    return mode == PlayMode::Solo || mode == PlayMode::Multi2 || mode == PlayMode::Multi4;
}

bool Host::validateMapFile(const std::string& mapFile) const {
    // TODO: Validate map file format
    return true;
}

bool Host::validateModFile(const std::string& modFile) const {
    // TODO: Validate mod file format
    return true;
}

void Host::updateConnectionStats() {
    // TODO: Update connection statistics
}

void Host::updateGameStats() {
    // TODO: Update game statistics
}

void Host::updatePlayerStats() {
    // TODO: Update player statistics
}

} // namespace Game
