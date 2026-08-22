#include "Game/Matchmaking.hpp"
#include "Game/Player.hpp"
#include "Core/Logging.hpp"
#include <algorithm>
#include <cmath>

namespace Game {

Matchmaking::Matchmaking() 
    : m_status(MatchmakingStatus::Idle),
      m_searchTime(0.0f),
      m_searching(false),
      m_threadRunning(false),
      m_masterServerUrl("https://master.rustyrage.com") {
}

Matchmaking::~Matchmaking() {
    shutdown();
}

bool Matchmaking::initialize() {
    LOG_INFO("Initializing matchmaking system");
    
    // Start matchmaking thread
    m_threadRunning = true;
    m_matchmakingThread = std::thread(&Matchmaking::matchmakingLoop, this);
    
    LOG_INFO("Matchmaking system initialized");
    return true;
}

void Matchmaking::shutdown() {
    LOG_INFO("Shutting down matchmaking system");
    
    m_threadRunning = false;
    m_searching = false;
    
    if (m_matchmakingThread.joinable()) {
        m_matchmakingThread.join();
    }
}

void Matchmaking::startSearch(GameMode gameMode, PlayMode playMode, Region region) {
    LOG_INFOF("Starting matchmaking search: mode=%d, playMode=%d, region=%d", 
              (int)gameMode, (int)playMode, (int)region);
    
    m_currentRequest.gameMode = gameMode;
    m_currentRequest.playMode = playMode;
    m_currentRequest.region = region;
    m_currentRequest.searchTime = 0.0f;
    m_currentRequest.isGroup = false;
    
    setStatus(MatchmakingStatus::Searching);
    m_searching = true;
}

void Matchmaking::stopSearch() {
    LOG_INFO("Stopping matchmaking search");
    m_searching = false;
    setStatus(MatchmakingStatus::Idle);
}

void Matchmaking::cancelSearch() {
    LOG_INFO("Cancelling matchmaking search");
    m_searching = false;
    setStatus(MatchmakingStatus::Cancelled);
}

void Matchmaking::addPlayer(const Player& player) {
    m_players[player.getId()] = player;
    LOG_INFOF("Added player %u to matchmaking", player.getId());
}

void Matchmaking::removePlayer(uint32_t playerId) {
    auto it = m_players.find(playerId);
    if (it != m_players.end()) {
        m_players.erase(it);
        LOG_INFOF("Removed player %u from matchmaking", playerId);
    }
}

void Matchmaking::updatePlayerRating(uint32_t playerId, int newRating) {
    auto it = m_players.find(playerId);
    if (it != m_players.end()) {
        it->second.getStatistics().currentRating = newRating;
    }
}

void Matchmaking::updatePlayerPing(uint32_t playerId, int ping) {
    auto it = m_players.find(playerId);
    if (it != m_players.end()) {
        it->second.setPing(ping);
    }
}

void Matchmaking::createGroup(uint32_t leaderId, const std::vector<uint32_t>& members) {
    uint32_t groupId = leaderId; // Use leader ID as group ID
    
    m_groups[groupId] = members;
    for (uint32_t memberId : members) {
        m_playerGroups[memberId] = groupId;
    }
    
    LOG_INFOF("Created group %u with %zu members", groupId, members.size());
}

void Matchmaking::joinGroup(uint32_t playerId, uint32_t groupId) {
    auto it = m_groups.find(groupId);
    if (it != m_groups.end()) {
        it->second.push_back(playerId);
        m_playerGroups[playerId] = groupId;
        LOG_INFOF("Player %u joined group %u", playerId, groupId);
    }
}

void Matchmaking::leaveGroup(uint32_t playerId) {
    auto it = m_playerGroups.find(playerId);
    if (it != m_playerGroups.end()) {
        uint32_t groupId = it->second;
        m_playerGroups.erase(it);
        
        auto groupIt = m_groups.find(groupId);
        if (groupIt != m_groups.end()) {
            auto& members = groupIt->second;
            members.erase(std::remove(members.begin(), members.end(), playerId), members.end());
            
            if (members.empty()) {
                m_groups.erase(groupIt);
            }
        }
        
        LOG_INFOF("Player %u left group %u", playerId, groupId);
    }
}

void Matchmaking::disbandGroup(uint32_t groupId) {
    auto it = m_groups.find(groupId);
    if (it != m_groups.end()) {
        for (uint32_t memberId : it->second) {
            m_playerGroups.erase(memberId);
        }
        m_groups.erase(it);
        LOG_INFOF("Disbanded group %u", groupId);
    }
}

void Matchmaking::addServer(const ServerInfo& server) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_serversMutex));
    m_servers.push_back(server);
    LOG_INFOF("Added server %s:%d", server.address.c_str(), server.port);
}

