#include "Game/UI.hpp"
#include "Core/Logging.hpp"
#include <algorithm>

namespace Game {

UI::UI() 
    : m_sdlRenderer(nullptr),
      m_sdlWindow(nullptr),
      m_inputManager(nullptr),
      m_font(nullptr),
      m_currentScreen(Screen::MainMenu),
      m_touchControlsEnabled(true) {
}

UI::~UI() {
    shutdown();
}

bool UI::initialize(SDL_Renderer* sdlRenderer, SDL_Window* sdlWindow, Core::InputManager* inputManager) {
    LOG_INFO("Initializing UI system");
    
    m_sdlRenderer = sdlRenderer;
    m_sdlWindow = sdlWindow;
    m_inputManager = inputManager;
    
    if (!m_sdlRenderer || !m_sdlWindow || !m_inputManager) {
        LOG_ERROR("Invalid renderer, window, or input manager");
        return false;
    }
    
    if (!initializeFonts()) {
        return false;
    }
    
    createMainMenuButtons();
    m_inputManager->enableMobileMode(true);
    createVirtualJoysticks();
    
    LOG_INFO("UI system initialized successfully");
    return true;
}

void UI::shutdown() {
    LOG_INFO("Shutting down UI system");
    cleanupFonts();
    clearUI();
    m_sdlRenderer = nullptr;
    m_sdlWindow = nullptr;
    m_inputManager = nullptr;
}

void UI::setCurrentScreen(Screen screen) {
    if (static_cast<int>(screen) < 0 || static_cast<int>(screen) > static_cast<int>(Screen::GameResults)) {
        LOG_ERRORF("Invalid screen value: %d", static_cast<int>(screen));
        return;
    }
    LOG_INFOF("Changing UI screen from %d to %d", static_cast<int>(m_currentScreen), static_cast<int>(screen));
    m_currentScreen = screen;
    setupScreen(screen);
}

void UI::update(float deltaTime) {
    LOG_INFOF("Current screen at start of update: %d", static_cast<int>(m_currentScreen));
    updateButtons();
    updateSliders();
    updateTouchControls();

    glm::vec2 mousePos = m_inputManager->getMousePosition();
    bool isClicked = m_inputManager->isMouseButtonJustPressed(SDL_BUTTON_LEFT);
    if (isClicked) {
        LOG_INFOF("Mouse button check: isClicked=%d at (%.0f, %.0f)", isClicked, mousePos.x, mousePos.y);
        handleButtonClick(mousePos);
    }
    
    handleMouseMove(mousePos);
}

void UI::render() {
    switch (m_currentScreen) {
        case Screen::MainMenu:
            renderMainMenu();
            break;
        case Screen::SinglePlayer:
            renderSinglePlayerMenu();
            break;
        case Screen::Multiplayer:
            renderMultiplayerMenu();
            break;
        case Screen::GameLibrary:
            renderGameLibrary();
            break;
        case Screen::Settings:
            renderSettings();
            break;
        case Screen::InGame:
            renderInGameHUD();
            break;
        case Screen::Matchmaking:
            renderMatchmaking();
            break;
        case Screen::Loading:
            renderLoading();
            break;
        case Screen::GameResults:
            renderGameResults(m_gameResults);
            break;
        default:
            break;
    }
    
    renderButtons();
    renderSliders();
    
    if (m_touchControlsEnabled) {
        renderTouchControls();
    }
}

void UI::renderMainMenu() {
    // Заголовок
    renderText("Rusty Rage", { (float)1280 / 2, 150.0f }, {220, 180, 150, 255}, true);
    renderButtons();
}

void UI::createMainMenuButtons() {
    clearUI();
    
    Button singlePlayerBtn;
    singlePlayerBtn.id = "single_player";
    singlePlayerBtn.text = "Одиночная игра";
    singlePlayerBtn.position = glm::vec2(640, 300);
    singlePlayerBtn.size = glm::vec2(400, 50);
    singlePlayerBtn.onClick = [this]() { 
        LOG_INFO("Single Player button clicked");
        setCurrentScreen(Screen::SinglePlayer); 
    };
    singlePlayerBtn.color = m_style.buttonColor;
    singlePlayerBtn.hoverColor = m_style.buttonHoverColor;
    singlePlayerBtn.pressedColor = m_style.buttonPressedColor;
    addButton(singlePlayerBtn);
    
    Button multiplayerBtn;
    multiplayerBtn.id = "multiplayer";
    multiplayerBtn.text = "Мультиплеер";
    multiplayerBtn.position = glm::vec2(640, 360);
    multiplayerBtn.size = glm::vec2(400, 50);
    multiplayerBtn.onClick = [this]() { 
        LOG_INFO("Multiplayer button clicked");
        setCurrentScreen(Screen::Multiplayer); 
    };
    multiplayerBtn.color = m_style.buttonColor;
    multiplayerBtn.hoverColor = m_style.buttonHoverColor;
    multiplayerBtn.pressedColor = m_style.buttonPressedColor;
    addButton(multiplayerBtn);
    
    Button gameLibraryBtn;
    gameLibraryBtn.id = "game_library";
    gameLibraryBtn.text = "Игротека";
    gameLibraryBtn.position = glm::vec2(640, 420);
    gameLibraryBtn.size = glm::vec2(400, 50);
    gameLibraryBtn.onClick = [this]() {
        LOG_INFO("Game Library button clicked");
        setCurrentScreen(Screen::GameLibrary);
    };
    gameLibraryBtn.color = m_style.buttonColor;
    gameLibraryBtn.hoverColor = m_style.buttonHoverColor;
    gameLibraryBtn.pressedColor = m_style.buttonPressedColor;
    addButton(gameLibraryBtn);
    
    Button settingsBtn;
    settingsBtn.id = "settings";
    settingsBtn.text = "Настройки";
    settingsBtn.position = glm::vec2(640, 480);
    settingsBtn.size = glm::vec2(400, 50);
    settingsBtn.onClick = [this]() {
        LOG_INFO("Settings button clicked");
        setCurrentScreen(Screen::Settings);
    };
    settingsBtn.color = m_style.buttonColor;
    settingsBtn.hoverColor = m_style.buttonHoverColor;
    settingsBtn.pressedColor = m_style.buttonPressedColor;
    addButton(settingsBtn);
    
    Button exitBtn;
    exitBtn.id = "exit";
    exitBtn.text = "Выход";
    exitBtn.position = glm::vec2(640, 540);
    exitBtn.size = glm::vec2(400, 50);
    exitBtn.onClick = [this]() {
        LOG_INFO("Exit button clicked - QUIT NOW!");
        SDL_Event quitEvent{};
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    };
    exitBtn.color = m_style.buttonColor;
    exitBtn.hoverColor = m_style.buttonHoverColor;
    exitBtn.pressedColor = m_style.buttonPressedColor;
    addButton(exitBtn);
}

void UI::renderSinglePlayerMenu() {
}

void UI::createSinglePlayerButtons() {
    clearUI();
    
    Button speedArenaBtn;
    speedArenaBtn.id = "speed_arena";
    speedArenaBtn.text = "Арена Скорости";
    speedArenaBtn.position = glm::vec2(640, 300);
    speedArenaBtn.size = glm::vec2(400, 50);
    speedArenaBtn.onClick = [this]() {
        LOG_INFO("Speed Arena selected for single player");
        if (m_onStartSinglePlayer) {
            m_onStartSinglePlayer("speed_arena");
        }
        setCurrentScreen(Screen::Matchmaking);
    };
    speedArenaBtn.color = m_style.buttonColor;
    speedArenaBtn.hoverColor = m_style.buttonHoverColor;
    speedArenaBtn.pressedColor = m_style.buttonPressedColor;
    addButton(speedArenaBtn);
    
    Button roadAvengersBtn;
    roadAvengersBtn.id = "road_avengers";
    roadAvengersBtn.text = "Мстители Дороги";
    roadAvengersBtn.position = glm::vec2(640, 360);
    roadAvengersBtn.size = glm::vec2(400, 50);
    roadAvengersBtn.onClick = [this]() {
        LOG_INFO("Road Avengers selected for single player");
        if (m_onStartSinglePlayer) {
            m_onStartSinglePlayer("road_avengers");
        }
        setCurrentScreen(Screen::Matchmaking);
    };
    roadAvengersBtn.color = m_style.buttonColor;
    roadAvengersBtn.hoverColor = m_style.buttonHoverColor;
    roadAvengersBtn.pressedColor = m_style.buttonPressedColor;
    addButton(roadAvengersBtn);
    
    Button backBtn;
    backBtn.id = "back";
    backBtn.text = "Назад";
    backBtn.position = glm::vec2(640, 420);
    backBtn.size = glm::vec2(400, 50);
    backBtn.onClick = [this]() {
        LOG_INFO("Back to main menu");
        setCurrentScreen(Screen::MainMenu);
    };
    backBtn.color = m_style.buttonColor;
    backBtn.hoverColor = m_style.buttonHoverColor;
    backBtn.pressedColor = m_style.buttonPressedColor;
    addButton(backBtn);
}

void UI::renderMultiplayerMenu() {
}

void UI::createMultiplayerButtons() {
    clearUI();
    
    Button speedArenaBtn;
    speedArenaBtn.id = "speed_arena";
    speedArenaBtn.text = "Арена Скорости";
    speedArenaBtn.position = glm::vec2(640, 300);
    speedArenaBtn.size = glm::vec2(400, 50);
    speedArenaBtn.onClick = [this]() {
        LOG_INFO("Speed Arena selected for multiplayer");
        if (m_onCheckPlayerGroup && !m_onCheckPlayerGroup()) {
            m_errorMessage = "Недостаточно игроков в группе";
            m_showError = true;
            return;
        }
        if (m_onStartMultiplayer) {
            m_onStartMultiplayer("speed_arena");
        }
        setCurrentScreen(Screen::Matchmaking);
    };
    speedArenaBtn.color = m_style.buttonColor;
    speedArenaBtn.hoverColor = m_style.buttonHoverColor;
    speedArenaBtn.pressedColor = m_style.buttonPressedColor;
    addButton(speedArenaBtn);
    
    Button roadAvengersBtn;
    roadAvengersBtn.id = "road_avengers";
    roadAvengersBtn.text = "Мстители Дороги";
    roadAvengersBtn.position = glm::vec2(640, 360);
    roadAvengersBtn.size = glm::vec2(400, 50);
    roadAvengersBtn.onClick = [this]() {
        LOG_INFO("Road Avengers selected for multiplayer");
        if (m_onCheckPlayerGroup && !m_onCheckPlayerGroup()) {
            m_errorMessage = "Недостаточно игроков в группе";
            m_showError = true;
            return;
        }
        if (m_onStartMultiplayer) {
            m_onStartMultiplayer("road_avengers");
        }
        setCurrentScreen(Screen::Matchmaking);
    };
    roadAvengersBtn.color = m_style.buttonColor;
    roadAvengersBtn.hoverColor = m_style.buttonHoverColor;
    roadAvengersBtn.pressedColor = m_style.buttonPressedColor;
    addButton(roadAvengersBtn);
    
    Button backBtn;
    backBtn.id = "back";
    backBtn.text = "Назад";
    backBtn.position = glm::vec2(640, 420);
    backBtn.size = glm::vec2(400, 50);
    backBtn.onClick = [this]() {
        LOG_INFO("Back to main menu");
        setCurrentScreen(Screen::MainMenu);
    };
    backBtn.color = m_style.buttonColor;
    backBtn.hoverColor = m_style.buttonHoverColor;
    backBtn.pressedColor = m_style.buttonPressedColor;
    addButton(backBtn);
}

void UI::renderGameLibrary() {
}

void UI::createGameLibraryUI() {
    clearUI();
    
    Button backBtn;
    backBtn.id = "back";
    backBtn.text = "Назад";
    backBtn.position = glm::vec2(640, 420);
    backBtn.size = glm::vec2(400, 50);
    backBtn.onClick = [this]() {
        LOG_INFO("Back to main menu from game library");
        setCurrentScreen(Screen::MainMenu);
    };
    backBtn.color = m_style.buttonColor;
    backBtn.hoverColor = m_style.buttonHoverColor;
    backBtn.pressedColor = m_style.buttonPressedColor;
    addButton(backBtn);
}

void UI::updateGameLibraryList(const std::vector<std::string>& games) {
    m_gameLibraryList = games;
}

void UI::renderSettings() {
}

void UI::createSettingsUI() {
    clearUI();
    
    Slider graphicsSlider;
    graphicsSlider.id = "graphics_quality";
    graphicsSlider.label = "Качество графики";
    graphicsSlider.position = glm::vec2(640, 300);
    graphicsSlider.size = glm::vec2(400, 20);
    graphicsSlider.value = 0.5f;
    addSlider(graphicsSlider);
    
    Slider audioSlider;
    audioSlider.id = "audio_volume";
    audioSlider.label = "Громкость";
    audioSlider.position = glm::vec2(640, 350);
    audioSlider.size = glm::vec2(400, 20);
    audioSlider.value = 0.8f;
    addSlider(audioSlider);
    
    Button regionBtn;
    regionBtn.id = "region";
    regionBtn.text = "Регион: Россия";
    regionBtn.position = glm::vec2(640, 400);
    regionBtn.size = glm::vec2(400, 50);
    addButton(regionBtn);
    
    Button backBtn;
    backBtn.id = "back";
    backBtn.text = "Назад";
    backBtn.position = glm::vec2(640, 500);
    backBtn.size = glm::vec2(400, 50);
    backBtn.onClick = [this]() { 
        LOG_INFO("Back button clicked");
        setCurrentScreen(Screen::MainMenu); 
    };
    addButton(backBtn);
}

void UI::applySettings() {
    LOG_INFO("Settings applied");
}

void UI::renderInGameHUD() {
    renderText("Player: " + m_hudData.playerName, glm::vec2(50, 50));
    renderText("Position: " + std::to_string(m_hudData.position), glm::vec2(50, 80));
    renderText("Time: " + std::to_string((int)m_hudData.raceTime), glm::vec2(50, 110));
    renderText("Health: " + std::to_string(m_hudData.health), glm::vec2(50, 140));
    renderText("Ammo: " + std::to_string(m_hudData.ammo), glm::vec2(50, 170));
    
    if (!m_hudData.powerUp.empty()) {
        renderText("Power-up: " + m_hudData.powerUp, glm::vec2(50, 200));
    }
}

void UI::updateHUDData(const std::string& playerName, int position, float time, 
                      int health, int ammo, const std::string& powerUp) {
    m_hudData.playerName = playerName;
    m_hudData.position = position;
    m_hudData.raceTime = time;
    m_hudData.health = health;
    m_hudData.ammo = ammo;
    m_hudData.powerUp = powerUp;
}

void UI::renderMatchmaking() {
    renderText("Поиск матча...", glm::vec2(640, 300));
    renderText(m_matchmakingData.status, glm::vec2(640, 330));
    renderText("Время поиска: " + std::to_string((int)m_matchmakingData.searchTime) + "с", glm::vec2(640, 360));
    
    if (m_matchmakingData.playersFound > 0) {
        renderText("Найдено игроков: " + std::to_string(m_matchmakingData.playersFound), glm::vec2(640, 390));
    }
}

void UI::updateMatchmakingStatus(const std::string& status, float searchTime, int playersFound) {
    m_matchmakingData.status = status;
    m_matchmakingData.searchTime = searchTime;
    m_matchmakingData.playersFound = playersFound;
    m_matchmakingData.isVisible = true;
}

void UI::renderLoading(const std::string& message) {
    renderCenteredText(message, glm::vec2(640, 360));
}

void UI::renderGameResults(const std::vector<std::string>& results) {
    float y = 200;
    for (const auto& result : results) {
        renderCenteredText(result, glm::vec2(640, y));
        y += 30;
    }
}

void UI::addButton(const Button& button) {
    m_buttons.push_back(button);
}

void UI::removeButton(const std::string& id) {
    auto it = std::find_if(m_buttons.begin(), m_buttons.end(),
                          [&id](const Button& btn) { return btn.id == id; });
    if (it != m_buttons.end()) {
        m_buttons.erase(it);
    }
}

void UI::updateButtons() {
    glm::vec2 mousePos = m_inputManager->getMousePosition();
    handleMouseMove(mousePos);
}

void UI::renderButtons() {
    for (const auto& button : m_buttons) {
        renderButton(button);
    }
}

void UI::addSlider(const Slider& slider) {
    m_sliders.push_back(slider);
}

void UI::removeSlider(const std::string& id) {
    auto it = std::find_if(m_sliders.begin(), m_sliders.end(),
                          [&id](const Slider& slider) { return slider.id == id; });
    if (it != m_sliders.end()) {
        m_sliders.erase(it);
    }
}

void UI::updateSliders() {
}

void UI::renderSliders() {
    for (const auto& slider : m_sliders) {
        if (slider.isVisible) {
            renderSlider(slider);
        }
    }
}

void UI::addTouchControl(const TouchControl& control) {
    m_touchControls.push_back(control);
}

void UI::removeTouchControl(const std::string& id) {
    auto it = std::find_if(m_touchControls.begin(), m_touchControls.end(),
                          [&id](const TouchControl& control) { return control.id == id; });
    if (it != m_touchControls.end()) {
        m_touchControls.erase(it);
    }
}

void UI::updateTouchControls() {
}

void UI::renderTouchControls() {
    for (const auto& control : m_touchControls) {
        if (control.isActive) {
            renderTouchControl(control);
        }
    }
}

void UI::createVirtualJoysticks() {
    TouchControl leftJoystick;
    leftJoystick.id = "left_joystick";
    leftJoystick.center = glm::vec2(150, 500);
    leftJoystick.radius = 60.0f;
    leftJoystick.touchId = -1;
    addTouchControl(leftJoystick);
    
    TouchControl rightJoystick;
    rightJoystick.id = "right_joystick";
    rightJoystick.center = glm::vec2(1130, 500);
    rightJoystick.radius = 60.0f;
    rightJoystick.touchId = -1;
    addTouchControl(rightJoystick);
}

glm::vec2 UI::getVirtualJoystickValue(const std::string& id) const {
    auto it = std::find_if(m_touchControls.begin(), m_touchControls.end(),
                          [&id](const TouchControl& control) { return control.id == id; });
    if (it != m_touchControls.end() && it->isActive) {
        return it->value;
    }
    return glm::vec2(0.0f);
}

void UI::renderText(const std::string& text, const glm::vec2& position, const SDL_Color& color, bool centered) {
    if (!m_font || !m_sdlRenderer) return;

    SDL_Surface* surface = TTF_RenderText_Blended(m_font, text.c_str(), text.size(), color);
    if (!surface) {
        LOG_WARNINGF("Failed to create text surface: %s", SDL_GetError());
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_sdlRenderer, surface);
    if (!texture) {
        LOG_WARNINGF("Failed to create text texture: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return;
    }

    SDL_FRect destRect;
    destRect.w = static_cast<float>(surface->w);
    destRect.h = static_cast<float>(surface->h);
    destRect.x = static_cast<float>(position.x);
    destRect.y = static_cast<float>(position.y);

    if (centered) {
        destRect.x -= destRect.w / 2.f;
        destRect.y -= destRect.h / 2.f;
    }

    SDL_RenderTexture(m_sdlRenderer, texture, nullptr, &destRect);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void UI::renderText(const std::string& text, const glm::vec2& position, 
                    const glm::vec3& color, float scale) {
    SDL_Color sdlColor = {
        static_cast<Uint8>(color.r * 255.0f),
        static_cast<Uint8>(color.g * 255.0f),
        static_cast<Uint8>(color.b * 255.0f),
        255
    };
    
    renderText(text, position, sdlColor, false);
}

void UI::renderCenteredText(const std::string& text, const glm::vec2& position,
                           const glm::vec3& color, float scale) {
    SDL_Color sdlColor = {
        static_cast<Uint8>(color.r * 255.0f),
        static_cast<Uint8>(color.g * 255.0f),
        static_cast<Uint8>(color.b * 255.0f),
        255
    };
    
    renderText(text, position, sdlColor, true);
}

void UI::renderButton(const Button& button) {
    if (!m_sdlRenderer || !button.isVisible || !button.isEnabled) return;

    glm::vec3 color;
    switch (button.state) {
        case ButtonState::Hovered:
            color = button.hoverColor;
            break;
        case ButtonState::Pressed:
            color = button.pressedColor;
            break;
        default:
            color = button.color;
    }
    if (color == glm::vec3(0.0f)) color = m_style.buttonColor;
    if (button.state == ButtonState::Hovered && button.hoverColor == glm::vec3(0.0f)) {
        color = m_style.buttonHoverColor;
    }
    if (button.state == ButtonState::Pressed && button.pressedColor == glm::vec3(0.0f)) {
        color = m_style.buttonPressedColor;
    }

    SDL_SetRenderDrawColor(m_sdlRenderer,
                           static_cast<Uint8>(color.r * 255.0f),
                           static_cast<Uint8>(color.g * 255.0f),
                           static_cast<Uint8>(color.b * 255.0f),
                           255);

    SDL_FRect rect = { button.position.x - button.size.x / 2.0f, button.position.y - button.size.y / 2.0f,
                       button.size.x, button.size.y };
    SDL_RenderFillRect(m_sdlRenderer, &rect);

    SDL_Color textColor = { static_cast<Uint8>(m_style.textColor.r * 255.0f),
                            static_cast<Uint8>(m_style.textColor.g * 255.0f),
                            static_cast<Uint8>(m_style.textColor.b * 255.0f), 255 };
    renderText(button.text, button.position, textColor, true);
}

bool UI::isPointInRect(const glm::vec2& point, const glm::vec2& position, const glm::vec2& size) const {
    glm::vec2 topLeft = position - size / 2.0f;
    glm::vec2 bottomRight = position + size / 2.0f;
    return point.x >= topLeft.x && point.x <= bottomRight.x &&
           point.y >= topLeft.y && point.y <= bottomRight.y;
}

glm::vec2 UI::screenToNormalized(const glm::vec2& screenPos) const {
    int windowWidth, windowHeight;
    SDL_GetWindowSize(m_sdlWindow, &windowWidth, &windowHeight);
    return glm::vec2(screenPos.x / windowWidth, screenPos.y / windowHeight);
}

glm::vec2 UI::normalizedToScreen(const glm::vec2& normalizedPos) const {
    int windowWidth, windowHeight;
    SDL_GetWindowSize(m_sdlWindow, &windowWidth, &windowHeight);
    return glm::vec2(normalizedPos.x * windowWidth, normalizedPos.y * windowHeight);
}

void UI::handleMouseClick(const glm::vec2& position) {
    handleButtonClick(position);
    handleSliderDrag(position);
}

void UI::handleMouseMove(const glm::vec2& position) {
    for (auto& button : m_buttons) {
        if (!button.isVisible || !button.isEnabled) continue;
        if (isPointInRect(position, button.position, button.size)) {
            button.state = ButtonState::Hovered;
        } else {
            button.state = ButtonState::Normal;
        }
    }
}

void UI::handleButtonClick(const glm::vec2& position) {
    LOG_INFOF("Handling button click at (%.0f, %.0f)", position.x, position.y);
    for (auto& button : m_buttons) {
        if (!button.isVisible || !button.isEnabled) continue;
        if (isPointInRect(position, button.position, button.size)) {
            button.state = ButtonState::Pressed;
            if (button.onClick) {
                LOG_INFOF("Button %s clicked", button.id.c_str());
                button.onClick();
            }
            button.state = ButtonState::Normal;
            break;
        }
    }
}

void UI::handleTouchControl(const glm::vec2& position, bool isDown, int touchId) {
    if (!m_sdlWindow) {
        LOG_ERROR("SDL window is null in handleTouchControl");
        return;
    }
    for (auto& control : m_touchControls) {
        float distance = glm::length(position - control.center);
        if (distance < control.radius) {
            control.isActive = isDown;
            control.currentPosition = position;
            control.value = (position - control.center) / control.radius;
            if (glm::length(control.value) > 1.0f) {
                control.value = glm::normalize(control.value);
            }
            control.touchId = touchId;
        } else if (control.touchId == touchId) {
            control.isActive = false;
            control.value = glm::vec2(0.0f);
            control.touchId = -1;
        }
    }

    if (isDown) {
        for (auto& button : m_buttons) {
            if (!button.isVisible || !button.isEnabled) continue;
            if (isPointInRect(position, button.position, button.size)) {
                button.state = ButtonState::Pressed;
                if (button.onClick) {
                    LOG_INFOF("Button %s touched", button.id.c_str());
                    button.onClick();
                }
                button.state = ButtonState::Normal;
                break;
            }
        }
    }
}

void UI::clearUI() {
    m_buttons.clear();
    m_sliders.clear();
    m_touchControls.clear();
}

void UI::setupScreen(Screen screen) {
    switch (screen) {
        case Screen::MainMenu:
            createMainMenuButtons();
            break;
        case Screen::SinglePlayer:
            createSinglePlayerButtons();
            break;
        case Screen::Multiplayer:
            createMultiplayerButtons();
            break;
        case Screen::GameLibrary:
            createGameLibraryUI();
            break;
        case Screen::Settings:
            createSettingsUI();
            break;
        default:
            break;
    }
}

void UI::renderBackground() {
}

void UI::renderSlider(const Slider& slider) {
}

void UI::renderTouchControl(const TouchControl& control) {
    if (!m_sdlRenderer || !control.isActive) return;

    SDL_SetRenderDrawColor(m_sdlRenderer, 100, 100, 100, 100);
    const int segments = 32;
    std::vector<SDL_FPoint> points;
    for (int i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) / segments * 2.0f * 3.14159f;
        points.push_back({
            control.center.x + static_cast<float>(cos(angle)) * control.radius,
            control.center.y + static_cast<float>(sin(angle)) * control.radius
        });
    }
    SDL_RenderLines(m_sdlRenderer, points.data(), points.size());

    SDL_SetRenderDrawColor(m_sdlRenderer, 200, 200, 200, 200);
    SDL_FRect handle = {control.currentPosition.x - 10.0f, control.currentPosition.y - 10.0f, 20.0f, 20.0f};
    SDL_RenderFillRect(m_sdlRenderer, &handle);
}

void UI::handleSliderDrag(const glm::vec2& position) {
}

void UI::createMobileUI() {
}

void UI::updateMobileControls() {
}

void UI::renderMobileControls() {
}

bool UI::initializeFonts() {
    if (TTF_Init() == -1) {
        LOG_ERRORF("Failed to initialize SDL_ttf: %s", SDL_GetError());
        return false;
    }
    
    m_font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 48);
    if (!m_font) {
        LOG_ERRORF("Failed to load font: %s", SDL_GetError());
        return false;
    }
    
    LOG_INFO("Fonts initialized");
    return true;
}

void UI::cleanupFonts() {
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
    TTF_Quit();
}

void UI::updateAnimations(float deltaTime) {
    for (auto& button : m_buttons) {
        animateButton(button, deltaTime);
    }
    
    for (auto& slider : m_sliders) {
        animateSlider(slider, deltaTime);
    }
}

void UI::animateButton(Button& button, float deltaTime) {
}

void UI::animateSlider(Slider& slider, float deltaTime) {
}

} // namespace Game