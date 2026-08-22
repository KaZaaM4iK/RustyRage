#include "Game/Player.hpp"
#include "Core/Logging.hpp"

namespace Game {

Player::Player() 
    : m_id(0),
      m_status(Status::Offline),
      m_region(Region::Russia),
      m_currentTeamId(0),
      m_ping(0),
      m_lastSeen(0.0f),
      m_position(0.0f),
      m_rotation(1.0f, 0.0f, 0.0f, 0.0f),
      m_velocity(0.0f),
      m_health(100.0f),
      m_score(0),
      m_isAlive(true),
      m_isReady(false) {
}

Player::Player(uint32_t id, const std::string& name, const std::string& displayName)
    : m_id(id),
      m_name(name),
      m_displayName(displayName),
      m_status(Status::Online),
      m_region(Region::Russia),
      m_currentTeamId(0),
      m_ping(0),
      m_lastSeen(0.0f),
      m_position(0.0f),
      m_rotation(1.0f, 0.0f, 0.0f, 0.0f),
      m_velocity(0.0f),
      m_health(100.0f),
      m_score(0),
      m_isAlive(true),
      m_isReady(false) {
}

Player::~Player() {
}

void Player::updateStatistics() {
    calculateWinRate();
    updateHighestRating();
}

void Player::addIncompleteGame() {
    m_penaltySystem.incompleteGames++;
    
    // Apply penalties based on incomplete games
    if (m_penaltySystem.incompleteGames >= 3) {
        applyPenalty();
    }
    
    LOG_INFOF("Player %u incomplete games: %d", m_id, m_penaltySystem.incompleteGames);
}

void Player::resetIncompleteGames() {
    m_penaltySystem.incompleteGames = 0;
    LOG_INFOF("Reset incomplete games for player %u", m_id);
}

void Player::updatePenaltySystem(float deltaTime) {
    if (m_penaltySystem.isBlocked && m_penaltySystem.blockRemainingTime > 0.0f) {
        m_penaltySystem.blockRemainingTime -= deltaTime;
        
        if (m_penaltySystem.blockRemainingTime <= 0.0f) {
            m_penaltySystem.isBlocked = false;
            m_penaltySystem.blockRemainingTime = 0.0f;
            LOG_INFOF("Player %u penalty block expired", m_id);
        }
    }
}

bool Player::canPlay() const {
    return !m_penaltySystem.isBlocked && m_status != Status::Banned;
}

float Player::getBlockRemainingTime() const {
    return m_penaltySystem.blockRemainingTime;
}

void Player::updateRating(int pointsChange) {
    m_statistics.currentRating += pointsChange;
    updateHighestRating();
    
    LOG_INFOF("Player %u rating updated: %d (change: %+d)", 
              m_id, m_statistics.currentRating, pointsChange);
}

bool Player::isValid() const {
    return m_id != 0 && !m_name.empty();
}

std::string Player::serialize() const {
    // TODO: Implement JSON serialization
    return "{}";
}

bool Player::deserialize(const std::string& data) {
    // TODO: Implement JSON deserialization
    return true;
}

void Player::calculateWinRate() {
    if (m_statistics.totalGames > 0) {
        m_statistics.winRate = (float)m_statistics.wins / (float)m_statistics.totalGames;
    } else {
        m_statistics.winRate = 0.0f;
    }
}

void Player::updateHighestRating() {
    if (m_statistics.currentRating > m_statistics.highestRating) {
        m_statistics.highestRating = m_statistics.currentRating;
    }
}

void Player::applyPenalty() {
    if (m_penaltySystem.incompleteGames == 3) {
        m_penaltySystem.blockTime = 3600.0f; // 1 hour
    } else if (m_penaltySystem.incompleteGames == 4) {
        m_penaltySystem.blockTime = 10800.0f; // 3 hours
    } else {
        m_penaltySystem.blockTime = 86400.0f; // 24 hours
    }
    
    m_penaltySystem.isBlocked = true;
    m_penaltySystem.blockRemainingTime = m_penaltySystem.blockTime;
    
    LOG_WARNINGF("Player %u blocked for %.1f seconds due to %d incomplete games", 
                 m_id, m_penaltySystem.blockTime, m_penaltySystem.incompleteGames);
}

std::string Player::regionToString(Region region) const {
    switch (region) {
        case Region::Russia: return "RU";
        case Region::China: return "CN";
        default: return "RU";
    }
}

Player::Region Player::stringToRegion(const std::string& region) const {
    if (region == "CN") return Region::China;
    return Region::Russia;
}

std::string Player::statusToString(Status status) const {
    switch (status) {
        case Status::Online: return "online";
        case Status::InGame: return "ingame";
        case Status::InLobby: return "lobby";
        case Status::Offline: return "offline";
        case Status::Banned: return "banned";
        default: return "offline";
    }
}

Player::Status Player::stringToStatus(const std::string& status) const {
    if (status == "online") return Status::Online;
    if (status == "ingame") return Status::InGame;
    if (status == "lobby") return Status::InLobby;
    if (status == "banned") return Status::Banned;
    return Status::Offline;
}

} // namespace Game
