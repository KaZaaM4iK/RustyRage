#include "rr/world/TrackMeshGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rr::world {
namespace {

constexpr float kEpsilon = 1.0e-6f;
constexpr std::size_t kRingVertexCount = 6;

// Ring columns increase left -> right:
// 0 left outer, 1 left shoulder inner, 2 left road edge,
// 3 right road edge, 4 right shoulder inner, 5 right outer.
struct Ring {
    std::array<rr::math::Vec3, kRingVertexCount> positions{};
    std::array<rr::math::Vec3, kRingVertexCount> normals{};
    std::array<float, kRingVertexCount> u{};
    rr::math::Vec3 tangent{};
};

struct SurfaceTable {
    std::vector<SurfaceBinding> bindings{};
    std::map<std::string, std::uint32_t> by_name{};
};

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

bool finite(rr::math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float length(rr::math::Vec3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

rr::math::Vec3 normalized(rr::math::Vec3 value, const char* context) {
    const float magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude <= kEpsilon) {
        fail(std::string(context) + ": degenerate vector");
    }
    return value / magnitude;
}

float triangle_area(rr::math::Vec3 a, rr::math::Vec3 b, rr::math::Vec3 c) {
    return 0.5f * length(glm::cross(b - a, c - a));
}

SurfaceTable build_surface_table(const std::vector<TrackSurface>& surfaces) {
    if (surfaces.empty()) {
        fail("mesh generation requires at least one surface");
    }

    std::vector<const TrackSurface*> sorted;
    sorted.reserve(surfaces.size());
    for (const TrackSurface& surface : surfaces) {
        if (surface.surface_id.empty()) {
            fail("surface with empty surface_id cannot be used for mesh generation");
        }
        sorted.push_back(&surface);
    }
    std::sort(sorted.begin(), sorted.end(), [](const TrackSurface* first, const TrackSurface* second) {
        return first->surface_id < second->surface_id;
    });

    std::vector<std::string> materials;
    for (const TrackSurface* surface : sorted) {
        if (!surface->renderer_material_id.empty()) {
            materials.push_back(surface->renderer_material_id);
        }
    }
    std::sort(materials.begin(), materials.end());
    materials.erase(std::unique(materials.begin(), materials.end()), materials.end());

    std::map<std::string, std::uint32_t> material_ids;
    for (std::size_t index = 0; index < materials.size(); ++index) {
        material_ids[materials[index]] = static_cast<std::uint32_t>(index + 1);
    }

    SurfaceTable table;
    table.bindings.reserve(sorted.size());
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        const TrackSurface& surface = *sorted[index];
        if (table.by_name.find(surface.surface_id) != table.by_name.end()) {
            fail("duplicate surface_id in mesh surface table");
        }
        SurfaceBinding binding;
        binding.surface_id = static_cast<std::uint32_t>(index + 1);
        binding.surface_name = surface.surface_id;
        binding.material_name = surface.renderer_material_id;
        if (!surface.renderer_material_id.empty()) {
            binding.material_id = material_ids.at(surface.renderer_material_id);
        }
        table.by_name[surface.surface_id] = binding.surface_id;
        table.bindings.push_back(std::move(binding));
    }
    return table;
}

const SurfaceBinding& binding_for(const SurfaceTable& table,
                                 const std::string& surface_name,
                                 std::string_view context) {
    const auto iterator = table.by_name.find(surface_name);
    if (iterator == table.by_name.end()) {
        fail(std::string(context) + ": surface '" + surface_name + "' is not declared in the map");
    }
    for (const SurfaceBinding& binding : table.bindings) {
        if (binding.surface_id == iterator->second) {
            return binding;
        }
    }
    fail(std::string(context) + ": invalid surface table");
}

Ring make_ring(const TrackSample& sample,
               const CrossSectionProfile& profile,
               float slope_tangent) {
    if (!std::isfinite(sample.road_width_m) || sample.road_width_m <= kEpsilon) {
        fail("sampled road width must be finite and positive");
    }
    if (!finite(sample.position) || !finite(sample.right) || !finite(sample.up) ||
        !finite(sample.tangent)) {
        fail("sampled track frame contains non-finite data");
    }

    const float half_width = sample.road_width_m * 0.5f;
    const rr::math::Vec3 left_road = sample.position - sample.right * half_width;
    const rr::math::Vec3 right_road = sample.position + sample.right * half_width;
    const float left_width = profile.left_shoulder_width_m;
    const float right_width = profile.right_shoulder_width_m;
    const rr::math::Vec3 left_outer = left_road - sample.right * left_width -
                                      sample.up * (left_width * slope_tangent);
    const rr::math::Vec3 right_outer = right_road + sample.right * right_width -
                                       sample.up * (right_width * slope_tangent);

    const rr::math::Vec3 left_shoulder_normal =
        normalized(sample.up - sample.right * slope_tangent, "left shoulder normal");
    const rr::math::Vec3 right_shoulder_normal =
        normalized(sample.up + sample.right * slope_tangent, "right shoulder normal");
    const rr::math::Vec3 road_normal = normalized(sample.up, "road normal");

    Ring ring;
    ring.positions = {left_outer, left_road, left_road, right_road, right_road, right_outer};
    ring.normals = {left_shoulder_normal, left_shoulder_normal, road_normal, road_normal,
                    right_shoulder_normal, right_shoulder_normal};
    ring.u = {-left_width / sample.road_width_m, 0.0f, 0.0f, 1.0f, 1.0f,
              1.0f + right_width / sample.road_width_m};
    ring.tangent = sample.tangent;
    return ring;
}

struct TriangleRecord {
    std::uint32_t a{0};
    std::uint32_t b{0};
    std::uint32_t c{0};
    std::uint32_t surface_id{0};
    std::uint32_t material_id{0};
};

struct QuadColumns {
    std::size_t a{0};
    std::size_t b{0};
};

bool add_triangle(std::vector<TriangleRecord>& triangles,
                  std::uint32_t a,
                  std::uint32_t b,
                  std::uint32_t c,
                  std::uint32_t surface_id,
                  std::uint32_t material_id,
                  rr::math::Vec3 pa,
                  rr::math::Vec3 pb,
                  rr::math::Vec3 pc,
                  float min_area) {
    if (triangle_area(pa, pb, pc) <= min_area) {
        return false;
    }
    triangles.push_back(TriangleRecord{a, b, c, surface_id, material_id});
    return true;
}

void add_quad(std::vector<TriangleRecord>& triangles,
              std::uint32_t current_base,
              std::uint32_t next_base,
              const std::array<rr::math::Vec3, kRingVertexCount>& current_positions,
              const std::array<rr::math::Vec3, kRingVertexCount>& next_positions,
              const QuadColumns& columns,
              std::uint32_t surface_id,
              std::uint32_t material_id,
              float min_area) {
    const std::uint32_t a_current = current_base + static_cast<std::uint32_t>(columns.a);
    const std::uint32_t b_current = current_base + static_cast<std::uint32_t>(columns.b);
    const std::uint32_t a_next = next_base + static_cast<std::uint32_t>(columns.a);
    const std::uint32_t b_next = next_base + static_cast<std::uint32_t>(columns.b);

    (void)add_triangle(triangles, a_current, a_next, b_current, surface_id, material_id,
                       current_positions[columns.a], next_positions[columns.a],
                       current_positions[columns.b], min_area);
    (void)add_triangle(triangles, b_current, a_next, b_next, surface_id, material_id,
                       current_positions[columns.b], next_positions[columns.a],
                       next_positions[columns.b], min_area);
}

void build_render_mesh(const std::vector<Ring>& rings,
                       const std::vector<float>& ring_v,
                       const std::vector<TriangleRecord>& triangles,
                       const SurfaceTable& surface_table,
                       RenderMeshData& render) {
    render.surfaces = surface_table.bindings;
    render.vertices.reserve(rings.size() * kRingVertexCount);
    for (std::size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
        for (std::size_t column = 0; column < kRingVertexCount; ++column) {
            RenderVertex vertex;
            vertex.position = rings[ring_index].positions[column];
            vertex.normal = rings[ring_index].normals[column];
            vertex.tangent = rings[ring_index].tangent;
            vertex.uv = rr::math::Vec2{rings[ring_index].u[column], ring_v[ring_index]};
            render.vertices.push_back(vertex);
        }
    }

    std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::uint32_t>> groups;
    for (const TriangleRecord& triangle : triangles) {
        std::vector<std::uint32_t>& group =
            groups[{triangle.surface_id, triangle.material_id}];
        group.push_back(triangle.a);
        group.push_back(triangle.b);
        group.push_back(triangle.c);
    }

    for (const auto& group : groups) {
        if (render.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
            group.second.size() > std::numeric_limits<std::uint32_t>::max()) {
            fail("render mesh exceeds 32-bit index limits");
        }
        SubMesh submesh;
        submesh.index_offset = static_cast<std::uint32_t>(render.indices.size());
        submesh.index_count = static_cast<std::uint32_t>(group.second.size());
        submesh.surface_id = group.first.first;
        submesh.material_id = group.first.second;
        render.indices.insert(render.indices.end(), group.second.begin(), group.second.end());
        render.submeshes.push_back(submesh);
    }
}

void build_collision_mesh(const std::vector<Ring>& rings,
                          const std::vector<TriangleRecord>& triangles,
                          const SurfaceTable& surface_table,
                          CollisionMeshData& collision) {
    collision.surfaces = surface_table.bindings;
    collision.positions.reserve(rings.size() * kRingVertexCount);
    for (const Ring& ring : rings) {
        for (const rr::math::Vec3& position : ring.positions) {
            collision.positions.push_back(position);
        }
    }
    collision.indices.reserve(triangles.size() * 3);
    collision.triangle_surface_ids.reserve(triangles.size());
    for (const TriangleRecord& triangle : triangles) {
        collision.indices.push_back(triangle.a);
        collision.indices.push_back(triangle.b);
        collision.indices.push_back(triangle.c);
        collision.triangle_surface_ids.push_back(triangle.surface_id);
    }
}

} // namespace

