#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "rr/platform/Window.hpp"
#include "rr/render/GpuRenderer.hpp"

#include <atomic>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#ifndef RUSTYRAGE_VERSION
#define RUSTYRAGE_VERSION "0.1.0"
#endif

namespace {

struct AppState {
    rr::platform::Window window;
    rr::render::GpuRenderer renderer;

    std::atomic<bool> quit_requested{false};
    std::atomic<bool> resize_requested{false};

    SDL_WindowID window_id{0};
    int initial_width{960};
    int initial_height{640};
    int smoke_frames{0};
    int resize_after{-1};
    int resize_width{800};
    int resize_height{600};

    std::uint64_t frame{0};
    float angle{0.0f};
    int last_width{0};
    int last_height{0};
    bool resize_done{false};
    bool resize_handled{false};
    bool initialized{false};
};

bool parse_int(std::string_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

void print_usage() {
    std::cout
        << "RustyRage client (foundation smoke test)\n"
        << "Usage: rusty_client [options]\n"
        << "  --width N          initial window width (default 960)\n"
        << "  --height N         initial window height (default 640)\n"
        << "  --smoke-frames N   bounded test mode: exit successfully after N frames\n"
        << "  --resize-after N   request a window resize before frame N (test mode)\n"
        << "  --resize-width N   target width for the resize exercise (default 800)\n"
        << "  --resize-height N  target height for the resize exercise (default 600)\n"
        << "  --help             print this help\n";
}

bool parse_args(AppState& state, int argc, char** argv, bool& show_help, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto require_value = [&](const char* option, int& target) {
            if (index + 1 >= argc) {
                error = std::string(option) + " requires a value";
                return false;
            }
            if (!parse_int(argv[++index], target) || target <= 0) {
                error = std::string(option) + " value is invalid";
                return false;
            }
            return true;
        };

        if (argument == "--help" || argument == "-h") {
            show_help = true;
        } else if (argument == "--width") {
            if (!require_value("--width", state.initial_width)) {
                return false;
            }
        } else if (argument == "--height") {
            if (!require_value("--height", state.initial_height)) {
                return false;
            }
        } else if (argument == "--smoke-frames") {
            if (!require_value("--smoke-frames", state.smoke_frames)) {
                return false;
            }
        } else if (argument == "--resize-after") {
            if (!require_value("--resize-after", state.resize_after)) {
                return false;
            }
        } else if (argument == "--resize-width") {
            if (!require_value("--resize-width", state.resize_width)) {
                return false;
            }
        } else if (argument == "--resize-height") {
            if (!require_value("--resize-height", state.resize_height)) {
                return false;
            }
        } else {
            error = "unknown argument: ";
            error += argument;
            return false;
        }
    }
    return true;
}

void log_gpu_info(const AppState& state) {
    const auto& info = state.renderer.info();
    std::cout << "[INFO] RustyRage client " RUSTYRAGE_VERSION << '\n';
    std::cout << "[INFO] SDL video driver: " << rr::platform::Window::video_driver() << '\n';
    std::cout << "[INFO] SDL_GPU backend: " << info.backend << '\n';
    std::cout << "[INFO] SDL_GPU device: " << info.device << '\n';
    std::cout << "[INFO] SDL_GPU driver: " << info.driver << " " << info.driver_version << '\n';
    std::cout << "[INFO] SDL_GPU driver info: " << info.driver_info << '\n';
    std::cout << "[INFO] window pixel size: " << state.last_width << "x" << state.last_height
              << '\n';
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    auto* state = new AppState();
    *appstate = state;

    bool show_help = false;
    std::string error;
    if (!parse_args(*state, argc, argv, show_help, error)) {
        std::cerr << "[ERROR] " << error << '\n';
        print_usage();
        return SDL_APP_FAILURE;
    }

    if (show_help) {
        print_usage();
        return SDL_APP_SUCCESS;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[ERROR] SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError() << '\n';
        return SDL_APP_FAILURE;
    }

    if (!state->window.create("RustyRage — Foundation Smoke Test",
                              state->initial_width,
                              state->initial_height,
                              error)) {
        std::cerr << "[ERROR] window creation failed: " << error << '\n';
        return SDL_APP_FAILURE;
    }
    state->window_id = SDL_GetWindowID(state->window.native());

    if (!state->renderer.initialize(state->window.native(), error)) {
        std::cerr << "[ERROR] SDL_GPU initialization failed: " << error << '\n';
        return SDL_APP_FAILURE;
    }

    const auto size = state->window.pixel_size();
    state->last_width = size.width;
    state->last_height = size.height;
    state->initialized = true;

    log_gpu_info(*state);
    std::cout << "[INFO] graphics smoke mode: "
              << (state->smoke_frames > 0 ? "bounded" : "interactive")
              << ", smoke_frames=" << state->smoke_frames
              << ", resize_after=" << state->resize_after << '\n';

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);
    if (state == nullptr || event == nullptr) {
        return SDL_APP_CONTINUE;
    }

