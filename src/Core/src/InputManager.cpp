#include "Core/InputManager.hpp"
#include <algorithm>
#include "Core/Logging.hpp"
#include <SDL3/SDL.h>

namespace Core {

InputManager::InputManager() : m_mobileMode(false), m_touchSensitivity(1.0f) {
    LOG_INFO("Creating input manager");
}

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::initialize() {
    LOG_INFO("Initializing input manager");
    
    // Initialize input mappings
    setInputMapping(InputAction::Accelerate, SDL_SCANCODE_W);
    setInputMapping(InputAction::Brake, SDL_SCANCODE_S);
    setInputMapping(InputAction::TurnLeft, SDL_SCANCODE_A);
    setInputMapping(InputAction::TurnRight, SDL_SCANCODE_D);
    setInputMapping(InputAction::Fire, SDL_SCANCODE_SPACE);
    setInputMapping(InputAction::UsePowerUp, SDL_SCANCODE_E);
    setInputMapping(InputAction::Pause, SDL_SCANCODE_ESCAPE);
    setInputMapping(InputAction::Menu, SDL_SCANCODE_TAB);
    
    LOG_INFO("Input manager initialized");
    return true;
}

void InputManager::shutdown() {
    LOG_INFO("Shutting down input manager");
}

void InputManager::update() {
    updateKeyboardState();
    updateMouseState();
    updateGamepadState();
    updateTouchState();
    updateVirtualJoysticks();
}

bool InputManager::isKeyPressed(SDL_Scancode key) const {
    auto it = m_keyStates.find(key);
    return (it != m_keyStates.end()) ? it->second : false;
}

bool InputManager::isKeyJustPressed(SDL_Scancode key) const {
    auto it = m_keyStates.find(key);
    auto prevIt = m_previousKeyStates.find(key);
    
    bool current = (it != m_keyStates.end()) ? it->second : false;
    bool previous = (prevIt != m_previousKeyStates.end()) ? prevIt->second : false;
    
    return current && !previous;
}

bool InputManager::isKeyJustReleased(SDL_Scancode key) const {
    auto it = m_keyStates.find(key);
    auto prevIt = m_previousKeyStates.find(key);
    
    bool current = (it != m_keyStates.end()) ? it->second : false;
    bool previous = (prevIt != m_previousKeyStates.end()) ? prevIt->second : false;
    
    return !current && previous;
}

glm::vec2 InputManager::getMousePosition() const {
    return m_mousePosition;
}

glm::vec2 InputManager::getMouseDelta() const {
    return m_mousePosition - m_previousMousePosition;
}

bool InputManager::isMouseButtonPressed(int button) const {
    auto it = m_mouseButtonStates.find(button);
    return (it != m_mouseButtonStates.end()) ? it->second : false;
}

bool InputManager::isMouseButtonJustPressed(int button) const {
    auto it = m_mouseButtonStates.find(button);
    auto prevIt = m_previousMouseButtonStates.find(button);
    
    bool current = (it != m_mouseButtonStates.end()) ? it->second : false;
    bool previous = (prevIt != m_previousMouseButtonStates.end()) ? prevIt->second : false;
    
    bool result = current && !previous;
    LOG_INFOF("isMouseButtonJustPressed: button=%d, current=%d, previous=%d, result=%d", 
              button, current, previous, result);
    return result;
}

bool InputManager::isMouseButtonJustReleased(int button) const {
    auto it = m_mouseButtonStates.find(button);
    auto prevIt = m_previousMouseButtonStates.find(button);
    
    bool current = (it != m_mouseButtonStates.end()) ? it->second : false;
    bool previous = (prevIt != m_previousMouseButtonStates.end()) ? prevIt->second : false;
    
    return !current && previous;
}

float InputManager::getMouseWheelDelta() const {
    return m_mouseWheelDelta;
}

bool InputManager::isGamepadConnected(int gamepadIndex) const {
    return SDL_IsGamepad(gamepadIndex);
}

float InputManager::getGamepadAxis(int gamepadIndex, SDL_GamepadAxis axis) const {
    if (!isGamepadConnected(gamepadIndex)) return 0.0f;
    
    SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepadIndex);
    if (!gamepad) return 0.0f;
    
