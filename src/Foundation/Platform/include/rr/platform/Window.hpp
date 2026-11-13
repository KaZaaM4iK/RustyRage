#pragma once

#include <string>

struct SDL_Window;

namespace rr::platform {

struct WindowSize {
    int width{0};
    int height{0};
};

/// SDL3 window ownership wrapper.
///
/// The wrapper is intentionally small: it owns the native SDL window and
/// exposes only window operations needed by the platform/renderer layers. It
/// does not own game rules or simulation state.
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool create(const std::string& title, int width, int height, std::string& error);
    void destroy();

    [[nodiscard]] SDL_Window* native() const noexcept;
    [[nodiscard]] WindowSize pixel_size() const;
    [[nodiscard]] WindowSize window_size() const;

    bool set_size(int width, int height, std::string& error);
    bool sync(std::string& error);

    [[nodiscard]] static std::string video_driver();

private:
    SDL_Window* window_{nullptr};
};

} // namespace rr::platform
