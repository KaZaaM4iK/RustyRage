#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h> // For rendering text
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "Core/InputManager.hpp" // Включаем полный заголовок InputManager

namespace Game {

class UI {
public:
    enum class Screen {
        MainMenu,
        SinglePlayer,
        Multiplayer,
        GameLibrary,
        Settings,
        InGame,
        Paused,
        Loading,
        Matchmaking,
        GameResults
    };
    
    enum class ButtonState {
        Normal,
        Hovered,
        Pressed,
        Disabled
    };
    
    struct Button {
        std::string id;
        std::string text;
        glm::vec2 position;
        glm::vec2 size;
        ButtonState state;
        std::function<void()> onClick;
        bool isVisible;
        glm::vec3 color;
        glm::vec3 hoverColor;
        glm::vec3 pressedColor;
        bool isEnabled;
        
        Button() : state(ButtonState::Normal), isVisible(true), isEnabled(true),
                color(0.3f, 0.3f, 0.3f), hoverColor(0.4f, 0.4f, 0.4f), pressedColor(0.2f, 0.2f, 0.2f) {}
    };
    
    struct Slider {
        std::string id;
        std::string label;
        glm::vec2 position;
        glm::vec2 size;
        float value;
        float minValue;
        float maxValue;
        std::function<void(float)> onChange;
        bool isVisible;
        
        Slider() : value(0.5f), minValue(0.0f), maxValue(1.0f), isVisible(true) {}
    };
    
    struct TouchControl {
        std::string id;
        glm::vec2 center;
        float radius;
        bool isActive;
        glm::vec2 currentPosition;
        glm::vec2 value; // Normalized -1 to 1
        int touchId; // Добавляем поле для отслеживания ID касания
        
        TouchControl() : radius(50.0f), isActive(false), value(0.0f), touchId(-1) {}
    };

    UI();
    ~UI();

    // Initialization with SDL_Renderer and SDL_Window
    bool initialize(SDL_Renderer* sdlRenderer, SDL_Window* sdlWindow, Core::InputManager* inputManager);
    void shutdown();
    
    // Screen management
    void setCurrentScreen(Screen screen);
    Screen getCurrentScreen() const { return m_currentScreen; }
    
    // Update and render
    void update(float deltaTime);
    void render();
    
    // Main menu
    void renderMainMenu();
    void createMainMenuButtons();
    
    // Single player menu
    void renderSinglePlayerMenu();
    void createSinglePlayerButtons();
    
    // Multiplayer menu
    void renderMultiplayerMenu();
    void createMultiplayerButtons();
    
    // Game library
    void renderGameLibrary();
    void createGameLibraryUI();
    void updateGameLibraryList(const std::vector<std::string>& games);
    
    // Settings
    void renderSettings();
    void createSettingsUI();
    void applySettings();
    
    // In-game HUD
    void renderInGameHUD();
    void updateHUDData(const std::string& playerName, int position, float time, 
                      int health, int ammo, const std::string& powerUp);
    
    // Matchmaking
    void renderMatchmaking();
    void updateMatchmakingStatus(const std::string& status, float searchTime, int playersFound);
    
    // Loading screen
    void renderLoading(const std::string& message = "Loading...");
    
    // Game results
    void renderGameResults(const std::vector<std::string>& results);
    
    // Button management
    void addButton(const Button& button);
    void removeButton(const std::string& id);
    Button* getButton(const std::string& id);
    void updateButtons();
    void renderButtons();
    
    // Slider management
    void addSlider(const Slider& slider);
    void removeSlider(const std::string& id);
    Slider* getSlider(const std::string& id);
    void updateSliders();
    void renderSliders();
    
    // Touch controls (mobile)
    void enableTouchControls(bool enable) { m_touchControlsEnabled = enable; }
    bool areTouchControlsEnabled() const { return m_touchControlsEnabled; }
    
    void addTouchControl(const TouchControl& control);
    void removeTouchControl(const std::string& id);
    TouchControl* getTouchControl(const std::string& id);
    void updateTouchControls();
    void renderTouchControls();
    
    // Virtual joysticks
    void createVirtualJoysticks();
    glm::vec2 getVirtualJoystickValue(const std::string& id) const;
    
    // Text rendering
    void renderText(const std::string& text, const glm::vec2& position, 
                   const glm::vec3& color = glm::vec3(1.0f), float scale = 1.0f);
    void renderCenteredText(const std::string& text, const glm::vec2& position,
                           const glm::vec3& color = glm::vec3(1.0f), float scale = 1.0f);
    
