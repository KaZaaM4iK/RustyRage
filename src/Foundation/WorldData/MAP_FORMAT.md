# RustyRage Map Format — TASK_002 Schema 1

- **Public schema version:** `schema_version = 1`
- **Compatibility:** this is **not** legacy `MapData` v3. No legacy importer or implicit compatibility is provided.
- **Serializer:** `rr::world::MapSerializer`
- **Validator:** `rr::world::MapValidator`
- **Private implementation dependency:** nlohmann_json 3.12.0. JSON types do not appear in public headers.

## Version policy

- Every document has a required top-level integer `schema_version`.
- Only exactly `1` is accepted by this implementation.
- Missing, non-integer, zero, negative or unsupported versions are rejected before map data is used.
- A future format change must increment `schema_version`; it must not silently reinterpret schema 1.
- Legacy MapData v3 files are not accepted and are not converted. Saved legacy maps were not found in TASK_002 Stage A.

## Unknown-field policy

The schema-1 parser is **strict**:

- every object has an explicit allow-list of fields;
- an unknown field is an error with a JSON path;
- there is no “ignore unknown optional fields” mode;
- a future additive field requires a schema version decision or an explicit approved compatibility policy.

This prevents silent loss of UGC data and requires an explicit migration when the schema evolves.

## Stable identifiers

- IDs are strings with a bounded length; allowed characters are ASCII alphanumerics, `_`, `-`, and `.`.
- IDs are stable and must be unique within the map document.
- References between routes, checkpoints, tracks and surfaces use those IDs, never vector positions.
- Duplicate IDs and unknown references are validation errors.

## Limits and safety

- The default document budget is 2 MiB (`MapLimits::max_document_bytes`), configurable by the caller.
- Input size is checked before JSON parsing.
- Nesting depth, collection counts, string lengths, numeric ranges and coordinates are checked afterwards and by `MapValidator`.
- Non-finite numbers are rejected.
- Trigger data is declarative only: no scripts, native code, file paths, network endpoints or arbitrary executable payloads.
- RTS/base-building entities are not part of this schema.

## Top-level fields

`map_id`, `schema_version`, `name`, `author`, `space`, `max_players`, `surfaces`, `tracks`, `checkpoints`, `routes`, `spawn_points`, `pickup_spawns`, `interactive_objects`, `destructibles`, `trigger_zones`, `survival_safe_zone`.

`space` is one of `racing`, `survival` or `mixed`. `max_players` is 1–8.

## Coordinate contract

Quaternions serialize as `[x, y, z, w]`; see `COORDINATE_SYSTEM.md` for handedness, axes, CCW, matrix and banking conventions.
