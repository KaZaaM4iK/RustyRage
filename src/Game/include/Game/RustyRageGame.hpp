#pragma once

#include "Core/Application.hpp"
// Remove Core/Renderer.hpp for now as we use SDL_Renderer
#include "Core/InputManager.hpp"
#include "Core/Config.hpp"
#include "Core/Camera.hpp"
#include "Core/Physics.hpp"
#include "Core/Model.hpp"
#include "Core/Logging.hpp"

#include "GameModes.hpp"
#include "Player.hpp"
#include "Vehicle.hpp"
#include "Track.hpp"
#include "UI.hpp"
#include "Matchmaking.hpp"
#include "Host.hpp"

#include <memory>
#include <vector>
#include <map>

// Forward declare SDL_Renderer
struct SDL_Renderer;

namespace Game {

class RustyRageGame : public Core::Application {
public:
    enum class GameState {
        MainMenu,
        SinglePlayer,
        Multiplayer,
        GameLibrary,
        Settings,
        InGame,
        Paused,
        Loading
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

    RustyRageGame(int width, int height, const std::string& title);
    ~RustyRageGame();

    // Core::Application overrides
    void initialize() override;
    void update(float deltaTime) override;
    void render() override;
    void shutdown() override;
    
    // Game state management
    void setGameState(GameState state);
    GameState getGameState() const { return m_gameState; }
    
    // Game mode management
    void setGameMode(GameMode mode) { m_gameMode = mode; }
    GameMode getGameMode() const { return m_gameMode; }
    
    void setPlayMode(PlayMode mode) { m_playMode = mode; }
    PlayMode getPlayMode() const { return m_playMode; }
    
    // Player management
    void addPlayer(const Player& player);
    void removePlayer(uint32_t playerId);
    Player* getPlayer(uint32_t playerId);
    const std::vector<Player>& getPlayers() const { return m_players; }
    
    // Scoring system
    struct ScoreEntry {
        uint32_t playerId;
        int position;
        int points;
        bool isGroup;
        std::vector<uint32_t> groupMembers; // For multiplayer groups
    };
    
    void calculateScores();
    const std::vector<ScoreEntry>& getScores() const { return m_scores; }
    
    // Matchmaking
    void startMatchmaking(GameMode mode, PlayMode playMode, const std::string& region = "RU");
    void stopMatchmaking();
    bool isMatchmaking() const { return m_matchmaking != nullptr; }
    
    // Game hosting
    void startHosting(GameMode mode, PlayMode playMode, const std::string& mapName = "");
    void stopHosting();
    bool isHosting() const { return m_host != nullptr; }

    // Track / map loading. Pure data layer — does not spawn vehicles or
    // initialize physics. Safe to call before or during InGame state.
    void loadTrack(const std::string& mapPath);
    bool hasTrack() const { return m_currentTrack != nullptr; }
    const Track* getTrack() const { return m_currentTrack.get(); }
    
    // Game library
    void refreshGameLibrary();
    const std::vector<Host::GameServer>& getAvailableGames() const { return m_availableGames; }
    
    // Settings
    void applySettings();
    void resetSettings();

private:
    // Game state
    GameState m_gameState;
    GameMode m_gameMode;
    PlayMode m_playMode;
    
    // Game systems
    std::unique_ptr<Core::Camera> m_camera;
    std::unique_ptr<Core::Physics> m_physics;
    std::unique_ptr<Matchmaking> m_matchmaking;
    std::unique_ptr<Host> m_host;

    // Currently loaded track. Optional — the game still runs without one,
    // but InGame rendering and physics spawn-points only activate after
    // loadTrack() succeeds.
    std::unique_ptr<Track> m_currentTrack;
    
    // Game objects
    std::vector<Player> m_players;
    std::vector<Vehicle> m_vehicles;
    std::vector<Core::Model> m_models;
    
    // Game library
    std::vector<Host::GameServer> m_availableGames;
    
    // Scoring
    std::vector<ScoreEntry> m_scores;
    std::map<int, int> m_scoreTable; // Position -> Points mapping
    
    // Game timing
    float m_gameTimer;
    float m_maxGameTime;
    bool m_gameEnded;
    
    // Initialize game systems
    void initializeGameSystems();
    void initializeScoreTable();
    void initializeModels();
    
    // Game loop methods
    void updateMainMenu(float deltaTime);
    void updateInGame(float deltaTime);
    void updateLoading(float deltaTime);
    
    void renderMainMenu();
    void renderInGame();
    void renderLoading();
    
    // Game mode specific
    void startSpeedArena();
    void startRoadAvengers();
    void endGame();
    
    // Player management
    void spawnPlayer(uint32_t playerId, const glm::vec3& position);
    void respawnPlayer(uint32_t playerId);
    void handlePlayerDeath(uint32_t playerId);
    
    // Event handling
    void handleInput();
    void handleNetworkEvents();
    
    // Helper methods
    glm::vec3 getSpawnPosition(int index) const;
    void updateGameTimer(float deltaTime);
    bool isGameTimeUp() const;
    void processGameEnd();
    
    // UI callbacks setup
    void setupUICallbacks();
    bool checkPlayerGroup() const;
};

} // namespace Game
