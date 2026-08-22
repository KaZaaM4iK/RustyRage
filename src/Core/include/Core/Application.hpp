#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include "Game/UI.hpp"
#include "Core/Logging.hpp"

namespace Core {

class Renderer; // Vulkan renderer
class InputManager;
class Config;

/**
 * Base application class for Rusty Rage
 * Handles SDL initialization, main loop, and core systems
 */
class Application {
public:
    Application(int width, int height, const std::string& title);
    virtual ~Application();

    void run();
    
    virtual void initialize() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void shutdown() = 0;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    const std::string& getTitle() const { return m_title; }
    bool isRunning() const { return m_running; }
    
    Renderer* getRenderer() const { return nullptr; }
    InputManager* getInputManager() const { return m_inputManager.get(); }
    Config* getConfig() const { return m_config.get(); }
    SDL_Renderer* getSdlRenderer() const { return m_renderer; }
    SDL_Window* getSdlWindow() const { return m_window; }
    Game::UI* getUI() const { return m_ui.get(); }

protected:
    void setRunning(bool running) {
        LOG_INFOF("setRunning called: %d", running);
        m_running = running;
    }

    // Перемещаем m_ui в protected
    std::unique_ptr<Game::UI> m_ui;

private:
    int m_width;
    int m_height;
    std::string m_title;
    bool m_running;
    
    std::unique_ptr<InputManager> m_inputManager;
    std::unique_ptr<Config> m_config;
    
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    
    bool initializeSDL();
    void shutdownSDL();
};

} // namespace Core