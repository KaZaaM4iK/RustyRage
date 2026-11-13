#pragma once

#include "rr/core/BinaryReader.hpp"
#include "rr/core/BinaryWriter.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace rr::world {

inline constexpr std::uint32_t kMapFormatVersion = 1;
inline constexpr std::size_t kMaxMapIdBytes = 64;
inline constexpr std::uint32_t kMaxPlayersPerMatch = 8;

/// Versioned, explicitly serialized map identity header.
///
/// This is the shared client/server/editor boundary for map metadata. Full map
/// geometry and validation belong to a later task; this header establishes
/// versioning and bounds checking without depending on client assets.
struct MapHeader {
    std::uint32_t format_version{kMapFormatVersion};
    std::string map_id{};
    std::uint32_t max_players{kMaxPlayersPerMatch};

    [[nodiscard]] bool validate(std::string& error) const;

    /// Throws std::invalid_argument when the header is invalid.
    void serialize(core::BinaryWriter& writer) const;

    [[nodiscard]] static std::optional<MapHeader> deserialize(
        core::BinaryReader& reader,
        std::string& error);
};

} // namespace rr::world
