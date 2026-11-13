#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <vector>
#include <sstream>
#include <mutex>

namespace Core {

/**
 * Modern C++20 Localization & Internationalization engine.
 * Thread-safe, supports UTF-8, nested key lookups, and parameter formatting.
 */
class LocalizationManager {
public:
    enum class Language {
        Russian,
        English,
        German,
        Spanish
    };

    static LocalizationManager& getInstance() {
        static LocalizationManager instance;
        return instance;
    }

    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    void setLanguage(Language lang);
    void setLanguageByCode(std::string_view code);
    Language getCurrentLanguage() const { return m_currentLanguage; }
    std::string getCurrentLanguageCode() const;

    // Load localization dictionary from JSON string or file
    bool loadFromJson(std::string_view jsonContent, Language lang);
    bool loadFromFile(const std::string& filepath, Language lang);

    // Get localized text by key (e.g. "menu.mode1_title")
    std::string get(std::string_view key) const;
    std::string get(std::string_view key, const std::string& fallback) const;

    // Format localized text with string replacements ({0}, {1}, etc.)
    template<typename... Args>
    std::string format(std::string_view key, Args&&... args) const {
        std::string raw = get(key);
        std::vector<std::string> strArgs = { toString(std::forward<Args>(args))... };
        for (size_t i = 0; i < strArgs.size(); ++i) {
            std::string placeholder = "{" + std::to_string(i) + "}";
            size_t pos = 0;
            while ((pos = raw.find(placeholder, pos)) != std::string::npos) {
                raw.replace(pos, placeholder.length(), strArgs[i]);
                pos += strArgs[i].length();
            }
        }
        return raw;
    }

private:
    LocalizationManager();
    ~LocalizationManager() = default;

    void initDefaultDictionaries();

    template<typename T>
    static std::string toString(T&& val) {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            return val;
        } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>) {
            return std::string(val);
        } else {
            std::ostringstream ss;
            ss << val;
            return ss.str();
        }
    }

    mutable std::mutex m_mutex;
    Language m_currentLanguage = Language::Russian;
    std::unordered_map<Language, std::unordered_map<std::string, std::string>> m_dictionaries;
};

// Global shorthand helper
inline std::string Loc(std::string_view key) {
    return LocalizationManager::getInstance().get(key);
}

template<typename... Args>
inline std::string LocFmt(std::string_view key, Args&&... args) {
    return LocalizationManager::getInstance().format(key, std::forward<Args>(args)...);
}

} // namespace Core
