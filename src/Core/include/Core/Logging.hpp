#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace Core {

/**
 * Logging system for debugging and monitoring
 * Thread-safe logging with different levels and file output
 */
class Logger {
public:
    enum class Level {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };

    static Logger& getInstance();
    
    // Configuration
    void setLogLevel(Level level);
    void setLogToFile(bool enable, const std::string& filename = "rustyrage.log");
    void setLogToConsole(bool enable);
    
    // Logging methods
    void log(Level level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);
    
    // Formatted logging
    void logf(Level level, const std::string& format, ...);
    
    void debugf(const std::string& format, ...);
    
    void infof(const std::string& format, ...);
    
    void warningf(const std::string& format, ...);
    
    void errorf(const std::string& format, ...);
    
    void criticalf(const std::string& format, ...);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    Level m_logLevel;
    bool m_logToFile;
    bool m_logToConsole;
    std::string m_logFilename;
    std::unique_ptr<std::ofstream> m_logFile;
    std::mutex m_logMutex;
    
    std::string levelToString(Level level) const;
    std::string getTimestamp() const;
    void writeLog(Level level, const std::string& message);
};

// Convenience macros
#define LOG_DEBUG(msg) Core::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Core::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Core::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Core::Logger::getInstance().error(msg)
#define LOG_CRITICAL(msg) Core::Logger::getInstance().critical(msg)

#define LOG_DEBUGF(fmt, ...) Core::Logger::getInstance().debugf(fmt, __VA_ARGS__)
#define LOG_INFOF(fmt, ...) Core::Logger::getInstance().infof(fmt, __VA_ARGS__)
#define LOG_WARNINGF(fmt, ...) Core::Logger::getInstance().warningf(fmt, __VA_ARGS__)
#define LOG_ERRORF(fmt, ...) Core::Logger::getInstance().errorf(fmt, __VA_ARGS__)
#define LOG_CRITICALF(fmt, ...) Core::Logger::getInstance().criticalf(fmt, __VA_ARGS__)

} // namespace Core
