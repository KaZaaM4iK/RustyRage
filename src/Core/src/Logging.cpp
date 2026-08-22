#include "Core/Logging.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <fstream>

namespace Core {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(Level level) {
    m_logLevel = level;
}

void Logger::setLogToFile(bool enable, const std::string& filename) {
    m_logToFile = enable;
    m_logFilename = filename;
    
    if (enable) {
        m_logFile = std::make_unique<std::ofstream>(filename, std::ios::app);
        if (!m_logFile->is_open()) {
            m_logToFile = false;
            m_logFile.reset();
        }
    } else {
        m_logFile.reset();
    }
}

void Logger::setLogToConsole(bool enable) {
    m_logToConsole = enable;
}

void Logger::log(Level level, const std::string& message) {
    if (level < m_logLevel) {
        return;
    }
    
    writeLog(level, message);
}

void Logger::debug(const std::string& message) {
    log(Level::Debug, message);
}

void Logger::info(const std::string& message) {
    log(Level::Info, message);
}

void Logger::warning(const std::string& message) {
    log(Level::Warning, message);
}

void Logger::error(const std::string& message) {
    log(Level::Error, message);
}

void Logger::critical(const std::string& message) {
    log(Level::Critical, message);
}

void Logger::logf(Level level, const std::string& format, ...) {
    if (level < m_logLevel) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    
    va_end(args);
    
    writeLog(level, std::string(buffer));
}

void Logger::debugf(const std::string& format, ...) {
    if (Level::Debug < m_logLevel) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    
    va_end(args);
    
    writeLog(Level::Debug, std::string(buffer));
}

void Logger::infof(const std::string& format, ...) {
    if (Level::Info < m_logLevel) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    
    va_end(args);
    
    writeLog(Level::Info, std::string(buffer));
}

void Logger::warningf(const std::string& format, ...) {
    if (Level::Warning < m_logLevel) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    
    va_end(args);
    
    writeLog(Level::Warning, std::string(buffer));
}

void Logger::errorf(const std::string& format, ...) {
    if (Level::Error < m_logLevel) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    
    va_end(args);
    
    writeLog(Level::Error, std::string(buffer));
}

void Logger::criticalf(const std::string& format, ...) {
    if (Level::Critical < m_logLevel) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    
    va_end(args);
    
    writeLog(Level::Critical, std::string(buffer));
}

void Logger::writeLog(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    
    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);
    std::string logMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    if (m_logToConsole) {
        std::cout << logMessage << std::endl;
    }
    
    if (m_logToFile && m_logFile && m_logFile->is_open()) {
        *m_logFile << logMessage << std::endl;
        m_logFile->flush();
    }
}

std::string Logger::levelToString(Level level) const {
    switch (level) {
        case Level::Debug:    return "DEBUG";
        case Level::Info:     return "INFO";
        case Level::Warning:  return "WARNING";
        case Level::Error:    return "ERROR";
        case Level::Critical: return "CRITICAL";
        default:              return "UNKNOWN";
    }
}

std::string Logger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

} // namespace Core
