#pragma once

#include "rr/core/Math.hpp"
#include "rr/world/MapData.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace rr::world {

struct ArcLengthEntry {
    float distance_m{0.0f};
    std::size_t segment_index{0};
    float segment_t{0.0f};
};

/// Monotonic cumulative-distance lookup table built from an adaptive spline
/// polyline. Entries are ordered by distance; the closing entry of a closed
/// spline is present even though the duplicate output vertex is omitted.
class ArcLengthTable {
public:
    [[nodiscard]] float total_length_m() const noexcept;
    [[nodiscard]] const std::vector<ArcLengthEntry>& entries() const noexcept;

    /// Robust distance -> (segment, local t) lookup. Returns false for an empty
    /// table or non-finite/negative distance.
    [[nodiscard]] bool parameter_at_distance(float distance_m,
                                             std::size_t& segment_index,
                                             float& segment_t) const noexcept;

private:
    friend class TrackSplineSampler;
    std::vector<ArcLengthEntry> entries_;
};

struct TrackSample {
    rr::math::Vec3 position{};
    rr::math::Vec3 tangent{};
    rr::math::Vec3 right{};
    rr::math::Vec3 up{};
    float distance_m{0.0f};
    float uv_v{0.0f};
    float road_width_m{0.0f};
    float banking_rad{0.0f};
    std::string surface_id{};
    std::size_t segment_index{0};
    float segment_t{0.0f};
    float holonomy_correction_rad{0.0f};
};

struct SampledTrack {
    std::vector<TrackSample> samples{};
    ArcLengthTable arc_length{};
    bool closed{false};
    float total_length_m{0.0f};
    float seam_position_error_m{0.0f};
    float seam_tangent_error_rad{0.0f};
    float seam_angular_error_rad{0.0f};
    float holonomy_residual_rad{0.0f};
    float max_orthonormality_error{0.0f};
};

struct TrackSamplingConfig {
    std::size_t max_samples{16384};
    std::size_t max_subdivision_depth{16};
    float max_tangent_angle_error_rad{0.2f};
    float orthonormal_tolerance{1.0e-3f};
};

/// Centripetal Catmull-Rom sampler with arc-length LUT, adaptive subdivision,
/// RMF/Parallel Transport, banking and closed-loop holonomy correction.
///
/// The sampler is deterministic and independent of renderer/physics backends.
class TrackSplineSampler {
public:
    [[nodiscard]] static std::optional<SampledTrack> sample(
        const TrackSpline& spline,
        const TrackSamplingConfig& config,
        std::string& error);
};

} // namespace rr::world
