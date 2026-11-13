#pragma once

#include "rr/world/MapData.hpp"
#include "rr/world/MapDiagnostics.hpp"
#include "rr/world/MapLimits.hpp"

namespace rr::world {

/// Validates a parsed map without mutating it. Errors make the map invalid;
/// warnings describe suspicious but accepted data.
class MapValidator {
public:
    [[nodiscard]] static MapValidationResult validate(const MapData& data,
                                                       const MapLimits& limits);
};

} // namespace rr::world