    float value = SDL_GetGamepadAxis(gamepad, axis) / 32767.0f;
    SDL_CloseGamepad(gamepad);
    
    return value;
}

bool InputManager::isGamepadButtonPressed(int gamepadIndex, SDL_GamepadButton button) const {
    if (!isGamepadConnected(gamepadIndex)) return false;
    
    auto it = m_gamepadButtonStates.find(gamepadIndex);
    if (it == m_gamepadButtonStates.end()) return false;
    
    auto buttonIt = it->second.find(button);
    return (buttonIt != it->second.end()) ? buttonIt->second : false;
}

bool InputManager::isGamepadButtonJustPressed(int gamepadIndex, SDL_GamepadButton button) const {
    auto it = m_gamepadButtonStates.find(gamepadIndex);
    auto prevIt = m_previousGamepadButtonStates.find(gamepadIndex);
    
    if (it == m_gamepadButtonStates.end() || prevIt == m_previousGamepadButtonStates.end()) {
        return false;
    }
    
    auto buttonIt = it->second.find(button);
    auto prevButtonIt = prevIt->second.find(button);
    
    bool current = (buttonIt != it->second.end()) ? buttonIt->second : false;
    bool previous = (prevButtonIt != prevIt->second.end()) ? prevButtonIt->second : false;
    
    return current && !previous;
}

bool InputManager::isGamepadButtonJustReleased(int gamepadIndex, SDL_GamepadButton button) const {
    auto it = m_gamepadButtonStates.find(gamepadIndex);
    auto prevIt = m_previousGamepadButtonStates.find(gamepadIndex);
    
    if (it == m_gamepadButtonStates.end() || prevIt == m_previousGamepadButtonStates.end()) {
        return false;
    }
    
    auto buttonIt = it->second.find(button);
    auto prevButtonIt = prevIt->second.find(button);
    
    bool current = (buttonIt != it->second.end()) ? buttonIt->second : false;
    bool previous = (prevButtonIt != prevIt->second.end()) ? prevButtonIt->second : false;
    
    return !current && previous;
}

void InputManager::setVirtualJoystick(int joystickId, const glm::vec2& center, float radius) {
    VirtualJoystick joystick;
    joystick.center = center;
    joystick.radius = radius;
    joystick.isActive = false;
    joystick.touchId = -1;
    
    m_virtualJoysticks[joystickId] = joystick;
}

glm::vec2 InputManager::getVirtualJoystickValue(int joystickId) const {
    auto it = m_virtualJoysticks.find(joystickId);
    if (it == m_virtualJoysticks.end() || !it->second.isActive) {
        return glm::vec2(0.0f);
    }
    
    glm::vec2 delta = it->second.currentPosition - it->second.center;
    float distance = glm::length(delta);
    
    if (distance > it->second.radius) {
        delta = glm::normalize(delta) * it->second.radius;
    }
    
    return delta / it->second.radius;
}

bool InputManager::isVirtualJoystickActive(int joystickId) const {
    auto it = m_virtualJoysticks.find(joystickId);
    return (it != m_virtualJoysticks.end()) ? it->second.isActive : false;
}

void InputManager::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            m_mousePosition = glm::vec2(event.motion.x, event.motion.y);
            break;
            
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            m_mouseButtonStates[event.button.button] = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            LOG_INFOF("Mouse button event: button=%d, state=%s", 
                      event.button.button, 
                      event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "down" : "up");
            LOG_INFOF("Button %d state set to %d", event.button.button, m_mouseButtonStates[event.button.button]);
            break;
            
        case SDL_EVENT_MOUSE_WHEEL:
            m_mouseWheelDelta = event.wheel.y;
            break;
            
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            m_gamepadButtonStates[event.gbutton.which][static_cast<SDL_GamepadButton>(event.gbutton.button)] = 
                (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            break;
            
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
            handleTouchEvent(event);
            break;
    }
}

void InputManager::setInputMapping(InputAction action, SDL_Scancode key) {
    m_keyMappings[action] = key;
}

void InputManager::setInputMapping(InputAction action, SDL_GamepadButton button) {
    m_gamepadMappings[action] = button;
}

