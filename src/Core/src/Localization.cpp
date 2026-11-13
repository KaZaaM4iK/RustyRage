#include "Core/Localization.hpp"
#include "Core/Logging.hpp"
#include <fstream>
#include <sstream>

namespace Core {

LocalizationManager::LocalizationManager() {
    initDefaultDictionaries();
}

void LocalizationManager::setLanguage(Language lang) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentLanguage = lang;
    LOG_INFOF("Language changed to: %s", getCurrentLanguageCode().c_str());
}

void LocalizationManager::setLanguageByCode(std::string_view code) {
    if (code == "ru" || code == "RU") {
        setLanguage(Language::Russian);
    } else if (code == "en" || code == "EN") {
        setLanguage(Language::English);
    } else if (code == "de" || code == "DE") {
        setLanguage(Language::German);
    } else if (code == "es" || code == "ES") {
        setLanguage(Language::Spanish);
    } else {
        setLanguage(Language::Russian);
    }
}

std::string LocalizationManager::getCurrentLanguageCode() const {
    switch (m_currentLanguage) {
        case Language::Russian: return "ru";
        case Language::English: return "en";
        case Language::German: return "de";
        case Language::Spanish: return "es";
        default: return "ru";
    }
}

std::string LocalizationManager::get(std::string_view key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string keyStr(key);

    auto dictIt = m_dictionaries.find(m_currentLanguage);
    if (dictIt != m_dictionaries.end()) {
        auto valIt = dictIt->second.find(keyStr);
        if (valIt != dictIt->second.end()) {
            return valIt->second;
        }
    }

    // Fallback to Russian then English
    if (m_currentLanguage != Language::Russian) {
        auto ruIt = m_dictionaries.find(Language::Russian);
        if (ruIt != m_dictionaries.end()) {
            auto valIt = ruIt->second.find(keyStr);
            if (valIt != ruIt->second.end()) {
                return valIt->second;
            }
        }
    }

    return std::string(key);
}

std::string LocalizationManager::get(std::string_view key, const std::string& fallback) const {
    std::string res = get(key);
    return (res == key) ? fallback : res;
}

bool LocalizationManager::loadFromFile(const std::string& filepath, Language lang) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARNINGF("Could not open localization file: %s", filepath.c_str());
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromJson(buffer.str(), lang);
}

bool LocalizationManager::loadFromJson(std::string_view jsonContent, Language lang) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Simple flat key-value JSON string parser without heavy external dependencies
    auto& dict = m_dictionaries[lang];
    
    size_t pos = 0;
    while (pos < jsonContent.size()) {
        size_t keyStart = jsonContent.find('"', pos);
        if (keyStart == std::string_view::npos) break;
        size_t keyEnd = jsonContent.find('"', keyStart + 1);
        if (keyEnd == std::string_view::npos) break;
        
        std::string key(jsonContent.substr(keyStart + 1, keyEnd - keyStart - 1));
        
        size_t colon = jsonContent.find(':', keyEnd + 1);
        if (colon == std::string_view::npos) break;
        
        size_t valStart = jsonContent.find('"', colon + 1);
        if (valStart == std::string_view::npos) {
            pos = colon + 1;
            continue;
        }
        size_t valEnd = jsonContent.find('"', valStart + 1);
        if (valEnd == std::string_view::npos) break;
        
        std::string val(jsonContent.substr(valStart + 1, valEnd - valStart - 1));
        dict[key] = val;
        
        pos = valEnd + 1;
    }
    
    LOG_INFOF("Loaded %zu localization keys for language [%s]", dict.size(), getCurrentLanguageCode().c_str());
    return true;
}

