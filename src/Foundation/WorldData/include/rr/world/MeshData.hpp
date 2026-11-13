#pragma once

#include "rr/core/Math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rr::world {

/// Stable project-owned surface/material binding used by both mesh outputs.
/// Numeric ids are local to a generated mesh and are not Jolt material indices,
/// GPU material indices or network ids.
struct SurfaceBinding {
    std::uint32_t surface_id{0};   // 1-based; 0 is reserved for "none"
    std::string surface_name{};
    std::uint32_t material_id{0};  // 1-based; 0 means no renderer material
    std::string material_name{};
};

struct RenderVertex {
    rr::math::Vec3 position{};
    rr::math::Vec3 normal{};
    rr::math::Vec3 tangent{};
    rr::math::Vec2 uv{};
};

struct SubMesh {
    std::uint32_t index_offset{0}; // index elements, not bytes
    std::uint32_t index_count{0};  // complete triangles only
    std::uint32_t material_id{0};
    std::uint32_t surface_id{0};
};

struct RenderMeshData {
    std::vector<RenderVertex> vertices{};
    std::vector<std::uint32_t> indices{};
    std::vector<SubMesh> submeshes{};
    std::vector<SurfaceBinding> surfaces{};
};

/// Backend-neutral triangle collision surface. `triangle_surface_ids` is
/// parallel to triangle indices; every value must resolve through `surfaces`.
struct CollisionMeshData {
    std::vector<rr::math::Vec3> positions{};
    std::vector<std::uint32_t> indices{};
    std::vector<std::uint32_t> triangle_surface_ids{};
    std::vector<SurfaceBinding> surfaces{};
};

struct TrackMeshData {
    std::string track_id{};
    RenderMeshData render{};
    CollisionMeshData collision{};
};

struct MeshGenerationConfig {
    bool include_render{true};
    bool include_collision{true};
    float min_triangle_area_m2{1.0e-8f};
};

struct TrackMeshEntry {
    std::string track_id{};
    TrackMeshData mesh{};
};

} // namespace rr::world