bool InputManager::isActionPressed(InputAction action) const {
    auto keyIt = m_keyMappings.find(action);
    if (keyIt != m_keyMappings.end() && isKeyPressed(keyIt->second)) {
        return true;
    }
    
    auto buttonIt = m_gamepadMappings.find(action);
    if (buttonIt != m_gamepadMappings.end()) {
        for (int i = 0; i < 4; ++i) {
            if (isGamepadButtonPressed(i, buttonIt->second)) {
                return true;
            }
        }
    }
    
    return false;
}

bool InputManager::isActionJustPressed(InputAction action) const {
    auto keyIt = m_keyMappings.find(action);
    if (keyIt != m_keyMappings.end() && isKeyJustPressed(keyIt->second)) {
        return true;
    }
    
    auto buttonIt = m_gamepadMappings.find(action);
    if (buttonIt != m_gamepadMappings.end()) {
        for (int i = 0; i < 4; ++i) {
            if (isGamepadButtonJustPressed(i, buttonIt->second)) {
                return true;
            }
        }
    }
    
    return false;
}

bool InputManager::isActionJustReleased(InputAction action) const {
    auto keyIt = m_keyMappings.find(action);
    if (keyIt != m_keyMappings.end() && isKeyJustReleased(keyIt->second)) {
        return true;
    }
    
    auto buttonIt = m_gamepadMappings.find(action);
    if (buttonIt != m_gamepadMappings.end()) {
        for (int i = 0; i < 4; ++i) {
            if (isGamepadButtonJustReleased(i, buttonIt->second)) {
                return true;
            }
        }
    }
    
    return false;
}

void InputManager::updateKeyboardState() {
    m_previousKeyStates = m_keyStates;
}

void InputManager::updateMouseState() {
    m_previousMouseButtonStates = m_mouseButtonStates;
    m_previousMousePosition = m_mousePosition;
    m_mouseWheelDelta = 0.0f;
}

void InputManager::updateGamepadState() {
    m_previousGamepadButtonStates = m_gamepadButtonStates;
}

void InputManager::updateTouchState() {
    // Clear inactive touch points
    m_touchPoints.erase(
        std::remove_if(m_touchPoints.begin(), m_touchPoints.end(),
            [](const TouchPoint& point) { return !point.isActive; }),
        m_touchPoints.end());
}

void InputManager::handleTouchEvent(const SDL_Event& event) {
    TouchPoint point;
    point.id = event.tfinger.fingerID;
    
    // Convert normalized touch coordinates to screen coordinates
    int windowWidth, windowHeight;
    SDL_GetWindowSize(SDL_GetWindowFromID(event.tfinger.windowID), &windowWidth, &windowHeight);
    point.position = glm::vec2(event.tfinger.x * windowWidth, event.tfinger.y * windowHeight);
    
    switch (event.type) {
        case SDL_EVENT_FINGER_DOWN:
            point.isActive = true;
            m_touchPoints.push_back(point);
            break;
            
        case SDL_EVENT_FINGER_MOTION:
            for (auto& p : m_touchPoints) {
                if (p.id == point.id) {
                    p.delta = point.position - p.position;
                    p.position = point.position;
                    break;
                }
            }
            break;
            
        case SDL_EVENT_FINGER_UP:
            for (auto& p : m_touchPoints) {
                if (p.id == point.id) {
                    p.isActive = false;
                    break;
                }
            }
            break;
    }
    
    // Update virtual joysticks
    for (auto& joystick : m_virtualJoysticks) {
        if (event.type == SDL_EVENT_FINGER_DOWN) {
            float distance = glm::length(point.position - joystick.second.center);
            if (distance < joystick.second.radius && !joystick.second.isActive) {
                joystick.second.isActive = true;
                joystick.second.touchId = point.id;
                joystick.second.currentPosition = point.position;
            }
        } else if (event.type == SDL_EVENT_FINGER_MOTION && joystick.second.isActive && joystick.second.touchId == point.id) {
            joystick.second.currentPosition = point.position;
        } else if (event.type == SDL_EVENT_FINGER_UP && joystick.second.isActive && joystick.second.touchId == point.id) {
            joystick.second.isActive = false;
            joystick.second.touchId = -1;
            joystick.second.currentPosition = joystick.second.center;
        }
    }
}

void InputManager::updateVirtualJoysticks() {
    // Handled in handleTouchEvent
}

} // namespace Core