std::optional<TrackMeshData> TrackMeshGenerator::generate(
    const TrackSpline& spline,
    const std::vector<TrackSurface>& surfaces,
    const TrackSamplingConfig& sampling,
    const MeshGenerationConfig& mesh_config,
    std::string& error) {
    try {
        if (!mesh_config.include_render && !mesh_config.include_collision) {
            fail("at least one mesh output must be requested");
        }
        if (!std::isfinite(mesh_config.min_triangle_area_m2) ||
            mesh_config.min_triangle_area_m2 < 0.0f) {
            fail("min_triangle_area_m2 must be finite and non-negative");
        }
        if (!std::isfinite(spline.cross_section.left_shoulder_width_m) ||
            spline.cross_section.left_shoulder_width_m < 0.0f ||
            !std::isfinite(spline.cross_section.right_shoulder_width_m) ||
            spline.cross_section.right_shoulder_width_m < 0.0f) {
            fail("shoulder widths must be finite and non-negative");
        }
        if (!std::isfinite(spline.cross_section.shoulder_slope_rad) ||
            std::fabs(spline.cross_section.shoulder_slope_rad) >= 1.5707964f) {
            fail("shoulder slope must be finite and below 90 degrees");
        }

        const SurfaceTable surface_table = build_surface_table(surfaces);
        // An empty shoulder reference is valid only for a zero-width shoulder.
        if (spline.cross_section.left_shoulder_width_m > kEpsilon) {
            (void)binding_for(surface_table, spline.cross_section.left_shoulder_surface_id,
                             "left shoulder");
        }
        if (spline.cross_section.right_shoulder_width_m > kEpsilon) {
            (void)binding_for(surface_table, spline.cross_section.right_shoulder_surface_id,
                             "right shoulder");
        }

        std::string sampling_error;
        const auto sampled = TrackSplineSampler::sample(spline, sampling, sampling_error);
        if (!sampled.has_value()) {
            fail("track sampling failed: " + sampling_error);
        }
        if (sampled->samples.size() < 2) {
            fail("track sampling produced fewer than two samples");
        }

        const float slope_tangent = std::tan(spline.cross_section.shoulder_slope_rad);
        std::vector<Ring> rings;
        std::vector<Ring> render_rings;
        std::vector<float> ring_v;
        rings.reserve(sampled->samples.size());
        for (const TrackSample& sample : sampled->samples) {
            if (sample.surface_id.empty()) {
                fail("sampled track has an empty road surface_id");
            }
            (void)binding_for(surface_table, sample.surface_id, "road");
            rings.push_back(make_ring(sample, spline.cross_section, slope_tangent));
        }
        ring_v.reserve(rings.size());
        for (const TrackSample& sample : sampled->samples) {
            ring_v.push_back(sample.distance_m / spline.texture_repeat_m);
        }

        render_rings = rings;
        if (sampled->closed) {
            render_rings.push_back(rings.front());
            ring_v.push_back(sampled->total_length_m / spline.texture_repeat_m);
        }

        std::vector<TriangleRecord> triangles;
        const std::size_t row_count = sampled->samples.size();
        const std::size_t row_iterations = sampled->closed ? row_count : row_count - 1;
        for (std::size_t row = 0; row < row_iterations; ++row) {
            const std::size_t render_next = (sampled->closed && row == row_count - 1)
                                                ? render_rings.size() - 1
                                                : row + 1;
            const std::uint32_t current_base = static_cast<std::uint32_t>(row * kRingVertexCount);
            const std::uint32_t next_render_base =
                static_cast<std::uint32_t>(render_next * kRingVertexCount);

            const auto add_region = [&](std::size_t column_a,
                                        std::size_t column_b,
                                        const std::string& surface_name,
                                        const char* context) {
                const SurfaceBinding& binding = binding_for(surface_table, surface_name, context);
                add_quad(triangles, current_base, next_render_base,
                         render_rings[row].positions, render_rings[render_next].positions,
                         QuadColumns{column_a, column_b},
                         binding.surface_id, binding.material_id,
                         mesh_config.min_triangle_area_m2);
            };

            if (spline.cross_section.left_shoulder_width_m > kEpsilon) {
                add_region(0, 1, spline.cross_section.left_shoulder_surface_id, "left shoulder");
            }
            add_region(2, 3, sampled->samples[row].surface_id, "road");
            if (spline.cross_section.right_shoulder_width_m > kEpsilon) {
                add_region(4, 5, spline.cross_section.right_shoulder_surface_id, "right shoulder");
            }

            // The render quad above uses the duplicated seam row. The collision
            // mesh uses the wrapped first row and has no duplicate vertex.
        }

        // Collision quads are generated in the same order from the non-duplicated
        // rings. Build them separately so render and collision can use different
        // seam-topology without changing the shared sampled geometry.
        std::vector<TriangleRecord> collision_triangles;
        for (std::size_t row = 0; row < row_iterations; ++row) {
            const std::size_t next_row = (row + 1) % row_count;
            const std::uint32_t current_base = static_cast<std::uint32_t>(row * kRingVertexCount);
            const std::uint32_t next_base = static_cast<std::uint32_t>(next_row * kRingVertexCount);
            const auto add_region = [&](std::size_t column_a,
                                        std::size_t column_b,
                                        const std::string& surface_name,
                                        const char* context) {
                const SurfaceBinding& binding = binding_for(surface_table, surface_name, context);
                add_quad(collision_triangles, current_base, next_base,
                         rings[row].positions, rings[next_row].positions,
                         QuadColumns{column_a, column_b},
                         binding.surface_id, binding.material_id,
                         mesh_config.min_triangle_area_m2);
            };
            if (spline.cross_section.left_shoulder_width_m > kEpsilon) {
                add_region(0, 1, spline.cross_section.left_shoulder_surface_id, "left shoulder");
            }
            add_region(2, 3, sampled->samples[row].surface_id, "road");
            if (spline.cross_section.right_shoulder_width_m > kEpsilon) {
                add_region(4, 5, spline.cross_section.right_shoulder_surface_id, "right shoulder");
            }
        }

        if (triangles.size() != collision_triangles.size()) {
            fail("render and collision generation produced different triangle counts");
        }

        TrackMeshData result;
        result.track_id = spline.track_id;
        if (mesh_config.include_collision) {
            build_collision_mesh(rings, collision_triangles, surface_table, result.collision);
        }
        if (mesh_config.include_render) {
            build_render_mesh(render_rings, ring_v, triangles, surface_table, result.render);
        }
        error.clear();
        return result;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

std::optional<std::vector<TrackMeshEntry>> TrackMeshGenerator::generate_all(
    const MapData& map,
    const TrackSamplingConfig& sampling,
    const MeshGenerationConfig& mesh_config,
    std::string& error) {
    try {
        if (map.tracks.empty()) {
            fail("map has no tracks to generate");
        }
        std::vector<TrackMeshEntry> entries;
        entries.reserve(map.tracks.size());
        for (const TrackSpline& track : map.tracks) {
            std::string track_error;
            auto mesh = generate(track, map.surfaces, sampling, mesh_config, track_error);
            if (!mesh.has_value()) {
                fail("track '" + track.track_id + "': " + track_error);
            }
            entries.push_back(TrackMeshEntry{track.track_id, std::move(*mesh)});
        }
        error.clear();
        return entries;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

} // namespace rr::world