    // Utility methods
    bool isPointInRect(const glm::vec2& point, const glm::vec2& position, const glm::vec2& size) const;
    glm::vec2 screenToNormalized(const glm::vec2& screenPos) const;
    glm::vec2 normalizedToScreen(const glm::vec2& normalizedPos) const;
    
    // Event handling
    void handleMouseClick(const glm::vec2& position);
    void handleMouseMove(const glm::vec2& position);
    void handleTouchEvent(const glm::vec2& position, bool isDown, int touchId);
    
    // Styling
    struct UIStyle {
        glm::vec3 backgroundColor;
        glm::vec3 buttonColor;
        glm::vec3 buttonHoverColor;
        glm::vec3 buttonPressedColor;
        glm::vec3 textColor;
        glm::vec3 sliderColor;
        glm::vec3 sliderHandleColor;
        float buttonBorderRadius;
        float sliderBorderRadius;
        float fontSize;
        
        UIStyle() : backgroundColor(0.1f, 0.1f, 0.1f), buttonColor(0.3f, 0.3f, 0.3f),
                   buttonHoverColor(0.4f, 0.4f, 0.4f), buttonPressedColor(0.2f, 0.2f, 0.2f),
                   textColor(1.0f, 1.0f, 1.0f), sliderColor(0.2f, 0.2f, 0.2f),
                   sliderHandleColor(0.5f, 0.5f, 0.5f), buttonBorderRadius(5.0f),
                   sliderBorderRadius(3.0f), fontSize(16.0f) {}
    };
    
    void setStyle(const UIStyle& style) { m_style = style; }
    const UIStyle& getStyle() const { return m_style; }
    
    // Callback setters
    void setOnExitGame(std::function<void()> callback) { m_onExitGame = callback; }
    void setOnStartSinglePlayer(std::function<void(const std::string&)> callback) { m_onStartSinglePlayer = callback; }
    void setOnStartMultiplayer(std::function<void(const std::string&)> callback) { m_onStartMultiplayer = callback; }
    void setOnCheckPlayerGroup(std::function<bool()> callback) { m_onCheckPlayerGroup = callback; }

private:
    SDL_Renderer* m_sdlRenderer;
    SDL_Window* m_sdlWindow; // Добавляем указатель на окно
    Core::InputManager* m_inputManager;
    TTF_Font* m_font;
    
    Screen m_currentScreen;
    UIStyle m_style;
    
    // UI elements
    std::vector<Button> m_buttons;
    std::vector<Slider> m_sliders;
    std::vector<TouchControl> m_touchControls;
    
    // Touch controls
    bool m_touchControlsEnabled;
    std::vector<TouchControl> m_virtualJoysticks;
    
    // HUD data
    struct HUDData {
        std::string playerName;
        int position;
        float raceTime;
        int health;
        int ammo;
        std::string powerUp;
        bool isVisible;
        
        HUDData() : position(0), raceTime(0.0f), health(100), ammo(0), isVisible(true) {}
    } m_hudData;
    
    // Matchmaking data
    struct MatchmakingData {
        std::string status;
        float searchTime;
        int playersFound;
        bool isVisible;
        
        MatchmakingData() : searchTime(0.0f), playersFound(0), isVisible(false) {}
    } m_matchmakingData;
    
    // Screen-specific data
    std::vector<std::string> m_gameLibraryList;
    std::vector<std::string> m_gameResults;
    std::string m_errorMessage;
    bool m_showError;
    
    // Callbacks
    std::function<void()> m_onExitGame;
    std::function<void(const std::string&)> m_onStartSinglePlayer;
    std::function<void(const std::string&)> m_onStartMultiplayer;
    std::function<bool()> m_onCheckPlayerGroup;
    
    // Helper methods
    void clearUI();
    void setupScreen(Screen screen);
    void renderBackground();
    void renderButton(const Button& button);
    void renderSlider(const Slider& slider);
    void renderTouchControl(const TouchControl& control);
    
    // Input handling
    void handleButtonClick(const glm::vec2& position);
    void handleSliderDrag(const glm::vec2& position);
    void handleTouchControl(const glm::vec2& position, bool isDown, int touchId);
    
    // Mobile-specific
    void createMobileUI();
    void updateMobileControls();
    void renderMobileControls();
    
    // Font and text rendering
    bool initializeFonts();
    void cleanupFonts();
    void renderText(const std::string& text, const glm::vec2& position, const SDL_Color& color, bool centered = false);
    
    // Animation
    void updateAnimations(float deltaTime);
    void animateButton(Button& button, float deltaTime);
    void animateSlider(Slider& slider, float deltaTime);
};

} // namespace Game