#include "Game/RustyRageGame.hpp"
#include "Core/Logging.hpp"
#include <algorithm>
#include <stdexcept>

namespace Game {

RustyRageGame::RustyRageGame(int width, int height, const std::string& title)
    : Core::Application(width, height, title),
      m_gameState(GameState::MainMenu),
      m_gameMode(GameMode::SpeedArena),
      m_playMode(PlayMode::Solo),
      m_gameTimer(0.0f),
      m_maxGameTime(600.0f),
      m_gameEnded(false) {
    
    LOG_INFO("Creating Rusty Rage game instance");
}

RustyRageGame::~RustyRageGame() {
    LOG_INFO("Destroying Rusty Rage game instance");
}

void RustyRageGame::initialize() {
    LOG_INFO("Initializing Rusty Rage game systems");
    
    try {
        initializeGameSystems();
        initializeScoreTable();
        initializeModels();
        
        setupUICallbacks();
        
        m_physics = std::make_unique<Core::Physics>();
        if (!m_physics->initialize()) {
            throw std::runtime_error("Failed to initialize PhysX");
        }
        
        m_camera = std::make_unique<Core::Camera>();
        m_camera->setPerspective(45.0f, (float)getWidth() / (float)getHeight(), 0.1f, 1000.0f);
        m_camera->setPosition(glm::vec3(0.0f, 10.0f, 20.0f));
        m_camera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        m_camera->updateMatrices();
        
        LOG_INFO("Rusty Rage initialization complete");
    } catch (const std::exception& e) {
        LOG_ERRORF("Failed to initialize Rusty Rage: %s", e.what());
        throw;
    }
}

void RustyRageGame::update(float deltaTime) {
    handleInput();
    switch (m_gameState) {
        case GameState::MainMenu:
            updateMainMenu(deltaTime);
            break;
        case GameState::SinglePlayer:
        case GameState::Multiplayer:
        case GameState::GameLibrary:
        case GameState::Settings:
            break;
        case GameState::InGame:
            updateInGame(deltaTime);
            break;
        case GameState::Paused:
            break;
        case GameState::Loading:
            updateLoading(deltaTime);
            break;
    }
    if (m_physics) m_physics->step(deltaTime);
    if (m_camera) m_camera->updateMatrices();
}

void RustyRageGame::render() {
    switch (m_gameState) {
        case GameState::MainMenu:
            renderMainMenu();
            break;
        case GameState::InGame:
            renderInGame();
            break;
        case GameState::Loading:
            renderLoading();
            break;
        default:
            break;
    }
}

void RustyRageGame::shutdown() {
    LOG_INFO("Shutting down Rusty Rage game systems");
    
    if (m_matchmaking) {
        m_matchmaking->stopSearch();
        m_matchmaking.reset();
    }
    
    if (m_host) {
        m_host->stopServer();
        m_host.reset();
    }
    
    if (m_physics) {
        m_physics->shutdown();
        m_physics.reset();
    }
    
    m_camera.reset();
    
    m_players.clear();
    m_vehicles.clear();
    m_models.clear();
    m_availableGames.clear();
    m_scores.clear();
    
    LOG_INFO("Rusty Rage shutdown complete");
}

void RustyRageGame::setGameState(GameState state) {
    LOG_INFOF("Changing game state from %d to %d", (int)m_gameState, (int)state);
    m_gameState = state;
    
    // Используем getUI() вместо прямого доступа к m_ui
    if (getUI()) {
        switch (state) {
            case GameState::MainMenu:
                getUI()->setCurrentScreen(UI::Screen::MainMenu);
                break;
            case GameState::SinglePlayer:
                getUI()->setCurrentScreen(UI::Screen::SinglePlayer);
                break;
            case GameState::Multiplayer:
                getUI()->setCurrentScreen(UI::Screen::Multiplayer);
                break;
            case GameState::GameLibrary:
                getUI()->setCurrentScreen(UI::Screen::GameLibrary);
                break;
            case GameState::Settings:
                getUI()->setCurrentScreen(UI::Screen::Settings);
                break;
            case GameState::InGame:
                getUI()->setCurrentScreen(UI::Screen::InGame);
                break;
            case GameState::Paused:
                getUI()->setCurrentScreen(UI::Screen::Paused);
                break;
            case GameState::Loading:
                getUI()->setCurrentScreen(UI::Screen::Loading);
                break;
        }
    }
}

void RustyRageGame::addPlayer(const Player& player) {
    m_players.push_back(player);
    LOG_INFOF("Added player: %s (ID: %u)", player.getName().c_str(), player.getId());
}

void RustyRageGame::removePlayer(uint32_t playerId) {
    auto it = std::find_if(m_players.begin(), m_players.end(),
                          [playerId](const Player& p) { return p.getId() == playerId; });
    
    if (it != m_players.end()) {
        LOG_INFOF("Removed player: %s (ID: %u)", it->getName().c_str(), playerId);
        m_players.erase(it);
    }
}

Player* RustyRageGame::getPlayer(uint32_t playerId) {
    auto it = std::find_if(m_players.begin(), m_players.end(),
                          [playerId](const Player& p) { return p.getId() == playerId; });
    
    return (it != m_players.end()) ? &(*it) : nullptr;
}

void RustyRageGame::startMatchmaking(GameMode mode, PlayMode playMode, const std::string& region) {
    LOG_INFOF("Starting matchmaking: mode=%d, playMode=%d, region=%s", 
              (int)mode, (int)playMode, region.c_str());
    
    m_gameMode = mode;
    m_playMode = playMode;
    
    m_matchmaking = std::make_unique<Matchmaking>();
    if (!m_matchmaking->initialize()) {
        LOG_ERROR("Failed to initialize matchmaking");
        return;
    }
    
    // Set callbacks
    m_matchmaking->setOnMatchFound([this](const Matchmaking::Match& match) {
        LOG_INFOF("Match found: %s", match.matchId.c_str());
        // TODO: Connect to match server
    });
    
    m_matchmaking->setOnSearchFailed([this](const std::string& reason) {
        LOG_WARNINGF("Matchmaking failed: %s", reason.c_str());
        setGameState(GameState::MainMenu);
    });
    
    // Start search
    Matchmaking::Region matchRegion = (region == "CN") ? Matchmaking::Region::China : Matchmaking::Region::Russia;
    Matchmaking::GameMode matchGameMode = (mode == GameMode::SpeedArena) ? 
                                         Matchmaking::GameMode::SpeedArena : Matchmaking::GameMode::RoadAvengers;
    Matchmaking::PlayMode matchPlayMode = (playMode == PlayMode::Solo) ? 
                                         Matchmaking::PlayMode::Solo : 
                                         (playMode == PlayMode::Multi2) ? Matchmaking::PlayMode::Multi2 : Matchmaking::PlayMode::Multi4;
    
    m_matchmaking->startSearch(matchGameMode, matchPlayMode, matchRegion);
    setGameState(GameState::Loading);
}

void RustyRageGame::stopMatchmaking() {
    if (m_matchmaking) {
        m_matchmaking->stopSearch();
        m_matchmaking.reset();
        LOG_INFO("Stopped matchmaking");
    }
}

void RustyRageGame::startHosting(GameMode mode, PlayMode playMode, const std::string& mapName) {
    LOG_INFOF("Starting hosting: mode=%d, playMode=%d, map=%s", 
              (int)mode, (int)playMode, mapName.c_str());
    
    m_gameMode = mode;
    m_playMode = playMode;
    
    m_host = std::make_unique<Host>();
    
    Host::ServerConfig config;
    config.gameMode = (mode == GameMode::SpeedArena) ? Host::GameMode::SpeedArena : Host::GameMode::RoadAvengers;
    config.playMode = (playMode == PlayMode::Solo) ? Host::PlayMode::Solo : 
                     (playMode == PlayMode::Multi2) ? Host::PlayMode::Multi2 : Host::PlayMode::Multi4;
    config.mapName = mapName;
    
    if (!m_host->startServer(config)) {
        LOG_ERROR("Failed to start hosting");
        return;
    }
    
    setGameState(GameState::InGame);
}

void RustyRageGame::stopHosting() {
    if (m_host) {
        m_host->stopServer();
        m_host.reset();
        LOG_INFO("Stopped hosting");
    }
}

void RustyRageGame::refreshGameLibrary() {
    if (m_host) {
        m_host->refreshGameLibrary();
        m_availableGames = m_host->getAvailableGames();
        LOG_INFOF("Refreshed game library: %zu games available", m_availableGames.size());
    }
}

void RustyRageGame::loadTrack(const std::string& mapPath) {
    if (!m_currentTrack) {
        m_currentTrack = std::make_unique<Track>();
    }
    if (!m_currentTrack->load(mapPath)) {
        LOG_ERRORF("RustyRageGame::loadTrack: failed to load %s", mapPath.c_str());
        m_currentTrack.reset();
    }
}

void RustyRageGame::applySettings() {
    if (getConfig()) {
        getConfig()->saveToFile();
        LOG_INFO("Applied and saved settings");
    }
}

void RustyRageGame::resetSettings() {
    if (getConfig()) {
        getConfig()->resetToDefaults();
        getConfig()->saveToFile();
        LOG_INFO("Reset settings to defaults");
    }
}

// Private methods

void RustyRageGame::initializeGameSystems() {
    LOG_INFO("Initializing game systems");
    // Game systems initialization will be implemented later
}

void RustyRageGame::initializeScoreTable() {
    LOG_INFO("Initializing score tables");
    
    // Solo scoring (both modes)
    m_scoreTable[1] = 25;   // 1st place
    m_scoreTable[2] = 17;   // 2nd place
    m_scoreTable[3] = 10;   // 3rd place
    m_scoreTable[4] = 5;    // 4th place
    m_scoreTable[5] = -4;   // 5th place
    m_scoreTable[6] = -8;   // 6th place
    m_scoreTable[7] = -14;  // 7th place
    m_scoreTable[8] = -22;  // 8th place
    
    LOG_INFO("Score tables initialized");
}

void RustyRageGame::initializeModels() {
    LOG_INFO("Initializing 3D models");
    // Model loading will be implemented later
}

void RustyRageGame::updateMainMenu(float deltaTime) {
    // Main menu updates handled by UI
}

void RustyRageGame::updateInGame(float deltaTime) {
    m_physics->step(deltaTime);
    for (auto& vehicle : m_vehicles) {
        // Update vehicle physics (steering, engine force, etc.)
    }
    if (isGameTimeUp()) {
        processGameEnd();
    }
}

void RustyRageGame::updateLoading(float deltaTime) {
    if (m_matchmaking && m_matchmaking->isSearchComplete()) {
        setGameState(GameState::InGame);
        startSpeedArena();
    }
    
    // Используем getUI()
    if (getUI()) {
        getUI()->updateMatchmakingStatus("Searching for match...", m_matchmaking ? m_matchmaking->getSearchTime() : 0.0f, 0);
    }
}

void RustyRageGame::renderMainMenu() {
    // Main menu rendering handled by UI
}

void RustyRageGame::renderInGame() {
    // TODO: Render 3D world with Vulkan when implemented
    // For now, rendering is handled by SDL_Renderer in Application::render()

    if (m_currentTrack && m_currentTrack->isLoaded()) {
        // Minimal debug line so headless runs and first-frame sanity-checks
        // can confirm the map actually deserialised. Full track visualization
        // belongs to the Vulkan renderer (out of scope here).
        LOG_INFOF("Track loaded with %zu objects",
                  m_currentTrack->objectCount());
    }

    /* Vulkan rendering - to be implemented later
    for (const auto& model : m_models) {
        if (model.isValid()) {
            getRenderer()->renderModel(model, model.getTransform());
        }
    }

    // Render vehicles
    for (const auto& vehicle : m_vehicles) {
        if (vehicle.getModel() && vehicle.getModel()->isValid()) {
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), vehicle.getPosition()) *
                                 glm::mat4_cast(vehicle.getRotation());
            getRenderer()->renderModel(*vehicle.getModel(), transform);
        }
    }
    */
}

void RustyRageGame::renderLoading() {
    // Loading screen rendering handled by UI
}

void RustyRageGame::startSpeedArena() {
    LOG_INFO("Starting Speed Arena game mode");
    m_gameMode = GameMode::SpeedArena;
    m_gameTimer = 0.0f;
    m_gameEnded = false;
    // TODO: Initialize Speed Arena specific systems
}

void RustyRageGame::startRoadAvengers() {
    LOG_INFO("Starting Road Avengers game mode");
    m_gameMode = GameMode::RoadAvengers;
    m_gameTimer = 0.0f;
    m_gameEnded = false;
    // TODO: Initialize Road Avengers specific systems
}

void RustyRageGame::endGame() {
    LOG_INFO("Game ended");
    m_gameEnded = true;
    calculateScores();
    setGameState(GameState::MainMenu);
}

void RustyRageGame::spawnPlayer(uint32_t playerId, const glm::vec3& position) {
    LOG_INFOF("Spawning player %u at position (%.2f, %.2f, %.2f)", 
              playerId, position.x, position.y, position.z);
    
    // Create vehicle for player
    Vehicle vehicle(Vehicle::VehicleType::RustySedan, playerId);
    vehicle.setPosition(position);
    vehicle.setRespawnPosition(position);
    
    m_vehicles.push_back(vehicle);
}

void RustyRageGame::respawnPlayer(uint32_t playerId) {
    auto it = std::find_if(m_vehicles.begin(), m_vehicles.end(),
                           [playerId](const Vehicle& v) { return v.getPlayerId() == playerId; });
    if (it != m_vehicles.end()) {
        auto* rigidBody = it->getPhysicsBody();
        m_physics->respawnBody(static_cast<Core::Physics::RigidBody*>(rigidBody));
    }
}

void RustyRageGame::handlePlayerDeath(uint32_t playerId) {
    LOG_INFOF("Player %u died", playerId);
    
    // Find player's vehicle
    auto it = std::find_if(m_vehicles.begin(), m_vehicles.end(),
                          [playerId](const Vehicle& v) { return v.getPlayerId() == playerId; });
    
    if (it != m_vehicles.end()) {
        // Handle death logic based on game mode
        if (m_gameMode == GameMode::SpeedArena) {
            // Respawn after 2 seconds
            respawnPlayer(playerId);
        } else if (m_gameMode == GameMode::RoadAvengers) {
            // Player is eliminated
            it->takeDamage(1000.0f); // Ensure death
        }
    }
}

void RustyRageGame::handleInput() {
    auto input = getInputManager();
    
    if (input->isKeyJustPressed(SDL_SCANCODE_ESCAPE)) {
        if (m_gameState == GameState::InGame) {
            setGameState(GameState::Paused);
        } else if (m_gameState == GameState::Paused) {
            setGameState(GameState::InGame);
        }
    }
}

void RustyRageGame::handleNetworkEvents() {
    // Network event handling will be implemented later
}

glm::vec3 RustyRageGame::getSpawnPosition(int index) const {
    // Simple spawn positions in a circle
    float angle = (index * 2.0f * 3.14159f) / 8.0f;
    float radius = 10.0f;
    
    return glm::vec3(
        cos(angle) * radius,
        0.0f,
        sin(angle) * radius
    );
}

void RustyRageGame::updateGameTimer(float deltaTime) {
    m_gameTimer += deltaTime;
}

bool RustyRageGame::isGameTimeUp() const {
    return m_gameTimer >= m_maxGameTime;
}

void RustyRageGame::processGameEnd() {
    endGame();
}

void RustyRageGame::calculateScores() {
    LOG_INFO("Calculating game scores");
    
    m_scores.clear();
    
    // Simple scoring based on vehicle health and position
    std::vector<std::pair<uint32_t, float>> playerScores;
    
    for (const auto& vehicle : m_vehicles) {
        if (vehicle.isAlive()) {
            float score = vehicle.getHealth() + (m_maxGameTime - m_gameTimer);
            playerScores.push_back({vehicle.getPlayerId(), score});
        }
    }
    
    // Sort by score (descending)
    std::sort(playerScores.begin(), playerScores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Assign positions and points
    for (size_t i = 0; i < playerScores.size(); ++i) {
        ScoreEntry entry;
        entry.playerId = playerScores[i].first;
        entry.position = i + 1;
        entry.points = m_scoreTable[entry.position];
        entry.isGroup = false;
        
        m_scores.push_back(entry);
        
        LOG_INFOF("Player %u: position %d, points %d", 
                  entry.playerId, entry.position, entry.points);
    }
}

void RustyRageGame::setupUICallbacks() {
    LOG_INFO("Setting up UI callbacks");
    
    getUI()->setOnExitGame([this]() {
        LOG_INFO("Exit game requested");
        setRunning(false);
    });
    
    getUI()->setOnStartSinglePlayer([this](const std::string& gameMode) {
        LOG_INFOF("Starting single player game: %s", gameMode.c_str());
        if (gameMode == "speed_arena") {
            m_gameMode = GameMode::SpeedArena;
            m_playMode = PlayMode::Solo;
            startSpeedArena();
        } else if (gameMode == "road_avengers") {
            m_gameMode = GameMode::RoadAvengers;
            m_playMode = PlayMode::Solo;
            startRoadAvengers();
        }
    });
    
    getUI()->setOnStartMultiplayer([this](const std::string& gameMode) {
        LOG_INFOF("Starting multiplayer game: %s", gameMode.c_str());
        if (gameMode == "speed_arena") {
            m_gameMode = GameMode::SpeedArena;
            m_playMode = PlayMode::Multi2;
            startSpeedArena();
        } else if (gameMode == "road_avengers") {
            m_gameMode = GameMode::RoadAvengers;
            m_playMode = PlayMode::Multi2;
            startRoadAvengers();
        }
    });
    
    getUI()->setOnCheckPlayerGroup([this]() {
        return checkPlayerGroup();
    });
}

bool RustyRageGame::checkPlayerGroup() const {
    // For now, always return false to show the error message
    // In real implementation, this would check if 2-4 players are in the group
    return false;
}

} // namespace Game
