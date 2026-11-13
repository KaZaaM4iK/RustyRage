#pragma once

#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string_view>

namespace rr::core {

enum class LogLevel : std::uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4,
};

[[nodiscard]] std::string_view to_string(LogLevel level) noexcept;

/// Thread-safe, instance-owned logger.
///
/// No global mutable logger singleton is used; the owning application passes
/// the logger explicitly to the components that need it.
class Logger {
public:
    explicit Logger(std::ostream& output = std::cerr);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void set_min_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel min_level() const noexcept;

    void write(LogLevel level, std::string_view message);

private:
    std::mutex mutex_;
    std::ostream* output_;
    std::atomic<LogLevel> min_level_{LogLevel::Info};
};

} // namespace rr::core
