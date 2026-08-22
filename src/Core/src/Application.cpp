#include "Core/Application.hpp"
#include "Core/Renderer.hpp"
#include "Core/InputManager.hpp"
#include "Core/Config.hpp"
#include "Core/Logging.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdexcept>
#include "Game/UI.hpp"

namespace Core {

Application::Application(int width, int height, const std::string& title)
    : m_window(nullptr)
    , m_renderer(nullptr)
    , m_inputManager(std::make_unique<InputManager>())
    , m_ui(nullptr)
    , m_running(false)
    , m_width(width)
    , m_height(height)
    , m_title(title)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        LOG_ERRORF("SDL_Init failed: %s", SDL_GetError());
        throw std::runtime_error("Failed to initialize SDL");
    }

    m_window = SDL_CreateWindow(title.c_str(), m_width, m_height, SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        LOG_ERRORF("SDL_CreateWindow failed: %s", SDL_GetError());
        throw std::runtime_error("Failed to create window");
    }

    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        LOG_ERRORF("SDL_CreateRenderer failed: %s", SDL_GetError());
        throw std::runtime_error("Failed to create renderer");
    }

    m_ui = std::make_unique<Game::UI>();
    if (!m_ui->initialize(m_renderer, m_window, m_inputManager.get())) {
        LOG_ERROR("Failed to initialize UI");
        throw std::runtime_error("Failed to initialize UI");
    }
}

Application::~Application() {
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Application::run() {
    m_running = true;
    Uint64 lastTime = SDL_GetTicks();
    while (m_running) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        m_inputManager->update();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                m_width = event.window.data1;
                m_height = event.window.data2;
            }
            m_inputManager->handleEvent(event);
        }

        update(deltaTime);

        // === РЕНДЕР ===
        SDL_SetRenderDrawColor(m_renderer, 30, 15, 10, 255);
        SDL_RenderClear(m_renderer);

        render();  // рендер 3D (пока пустой)

        if (m_ui) {
            m_ui->update(deltaTime);
            m_ui->render();
        }

        SDL_RenderPresent(m_renderer);
    }
    LOG_INFO("Application main loop ended");
}

bool Application::initializeSDL() {
    LOG_INFO("Initializing SDL3");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) < 0) {
        LOG_ERRORF("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    
    m_window = SDL_CreateWindow(
        m_title.c_str(),
        m_width, m_height,
        SDL_WINDOW_RESIZABLE
    );
    
    if (!m_window) {
        LOG_ERRORF("Failed to create window: %s", SDL_GetError());
        return false;
    }
    
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_window);
    SDL_RaiseWindow(m_window);

    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        LOG_ERRORF("Failed to create SDL_Renderer: %s", SDL_GetError());
        return false;
    }
    
    LOG_INFOF("Created and showed window: %dx%d", m_width, m_height);
    return true;
}

void Application::shutdownSDL() {
    LOG_INFO("Shutting down SDL3");

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    
    SDL_Quit();
}

} // namespace Core