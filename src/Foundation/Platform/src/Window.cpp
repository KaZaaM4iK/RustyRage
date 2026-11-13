#include "rr/platform/Window.hpp"

#include <SDL3/SDL.h>

namespace rr::platform {

Window::~Window() {
    destroy();
}

bool Window::create(const std::string& title, int width, int height, std::string& error) {
    destroy();

    if (width <= 0 || height <= 0) {
        error = "window dimensions must be positive";
        return false;
    }

    window_ = SDL_CreateWindow(title.c_str(),
                               width,
                               height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr) {
        error = SDL_GetError();
        return false;
    }

    if (!SDL_SetWindowMinimumSize(window_, 320, 240)) {
        error = SDL_GetError();
        destroy();
        return false;
    }

    error.clear();
    return true;
}

void Window::destroy() {
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

SDL_Window* Window::native() const noexcept {
    return window_;
}

WindowSize Window::pixel_size() const {
    WindowSize size;
    if (window_ != nullptr) {
        if (!SDL_GetWindowSizeInPixels(window_, &size.width, &size.height)) {
            size.width = 0;
            size.height = 0;
        }
    }
    return size;
}

WindowSize Window::window_size() const {
    WindowSize size;
    if (window_ != nullptr) {
        if (!SDL_GetWindowSize(window_, &size.width, &size.height)) {
            size.width = 0;
            size.height = 0;
        }
    }
    return size;
}

bool Window::set_size(int width, int height, std::string& error) {
    if (window_ == nullptr) {
        error = "window is not created";
        return false;
    }
    if (width <= 0 || height <= 0) {
        error = "window dimensions must be positive";
        return false;
    }
    if (!SDL_SetWindowSize(window_, width, height)) {
        error = SDL_GetError();
        return false;
    }
    error.clear();
    return true;
}

bool Window::sync(std::string& error) {
    if (window_ == nullptr) {
        error = "window is not created";
        return false;
    }
    if (!SDL_SyncWindow(window_)) {
        error = SDL_GetError();
        return false;
    }
    error.clear();
    return true;
}

std::string Window::video_driver() {
    const char* driver = SDL_GetCurrentVideoDriver();
    return driver != nullptr ? driver : "unknown";
}

} // namespace rr::platform
