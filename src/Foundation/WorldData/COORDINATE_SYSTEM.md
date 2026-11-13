# RustyRage World and Track Coordinate Contract

**Status:** normative for `rr_world` data and generated geometry.  
**Applies to:** client, dedicated server, future editor, render and physics adapters.  
**Not a GPU convention:** conversion to a graphics API clip space belongs to `rr_render`.

## 1. Handedness and axes

- World space is **right-handed**.
- `+X` points to world **right**.
- `+Y` points to world **up**.
- `+Z` points to the agreed **forward** direction for the current vehicle convention.
- The right-handed cross-product identities are:
  - `cross(+X, +Y) = +Z`
  - `cross(+Y, +Z) = +X`
  - `cross(+Z, +X) = +Y`
- The vehicle's local forward axis is `+Z`; the vehicle's local up axis is `+Y`.
- Positive yaw is a right-handed rotation about `+Y`. It rotates the forward vector `+Z` toward `+X` (the vehicle's right).
- Positive pitch is a right-handed rotation about `+X`; positive roll is a right-handed rotation about `+Z`.
- Do not apply backend-specific axis flips to map data. The renderer owns World → View → Clip conversion and must preserve this world contract.

## 2. Units and scalar conventions

- Distances: metres.
- Time: seconds.
- Angles: radians.
- Mass: kilograms.
- Banking, slopes and route angular values are radians.
- No implicit degree/radian conversion exists in `rr_world`.

## 3. Quaternion convention

- Public quaternion values are unit quaternions.
- Serialized quaternions use the explicit array order **`[x, y, z, w]`**.
- In C++, `rr::math::Quat` is a GLM quaternion. GLM's constructor order is `(w, x, y, z)`; never construct a public value from JSON without converting the order explicitly.
- Identity is `[x, y, z, w] = [0, 0, 0, 1]`.
- A vector is rotated by `q * v * conjugate(q)` (equivalently GLM's `q * v` for a unit quaternion).
- Composition uses Hamilton product order; a parent rotation followed by a child rotation is represented as `q_parent * q_child`.
- Quaternions must remain finite and approximately unit length. Validation rejects non-finite or non-unit quaternions.
- No aligned/`GLM_FORCE_ALIGNED_GENTYPES` layout is enabled; do not assume a packed ABI for serialization. Quaternions are serialized field by field, never by raw memory copy.

## 4. Matrix convention

- Matrices are column-major GLM matrices (`glm::mat4` / `rr::math::Mat4`).
- Matrix-vector multiplication is `M * v`.
- Translation is stored in column 3 (`m[3]` in GLSL/GLM indexing).
- Composition is standard column-vector order: `M_total = M_projection * M_view * M_model`; the model transform is applied first.
- Projection uses `GLM_FORCE_DEPTH_ZERO_TO_ONE`, configured centrally through `rr_core`. Do not introduce a second depth convention in `rr_world`.
- `GLM_FORCE_LEFT_HANDED` and `GLM_FORCE_RADIANS` are forbidden.

## 5. Track frame: tangent, right, up

For each sampled centerline location the track frame is:

- `T` — unit tangent in the direction of increasing travel/control-point order;
- `R` — local right vector;
- `U` — local up vector.

Base frame construction (RMF starts from this deterministic reference and then parallel-transports it):

```text
T = normalize(tangent)
U_ref = +Y unless |dot(T, +Y)| is too close to 1
        (then U_ref = +Z as the deterministic fallback)
R = normalize(cross(U_ref, T))
U = normalize(cross(T, R))
```

For a horizontal track traveling along `+Z`, this gives `T=+Z`, `R=+X`, `U=+Y`. This is the reference case for normal-direction tests.

The frame must be re-orthonormalized after transport. Reject or diagnose degenerate tangents instead of producing NaN/Inf.

## 6. Banking

Positive `banking_rad` raises the road's **right** side relative to the unbanked frame:

```text
R_bank = R * cos(bank) + U * sin(bank)
U_bank = -R * sin(bank) + U * cos(bank)
```

Banking rotates the complete cross-section (road and both shoulders) consistently. The resulting frame must remain orthonormal within the documented numeric tolerance.

## 7. Cross-section and left/right

The cross-section is ordered as:

```text
[ Left Shoulder | Main Road | Right Shoulder ]
```

- `right` is `+R_bank`; `left` is `-R_bank`.
- The main road spans `center ± road_width_m/2`.
- Shoulders extend outward from the main road edges.
- A positive shoulder slope lowers the outer shoulder edge; exact slope sign is applied consistently by the generator.
- Shoulders do not by themselves guarantee that a vehicle cannot leave the track. Barriers, curbs and guard rails are separate map objects with their own collision intent.

## 8. Winding and CCW

The operational definition of CCW is authoritative:

```text
normal = normalize(cross(p1 - p0, p2 - p0))
```

A front-facing triangle is CCW when this normal points toward the intended **exterior/front side** of the surface. For a horizontal road top surface, the exterior side is **above** the road (`+Y`), so:

```text
normal == +Y
```

for a correctly wound top-facing triangle. Equivalently, an observer on the exterior (`+Y`) side looking toward the surface sees the vertex order as counter-clockwise. Do not infer CCW from an arbitrary 2D viewport, and do not disable backface culling to hide reversed winding.

For generated road meshes:

- render and collision index buffers use the same orientation;
- the renderer configures `SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE`;
- if a negative-determinant transform reverses winding, handle it explicitly in the renderer/transform contract, never by changing map geometry silently;
- GPU NDC conventions do not change this world-space contract.

## 9. Surface identity

- `surface_id` is a stable project-owned identifier.
- A surface ID is **not** a Jolt `PhysicsMaterialList` index and is not an SDL/GPU material handle.
- `renderer_material_id` is an opaque project reference, not a GPU object.
- Discrete surface IDs are not numerically interpolated. A segment uses an explicit point/sample boundary rule; transitions must be generated intentionally.

## 10. Coordinate invariants for tests

- Horizontal top-facing road triangles have `+Y` normals.
- A vehicle looking along `+Z` has its right side along `+X`.
- Positive yaw rotates `+Z` toward `+X`.
- Positive banking raises the right edge.
- Closed-loop seam correction is distributed by cumulative arc length, never by control-point index or segment count.
