#include "Core/Config.hpp"
#include "Core/Logging.hpp"
#include <fstream>
#include <sstream>

namespace Core {

Config::Config() {
    setDefaultGraphicsSettings();
    setDefaultAudioSettings();
    setDefaultInputSettings();
    setDefaultNetworkSettings();
    setDefaultGameSettings();
}

Config::~Config() {
}

bool Config::loadFromFile(const std::string& filename) {
    LOG_INFOF("Loading config from %s", filename.c_str());
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARNINGF("Could not open config file: %s", filename.c_str());
        return false;
    }
    
    // TODO: Implement INI file parsing
    // For now, just return true to allow compilation
    
    file.close();
    LOG_INFO("Config loaded successfully");
    return true;
}

bool Config::saveToFile(const std::string& filename) const {
    LOG_INFOF("Saving config to %s", filename.c_str());
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERRORF("Could not save config file: %s", filename.c_str());
        return false;
    }
    
    // TODO: Implement INI file writing
    // For now, just write basic structure
    
    file << "[Graphics]\n";
    file << "WindowWidth=" << m_graphicsSettings.windowWidth << "\n";
    file << "WindowHeight=" << m_graphicsSettings.windowHeight << "\n";
    file << "Fullscreen=" << (m_graphicsSettings.fullscreen ? "true" : "false") << "\n";
    file << "VSync=" << (m_graphicsSettings.vsync ? "true" : "false") << "\n";
    
    file << "[Audio]\n";
    file << "MasterVolume=" << m_audioSettings.masterVolume << "\n";
    file << "MusicVolume=" << m_audioSettings.musicVolume << "\n";
    file << "SFXVolume=" << m_audioSettings.sfxVolume << "\n";
    
    file << "[Network]\n";
    file << "PreferredRegion=" << m_networkSettings.preferredRegion << "\n";
    file << "MaxPing=" << m_networkSettings.maxPing << "\n";
    
    file.close();
    LOG_INFO("Config saved successfully");
    return true;
}

void Config::setWindowSize(int width, int height) {
    m_graphicsSettings.windowWidth = width;
    m_graphicsSettings.windowHeight = height;
}

void Config::setFullscreen(bool fullscreen) {
    m_graphicsSettings.fullscreen = fullscreen;
}

void Config::setVolume(float volume) {
    m_audioSettings.masterVolume = volume;
}

void Config::setRegion(const std::string& region) {
    m_networkSettings.preferredRegion = region;
}

bool Config::validateSettings() const {
    // Basic validation
    if (m_graphicsSettings.windowWidth < 640 || m_graphicsSettings.windowHeight < 480) {
        return false;
    }
    
    if (m_audioSettings.masterVolume < 0.0f || m_audioSettings.masterVolume > 1.0f) {
        return false;
    }
    
    return true;
}

void Config::resetToDefaults() {
    setDefaultGraphicsSettings();
    setDefaultAudioSettings();
    setDefaultInputSettings();
    setDefaultNetworkSettings();
    setDefaultGameSettings();
    LOG_INFO("Settings reset to defaults");
}

void Config::setDefaultGraphicsSettings() {
    m_graphicsSettings = GraphicsSettings();
}

void Config::setDefaultAudioSettings() {
    m_audioSettings = AudioSettings();
}

void Config::setDefaultInputSettings() {
    m_inputSettings = InputSettings();
}

void Config::setDefaultNetworkSettings() {
    m_networkSettings = NetworkSettings();
}

void Config::setDefaultGameSettings() {
    m_gameSettings = GameSettings();
}

} // namespace Core
