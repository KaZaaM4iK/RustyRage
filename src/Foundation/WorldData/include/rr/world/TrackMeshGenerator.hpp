#pragma once

#include "rr/world/MapData.hpp"
#include "rr/world/MeshData.hpp"
#include "rr/world/TrackSplineSampler.hpp"

#include <optional>
#include <string>
#include <vector>

namespace rr::world {

/// Generates neutral render and collision meshes from one shared sampled track.
/// No Jolt, SDL, GPU, renderer or networking type is used here.
class TrackMeshGenerator {
public:
    [[nodiscard]] static std::optional<TrackMeshData> generate(
        const TrackSpline& spline,
        const std::vector<TrackSurface>& surfaces,
        const TrackSamplingConfig& sampling,
        const MeshGenerationConfig& mesh_config,
        std::string& error);

    /// Generates each TrackSpline as an independent module. No N-way boolean
    /// merge is performed; branch/cut tracks are separate entries.
    [[nodiscard]] static std::optional<std::vector<TrackMeshEntry>> generate_all(
        const MapData& map,
        const TrackSamplingConfig& sampling,
        const MeshGenerationConfig& mesh_config,
        std::string& error);
};

} // namespace rr::world
