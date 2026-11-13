#include "rr/core/Log.hpp"

namespace rr::core {

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
    }
    return "UNKNOWN";
}

Logger::Logger(std::ostream& output)
    : output_(&output) {
}

void Logger::set_min_level(LogLevel level) noexcept {
    min_level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::min_level() const noexcept {
    return min_level_.load(std::memory_order_relaxed);
}

void Logger::write(LogLevel level, std::string_view message) {
    if (level < min_level_.load(std::memory_order_relaxed)) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    (*output_) << '[' << to_string(level) << "] " << message << '\n';
    output_->flush();
}

} // namespace rr::core
