#pragma once

#include "rr/core/Math.hpp"

namespace rr::world::detail {

/// Internal frame transport seam. This header lives under `src/`, is not part
/// of the public `rr_world` include tree, and exists so tests can exercise the
/// exact antiparallel fallback without widening the public API.
struct TrackFrame {
    rr::math::Vec3 tangent{};
    rr::math::Vec3 right{};
    rr::math::Vec3 up{};
};

/// Minimal rotation from `from` to `to`. Inputs are expected to be unit
/// vectors. Near-opposite inputs use the documented deterministic fallback.
[[nodiscard]] rr::math::Quat minimal_rotation(rr::math::Vec3 from,
                                              rr::math::Vec3 to);

/// Rotate/parallel-transport a frame to `next_tangent`, then re-orthonormalize.
[[nodiscard]] TrackFrame transport_frame(const TrackFrame& previous,
                                         rr::math::Vec3 next_tangent);

} // namespace rr::world::detail
