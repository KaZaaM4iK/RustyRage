#include "Game/Track.hpp"
#include "Core/Logging.hpp"

#include <utility>

namespace Game {

Track::Track()
    : m_mapPath(), m_loaded(false) {}

Track::Track(const std::string& mapPath)
    : m_mapPath(), m_loaded(false) {
    load(mapPath);
}

bool Track::load(const std::string& mapPath) {
    m_mapPath = mapPath;
    m_loaded = m_data.loadFromFile(mapPath);
    if (m_loaded) {
        LOG_INFOF("Track loaded: %s (%zu objects, %zu track points, closed=%s)",
                  mapPath.c_str(),
                  m_data.objects.size(),
                  m_data.trackPoints.size(),
                  m_data.trackClosed ? "yes" : "no");
    } else {
        LOG_ERRORF("Failed to load track from %s", mapPath.c_str());
    }
    return m_loaded;
}

} // namespace Game
