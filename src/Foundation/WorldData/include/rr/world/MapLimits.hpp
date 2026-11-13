#pragma once

#include <cstddef>

namespace rr::world {

/// Hard limits applied before and after JSON parsing.
///
/// The default document limit is the TASK_002 starting limit of 2 MiB. These
/// are safety limits, not a claim that every future user map must fit them.
struct MapLimits {
    std::size_t max_document_bytes{2u * 1024u * 1024u};
    std::size_t max_json_nesting_depth{64};

    std::size_t max_surfaces{128};
    std::size_t max_tracks{64};
    std::size_t max_control_points_per_track{4096};
    std::size_t max_total_control_points{16384};
    std::size_t max_checkpoints{1024};
    std::size_t max_routes{128};
    std::size_t max_route_edges{4096};
    std::size_t max_spawn_points{128};
    std::size_t max_pickup_spawns{512};
    std::size_t max_interactive_objects{1024};
    std::size_t max_destructibles{1024};
    std::size_t max_trigger_zones{1024};
    std::size_t max_safe_zone_phases{64};

    std::size_t max_string_bytes{128};
    float max_coordinate_abs_m{1.0e6f};
    float min_road_width_m{0.5f};
    float max_road_width_m{10000.0f};
    float max_banking_rad{1.5f};
    float min_texture_repeat_m{0.01f};
    std::size_t max_identifier_bytes{64};
    std::size_t max_kind_string_bytes{64};
};

} // namespace rr::world