void Matchmaking::removeServer(const std::string& address, int port) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_serversMutex));
    m_servers.erase(std::remove_if(m_servers.begin(), m_servers.end(),
        [&address, port](const ServerInfo& server) {
            return server.address == address && server.port == port;
        }), m_servers.end());
    
    LOG_INFOF("Removed server %s:%d", address.c_str(), port);
}

void Matchmaking::updateServerInfo(const std::string& address, int port, const ServerInfo& info) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_serversMutex));
    for (auto& server : m_servers) {
        if (server.address == address && server.port == port) {
            server = info;
            break;
        }
    }
}

std::vector<Matchmaking::ServerInfo> Matchmaking::getAvailableServers(Region region) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_serversMutex));
    std::vector<ServerInfo> available;
    
    for (const auto& server : m_servers) {
        if (server.isAvailable && server.region == Region::Russia) {
            available.push_back(server);
        }
    }
    
    return available;
}

Matchmaking::Match Matchmaking::createMatch(const std::vector<MatchmakingRequest>& players, 
                                           GameMode gameMode, PlayMode playMode, Region region) {
    Match match;
    match.matchId = generateMatchId();
    match.players = players;
    match.gameMode = gameMode;
    match.playMode = playMode;
    match.region = region;
    match.averageRating = 0;
    match.averagePing = 0;
    
    // Calculate averages
    for (const auto& player : players) {
        match.averageRating += player.rating;
        match.averagePing += player.averagePing;
    }
    
    if (!players.empty()) {
        match.averageRating /= players.size();
        match.averagePing /= players.size();
    }
    
    // Assign server
    assignServer(match);
    
    LOG_INFOF("Created match %s with %zu players", match.matchId.c_str(), players.size());
    return match;
}

void Matchmaking::assignServer(Match& match) {
    ServerInfo bestServer = selectBestServer(match.region, match.players.size());
    match.serverAddress = bestServer.address;
    match.serverPort = bestServer.port;
}

// void Matchmaking::setOnMatchFound(std::function<void(const Match&)> callback) {
//     m_onMatchFound = callback;
// }

// void Matchmaking::setOnSearchFailed(std::function<void(const std::string&)> callback) {
//     m_onSearchFailed = callback;
// }

// void Matchmaking::setOnStatusChanged(std::function<void(MatchmakingStatus)> callback) {
//     m_onStatusChanged = callback;
// }

int Matchmaking::calculateELODifference(int rating1, int rating2) const {
    return abs(rating1 - rating2);
}

int Matchmaking::calculateNewRating(int currentRating, int opponentRating, bool won) const {
    const int K = 32; // K-factor
    float expectedScore = 1.0f / (1.0f + pow(10.0f, (opponentRating - currentRating) / 400.0f));
    int actualScore = won ? 1 : 0;
    
    return currentRating + K * (actualScore - expectedScore);
}

bool Matchmaking::isRatingCompatible(int rating1, int rating2, float maxDifference) const {
    return calculateELODifference(rating1, rating2) <= maxDifference;
}

Matchmaking::Region Matchmaking::getPlayerRegion(uint32_t playerId) const {
    auto it = m_players.find(playerId);
    if (it != m_players.end()) {
        return Region::Russia; // TODO: Fix region conversion
    }
    return Region::Russia;
}

int Matchmaking::getPlayerPing(uint32_t playerId, Region region) const {
    auto it = m_players.find(playerId);
    if (it != m_players.end()) {
        return it->second.getPing();
    }
    return 0;
}

bool Matchmaking::isRegionCompatible(Region playerRegion, Region serverRegion) const {
    return playerRegion == serverRegion;
}

void Matchmaking::updateStats(const Match& match, float searchTime) {
    m_stats.totalSearches++;
    m_stats.successfulMatches++;
    m_stats.averageSearchTime = (m_stats.averageSearchTime * (m_stats.totalSearches - 1) + searchTime) / m_stats.totalSearches;
    
    auto it = m_stats.regionStats.find(match.region);
    if (it != m_stats.regionStats.end()) {
        it->second++;
    } else {
        m_stats.regionStats[match.region] = 1;
    }
}

void Matchmaking::sendHeartbeat() {
    // TODO: Implement heartbeat to master server
}

void Matchmaking::requestServerList() {
    // TODO: Implement server list request
}

void Matchmaking::reportMatchResult(const std::string& matchId, const std::vector<uint32_t>& winners) {
    // TODO: Implement match result reporting
    LOG_INFOF("Reporting match result for %s", matchId.c_str());
}