    switch (event->type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_TERMINATING:
            state->quit_requested.store(true, std::memory_order_relaxed);
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event->window.windowID == state->window_id) {
                state->quit_requested.store(true, std::memory_order_relaxed);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (!event->key.repeat && event->key.scancode == SDL_SCANCODE_ESCAPE) {
                state->quit_requested.store(true, std::memory_order_relaxed);
            }
            break;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (event->window.windowID == state->window_id) {
                state->resize_requested.store(true, std::memory_order_relaxed);
            }
            break;

        default:
            break;
    }

    // Deliberately no GPU/window mutation here: SDL_AppEvent may run on a
    // different thread and concurrently with SDL_AppIterate. The flags are
    // consumed by the owning main thread.
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* state = static_cast<AppState*>(appstate);
    if (state == nullptr) {
        return SDL_APP_FAILURE;
    }

    if (state->quit_requested.load(std::memory_order_relaxed)) {
        std::cout << "[INFO] quit requested; frames=" << state->frame
                  << ", resize_handled=" << (state->resize_handled ? "true" : "false") << '\n';
        return SDL_APP_SUCCESS;
    }

    if (!state->resize_done && state->resize_after >= 0 &&
        state->frame >= static_cast<std::uint64_t>(state->resize_after)) {
        const auto before = state->window.pixel_size();
        std::string error;
        if (!state->window.set_size(state->resize_width, state->resize_height, error)) {
            std::cerr << "[ERROR] resize failed: " << error << '\n';
            return SDL_APP_FAILURE;
        }
        if (!state->window.sync(error)) {
            std::cerr << "[ERROR] window sync after resize failed: " << error << '\n';
            return SDL_APP_FAILURE;
        }
        const auto after = state->window.pixel_size();
        state->resize_done = true;
        state->resize_handled = before.width != after.width || before.height != after.height;
        std::cout << "[INFO] resize exercise: " << before.width << "x" << before.height
                  << " -> " << after.width << "x" << after.height << '\n';
    }

    if (state->resize_requested.exchange(false, std::memory_order_relaxed)) {
        std::cout << "[INFO] window resize event observed\n";
    }

    std::string error;
    if (!state->renderer.draw_frame(state->window.native(), state->angle, error)) {
        std::cerr << "[ERROR] frame rendering failed: " << error << '\n';
        return SDL_APP_FAILURE;
    }

    const auto size = state->window.pixel_size();
    if (size.width != state->last_width || size.height != state->last_height) {
        std::cout << "[INFO] render size changed: " << state->last_width << "x"
                  << state->last_height << " -> " << size.width << "x" << size.height << '\n';
        state->last_width = size.width;
        state->last_height = size.height;
        state->resize_handled = true;
    }

    ++state->frame;
    state->angle += 0.035f;

    if (state->smoke_frames > 0 &&
        state->frame >= static_cast<std::uint64_t>(state->smoke_frames)) {
        std::cout << "graphics_smoke: PASS frames=" << state->frame
                  << " resize_handled=" << (state->resize_handled ? "true" : "false")
                  << " backend=" << state->renderer.info().backend
                  << " device=" << state->renderer.info().device << '\n';
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
    auto* state = static_cast<AppState*>(appstate);
    if (state == nullptr) {
        return;
    }

    state->renderer.shutdown();
    state->window.destroy();
    delete state;
    SDL_Quit();
}