void LocalizationManager::initDefaultDictionaries() {
    // 1. Russian Localization
    auto& ru = m_dictionaries[Language::Russian];
    ru["app.title"] = "Rusty Rage";
    ru["app.subtitle"] = "3D МНОГОПОЛЬЗОВАТЕЛЬСКИЙ БОЕВИК";
    ru["menu.play"] = "ИГРАТЬ";
    ru["menu.garage"] = "ГАРАЖ И ТЮНИНГ БОЕВЫХ МАШИН";
    ru["menu.editor"] = "3D РЕДАКТОР КАРТ И ПУБЛИКАЦИЯ";
    ru["menu.lobby"] = "МУЛЬТИПЛЕЕР И ЛОББИ КОМНАТЫ";
    ru["menu.workshop"] = "ИГРОТЕКА И МАСТЕРСКАЯ ТРАСС (UGC)";
    ru["menu.settings"] = "НАСТРОЙКИ";
    ru["menu.quit"] = "ВЫХОД";
    
    // Game Modes
    ru["modes.speed_arena"] = "Арена Скорости";
    ru["modes.speed_arena_desc"] = "Кольцевые гонки на выживание в стиле Rock'n'Roll Racing: чекпоинты, оружие, мины и штрафы за вылет!";
    ru["modes.road_avengers"] = "Мстители Дороги";
    ru["modes.road_avengers_desc"] = "Боевая арена на уничтожение в стиле Vigilante 8: спавн в разных точках и битва до последнего выжившего.";

    // HUD & In-Game
    ru["hud.speed"] = "КМ/Ч";
    ru["hud.lap"] = "КРУГ";
    ru["hud.pos"] = "МЕСТО";
    ru["hud.hull"] = "ПРОЧНОСТЬ";
    ru["hud.nitro"] = "НИТРО";
    ru["hud.ammo"] = "ЗАРЯДЫ";
    ru["hud.offtrack_warning"] = "ВЫЛЕТ С ТРАССЫ!";
    ru["hud.respawn_penalty"] = "Возврат на трассу через {0}с";
    ru["hud.target_locked"] = "ЦЕЛЬ ЗАХВАЧЕНА";
    
    // Match End
    ru["game.victory"] = "ПОБЕДА!";
    ru["game.eliminated"] = "ВЫБЫЛ ИЗ БОЯ";
    ru["game.match_finished"] = "МАТЧ ЗАВЕРШЕН";

    // 2. English Localization
    auto& en = m_dictionaries[Language::English];
    en["app.title"] = "Rusty Rage";
    en["app.subtitle"] = "3D MULTIPLAYER VEHICULAR COMBAT";
    en["menu.play"] = "PLAY";
    en["menu.garage"] = "VEHICLE GARAGE & TUNING";
    en["menu.editor"] = "3D TRACK BUILDER & PUBLISHER";
    en["menu.lobby"] = "MULTIPLAYER & LOBBY ROOMS";
    en["menu.workshop"] = "WORKSHOP & MAP LIBRARY (UGC)";
    en["menu.settings"] = "SETTINGS";
    en["menu.quit"] = "QUIT";

    // Game Modes
    en["modes.speed_arena"] = "Speed Arena";
    en["modes.speed_arena_desc"] = "Rock'n'Roll Racing style circuit races: checkpoints, weapon boxes, mines, and off-track penalties!";
    en["modes.road_avengers"] = "Road Avengers";
    en["modes.road_avengers_desc"] = "Vigilante 8 style demolition combat arena: scattered spawns and battle to the last survivor.";

    // HUD & In-Game
    en["hud.speed"] = "KM/H";
    en["hud.lap"] = "LAP";
    en["hud.pos"] = "POS";
    en["hud.hull"] = "HULL";
    en["hud.nitro"] = "NITRO";
    en["hud.ammo"] = "AMMO";
    en["hud.offtrack_warning"] = "OFF-TRACK HAZARD!";
    en["hud.respawn_penalty"] = "Respawn penalty in {0}s";
    en["hud.target_locked"] = "TARGET LOCKED";
    
    // Match End
    en["game.victory"] = "VICTORY!";
    en["game.eliminated"] = "ELIMINATED";
    en["game.match_finished"] = "MATCH FINISHED";
}

} // namespace Core
