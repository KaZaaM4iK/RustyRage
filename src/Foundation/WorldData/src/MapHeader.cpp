#include "rr/world/MapHeader.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace rr::world {
namespace {

bool is_safe_map_id_char(char value) {
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_' || value == '-' || value == '.';
}

} // namespace

bool MapHeader::validate(std::string& error) const {
    if (format_version != kMapFormatVersion) {
        error = "unsupported map format version";
        return false;
    }
    if (map_id.empty()) {
        error = "map_id must not be empty";
        return false;
    }
    if (map_id.size() > kMaxMapIdBytes) {
        error = "map_id exceeds maximum length";
        return false;
    }
    if (!std::all_of(map_id.begin(), map_id.end(), is_safe_map_id_char)) {
        error = "map_id contains unsupported characters";
        return false;
    }
    if (max_players == 0 || max_players > kMaxPlayersPerMatch) {
        error = "max_players is outside the supported range";
        return false;
    }

    error.clear();
    return true;
}

void MapHeader::serialize(core::BinaryWriter& writer) const {
    std::string error;
    if (!validate(error)) {
        throw std::invalid_argument("MapHeader::serialize: " + error);
    }

    writer.write_u32(format_version);
    writer.write_string(map_id, kMaxMapIdBytes);
    writer.write_u32(max_players);
}

std::optional<MapHeader> MapHeader::deserialize(core::BinaryReader& reader, std::string& error) {
    MapHeader header;

    if (!reader.read_u32(header.format_version) ||
        !reader.read_string(header.map_id, kMaxMapIdBytes) ||
        !reader.read_u32(header.max_players)) {
        error = "truncated MapHeader: ";
        error += reader.error();
        return std::nullopt;
    }

    if (!header.validate(error)) {
        return std::nullopt;
    }

    error.clear();
    return header;
}

} // namespace rr::world
