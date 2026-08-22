#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <functional>
#include <vector>

namespace Core {

/**
 * Input manager for handling keyboard, mouse, gamepad, and touch input
 * Supports cross-platform input including mobile touch controls
 */
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Initialization
    bool initialize();
    void shutdown();
    
    // Update input state
    void update();
    
    // Keyboard input
    bool isKeyPressed(SDL_Scancode key) const;
    bool isKeyJustPressed(SDL_Scancode key) const;
    bool isKeyJustReleased(SDL_Scancode key) const;
    
    // Mouse input
    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseDelta() const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonJustPressed(int button) const;
    bool isMouseButtonJustReleased(int button) const;
    float getMouseWheelDelta() const;
    
    // Gamepad input
    bool isGamepadConnected(int gamepadIndex) const;
    float getGamepadAxis(int gamepadIndex, SDL_GamepadAxis axis) const;
    bool isGamepadButtonPressed(int gamepadIndex, SDL_GamepadButton button) const;
    bool isGamepadButtonJustPressed(int gamepadIndex, SDL_GamepadButton button) const;
    bool isGamepadButtonJustReleased(int gamepadIndex, SDL_GamepadButton button) const;
    
    // Touch input (mobile)
    struct TouchPoint {
        int id;
        glm::vec2 position;
        glm::vec2 delta;
        bool isActive;
    };
    
    const std::vector<TouchPoint>& getTouchPoints() const { return m_touchPoints; }
    bool isTouchActive() const { return !m_touchPoints.empty(); }
    
    // Virtual joysticks for mobile
    struct VirtualJoystick {
        glm::vec2 center;
        glm::vec2 currentPosition;
        float radius;
        bool isActive;
        int touchId;
    };
    
    void setVirtualJoystick(int joystickId, const glm::vec2& center, float radius);
    glm::vec2 getVirtualJoystickValue(int joystickId) const;
    bool isVirtualJoystickActive(int joystickId) const;
    
    // Event handling
    void handleEvent(const SDL_Event& event);
    
    // Input mapping
    enum class InputAction {
        Accelerate,
        Brake,
        TurnLeft,
        TurnRight,
        Fire,
        UsePowerUp,
        Pause,
        Menu
    };
    
    void setInputMapping(InputAction action, SDL_Scancode key);
    void setInputMapping(InputAction action, SDL_GamepadButton button);
    bool isActionPressed(InputAction action) const;
    bool isActionJustPressed(InputAction action) const;
    bool isActionJustReleased(InputAction action) const;
    
    // Mobile-specific
    void enableMobileMode(bool enable) { m_mobileMode = enable; }
    bool isMobileMode() const { return m_mobileMode; }

private:
    // Input state
    std::unordered_map<SDL_Scancode, bool> m_keyStates;
    std::unordered_map<SDL_Scancode, bool> m_previousKeyStates;
    
    std::unordered_map<int, bool> m_mouseButtonStates;
    std::unordered_map<int, bool> m_previousMouseButtonStates;
    glm::vec2 m_mousePosition;
    glm::vec2 m_previousMousePosition;
    float m_mouseWheelDelta;
    
    std::unordered_map<int, std::unordered_map<SDL_GamepadButton, bool>> m_gamepadButtonStates;
    std::unordered_map<int, std::unordered_map<SDL_GamepadButton, bool>> m_previousGamepadButtonStates;
    
    // Touch input
    std::vector<TouchPoint> m_touchPoints;
    std::unordered_map<int, VirtualJoystick> m_virtualJoysticks;
    
    // Input mapping
    std::unordered_map<InputAction, SDL_Scancode> m_keyMappings;
    std::unordered_map<InputAction, SDL_GamepadButton> m_gamepadMappings;
    
    // Settings
    bool m_mobileMode;
    float m_touchSensitivity;
    
    // Helper methods
    void updateKeyboardState();
    void updateMouseState();
    void updateGamepadState();
    void updateTouchState();
    void handleTouchEvent(const SDL_Event& event);
    void updateVirtualJoysticks();
};

} // namespace Core
