#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Core {

/**
 * Configuration manager for game settings
 * Handles graphics, audio, input, and network settings
 */
class Config {
public:
    Config();
    ~Config();

    // Initialization
    bool loadFromFile(const std::string& filename = "config.ini");
    bool saveToFile(const std::string& filename = "config.ini") const;
    
    // Graphics settings
    struct GraphicsSettings {
        int windowWidth = 1280;
        int windowHeight = 720;
        bool fullscreen = false;
        bool vsync = true;
        int msaa = 4;
        float renderDistance = 1000.0f;
        int textureQuality = 2; // 0=low, 1=medium, 2=high
        int shadowQuality = 2;
        bool bloom = true;
        bool ssao = true;
    };
    
    // Audio settings
    struct AudioSettings {
        float masterVolume = 1.0f;
        float musicVolume = 0.8f;
        float sfxVolume = 1.0f;
        float voiceVolume = 1.0f;
        bool mute = false;
    };
    
    // Input settings
    struct InputSettings {
        float mouseSensitivity = 1.0f;
        float gamepadSensitivity = 1.0f;
        bool invertMouseY = false;
        bool showTouchControls = true; // Mobile only
        float touchSensitivity = 1.0f;
    };
    
    // Network settings
    struct NetworkSettings {
        std::string preferredRegion = "RU"; // RU, CN
        int maxPing = 150;
        bool autoRegion = true;
        std::string masterServerUrl = "https://master.rustyrage.com";
    };
    
    // Game settings
    struct GameSettings {
        std::string language = "ru";
        bool showFPS = false;
        bool showPing = true;
        bool autoReconnect = true;
        int maxReconnectAttempts = 3;
    };
    
    // Getters
    const GraphicsSettings& getGraphicsSettings() const { return m_graphicsSettings; }
    const AudioSettings& getAudioSettings() const { return m_audioSettings; }
    const InputSettings& getInputSettings() const { return m_inputSettings; }
    const NetworkSettings& getNetworkSettings() const { return m_networkSettings; }
    const GameSettings& getGameSettings() const { return m_gameSettings; }
    
    // Setters
    void setGraphicsSettings(const GraphicsSettings& settings) { m_graphicsSettings = settings; }
    void setAudioSettings(const AudioSettings& settings) { m_audioSettings = settings; }
    void setInputSettings(const InputSettings& settings) { m_inputSettings = settings; }
    void setNetworkSettings(const NetworkSettings& settings) { m_networkSettings = settings; }
    void setGameSettings(const GameSettings& settings) { m_gameSettings = settings; }
    
    // Individual setting getters/setters
    void setWindowSize(int width, int height);
    void setFullscreen(bool fullscreen);
    void setVolume(float volume);
    void setRegion(const std::string& region);
    
    // Validation
    bool validateSettings() const;
    void resetToDefaults();

private:
    GraphicsSettings m_graphicsSettings;
    AudioSettings m_audioSettings;
    InputSettings m_inputSettings;
    NetworkSettings m_networkSettings;
    GameSettings m_gameSettings;
    
    // Helper methods
    std::string getConfigValue(const std::string& section, const std::string& key) const;
    void setConfigValue(const std::string& section, const std::string& key, const std::string& value);
    void parseGraphicsSettings(const std::string& value);
    void parseAudioSettings(const std::string& value);
    void parseInputSettings(const std::string& value);
    void parseNetworkSettings(const std::string& value);
    void parseGameSettings(const std::string& value);
    
    // Default values
    void setDefaultGraphicsSettings();
    void setDefaultAudioSettings();
    void setDefaultInputSettings();
    void setDefaultNetworkSettings();
    void setDefaultGameSettings();
};

} // namespace Core
