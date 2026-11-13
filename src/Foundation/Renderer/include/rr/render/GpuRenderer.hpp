#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;

namespace rr::render {

struct GpuInfo {
    std::string backend{};
    std::string device{};
    std::string driver{};
    std::string driver_version{};
    std::string driver_info{};
};

/// Minimal SDL_GPU renderer used by the TASK_001 graphics smoke test.
///
/// The renderer owns the SDL_GPU device, pipeline and smoke-test resources.
/// It must be created, used and destroyed on the thread that owns the window.
class GpuRenderer {
public:
    GpuRenderer();
    ~GpuRenderer();

    GpuRenderer(const GpuRenderer&) = delete;
    GpuRenderer& operator=(const GpuRenderer&) = delete;
    GpuRenderer(GpuRenderer&&) = delete;
    GpuRenderer& operator=(GpuRenderer&&) = delete;

    bool initialize(SDL_Window* window, std::string& error);
    void shutdown();

    /// Renders one frame of the 3D test cube. Returns false on SDL_GPU error.
    bool draw_frame(SDL_Window* window, float angle_radians, std::string& error);

    void resize(int width, int height);

    [[nodiscard]] const GpuInfo& info() const noexcept;
    [[nodiscard]] std::uint64_t frames_rendered() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rr::render