void Matchmaking::matchmakingLoop() {
    LOG_INFO("Matchmaking thread started");
    
    while (m_threadRunning) {
        if (m_searching) {
            updateSearchTime(0.016f); // ~60 FPS
            
            // Find compatible players
            std::vector<MatchmakingRequest> compatiblePlayers = findCompatiblePlayers(m_currentRequest);
            
            if (compatiblePlayers.size() >= 8) { // Full match
                Match match = createMatchFromPlayers(compatiblePlayers);
                setStatus(MatchmakingStatus::Found);
                m_searching = false;
                
                if (m_onMatchFound) {
                    m_onMatchFound(match);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    LOG_INFO("Matchmaking thread ended");
}

std::vector<Matchmaking::MatchmakingRequest> Matchmaking::findCompatiblePlayers(const MatchmakingRequest& request) {
    std::vector<MatchmakingRequest> compatible;
    
    for (const auto& pair : m_players) {
        const Player& player = pair.second;
        
        MatchmakingRequest playerRequest;
        playerRequest.playerId = player.getId();
        playerRequest.playerName = player.getName();
        playerRequest.rating = player.getRating();
        playerRequest.region = Region::Russia; // TODO: Fix region conversion
        playerRequest.gameMode = request.gameMode;
        playerRequest.playMode = request.playMode;
        playerRequest.averagePing = player.getPing();
        playerRequest.searchTime = m_searchTime;
        
        if (isPlayerCompatible(request, playerRequest)) {
            compatible.push_back(playerRequest);
        }
    }
    
    return compatible;
}

bool Matchmaking::isPlayerCompatible(const MatchmakingRequest& player1, const MatchmakingRequest& player2) {
    // Check region compatibility
    if (player1.region != player2.region) {
        return false;
    }
    
    // Check rating compatibility
    if (!isRatingCompatible(player1.rating, player2.rating)) {
        return false;
    }
    
    // Check ping compatibility
    if (abs(player1.averagePing - player2.averagePing) > 100) {
        return false;
    }
    
    return true;
}

Matchmaking::Match Matchmaking::createMatchFromPlayers(const std::vector<MatchmakingRequest>& players) {
    return createMatch(players, m_currentRequest.gameMode, m_currentRequest.playMode, m_currentRequest.region);
}

Matchmaking::ServerInfo Matchmaking::selectBestServer(Region region, int playerCount) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_serversMutex));
    
    ServerInfo bestServer;
    int bestScore = -1;
    
    for (const auto& server : m_servers) {
        if (server.isAvailable && server.region == Region::Russia) {
            int score = calculateServerScore(server, region, playerCount);
            if (score > bestScore) {
                bestScore = score;
                bestServer = server;
            }
        }
    }
    
    return bestServer;
}

int Matchmaking::calculateServerScore(const ServerInfo& server, Region region, int playerCount) const {
    int score = 100; // Base score
    
    // Prefer servers with lower ping
    score -= server.ping / 10;
    
    // Prefer servers with more available slots
    score += (server.maxPlayers - server.currentPlayers) * 5;
    
    // Prefer servers in the same region
    if (server.region == Region::Russia) {
        score += 50;
    }
    
    return score;
}

void Matchmaking::setStatus(MatchmakingStatus status) {
    if (m_status != status) {
        m_status = status;
        if (m_onStatusChanged) {
            m_onStatusChanged(status);
        }
    }
}

void Matchmaking::updateSearchTime(float deltaTime) {
    m_searchTime += deltaTime;
    m_currentRequest.searchTime = m_searchTime;
}

void Matchmaking::cleanupExpiredSearches() {
    // TODO: Implement cleanup of expired searches
}

std::string Matchmaking::generateMatchId() const {
    // Simple match ID generation
    static int matchCounter = 0;
    return "match_" + std::to_string(++matchCounter);
}

bool Matchmaking::sendRequest(const std::string& endpoint, const std::string& data) {
    // TODO: Implement HTTP request sending
    return true;
}

std::string Matchmaking::receiveResponse() {
    // TODO: Implement HTTP response receiving
    return "";
}

void Matchmaking::handleServerResponse(const std::string& response) {
    // TODO: Implement server response handling
}

bool Matchmaking::validateMatchmakingRequest(const MatchmakingRequest& request) const {
    return request.playerId != 0 && !request.playerName.empty() && request.rating > 0;
}

bool Matchmaking::validatePlayer(uint32_t playerId) const {
    return m_players.find(playerId) != m_players.end();
}

bool Matchmaking::validateGroup(uint32_t groupId) const {
    return m_groups.find(groupId) != m_groups.end();
}

// std::string Matchmaking::regionToString(Region region) const {
//     switch (region) {
//         case Region::Russia: return "RU";
//         case Region::China: return "CN";
//         default: return "RU";
//     }
// }

} // namespace Game
