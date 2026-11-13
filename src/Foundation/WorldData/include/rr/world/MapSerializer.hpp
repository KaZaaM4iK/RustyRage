#pragma once

#include "rr/world/MapData.hpp"
#include "rr/world/MapLimits.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace rr::world {

/// Versioned, bounded JSON serializer. nlohmann_json is a private
/// implementation detail; no JSON type appears in this public header.
class MapSerializer {
public:
    /// Parses and applies structural limits. Semantic issues are reported by
    /// MapValidator; malformed JSON, unsupported schema versions and unknown
    /// fields are rejected here.
    [[nodiscard]] static std::optional<MapData> parse(std::string_view json,
                                                       const MapLimits& limits,
                                                       std::string& error);

    [[nodiscard]] static std::optional<MapData> load_file(const std::string& path,
                                                          const MapLimits& limits,
                                                          std::string& error);

    /// Returns an empty string on failure (error is filled).
    [[nodiscard]] static std::string write(const MapData& data,
                                           const MapLimits& limits,
                                           std::string& error);

    static bool write_file(const std::string& path,
                           const MapData& data,
                           const MapLimits& limits,
                           std::string& error);
};

} // namespace rr::world
