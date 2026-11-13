# DECISIONS — RustyRage 0.1

## D-001 — Архитектурная база 0.1
**Статус:** APPROVED / FROZEN.  
Фундаментальная архитектура, границы модулей и правила исполнения задач утверждены. Реальные сборки и интеграции подтверждаются только конкретными отчётами по платформам.

## D-002 — C++ и сборка
**Статус:** APPROVED.  
Проект использует C++20 и CMake 3.28+ как базовую конфигурацию. Jolt собирается в отдельном target с собственным требуемым стандартом C++17.

## D-003 — Physics boundary
**Статус:** APPROVED.  
Симуляция зависит от крупноблочного `rr_physics_api`; конкретный backend реализуется в `rr_physics_jolt`. Не создавать виртуальный вызов на каждое тело/форму Jolt.

## D-004 — Network architecture
**Статус:** APPROVED.  
Dedicated server authority; prediction/reconciliation local vehicle; remote interpolation; reconnect token; no physics rewind for ramming. GNS остаётся `PROPOSED` до требуемых portability checks.

## D-005 — Graphics and platform
**Статус:** APPROVED.  
SDL3 platform layer; SDL_GPU — основной графический кандидат; SDL_shadercross — offline shader preparation path; bgfx — только резерв при доказанном блокере. SDL_GPU runtime проверен только в Linux x86_64 через software Vulkan llvmpipe.

## D-006 — Map data
**Статус:** APPROVED.  
Общая модель `rr_world` доступна клиенту, серверу и будущему редактору без зависимости от клиентского AssetRuntime, SDL_GPU или Jolt. Подробные контракты данных и геометрии для следующего этапа определены в `TASKS/active/TASK_002_WORLD_DATA_TRACK_SPLINES.md`.

## D-007 — UGC progression
**Статус:** APPROVED.  
Этап 1: локальный редактор, импорт/экспорт, серверная валидация, закрытые комнаты. Этап 2: публичный каталог, публикация, рейтинги и модерация. Формат с первого этапа учитывает версии и хеширование.

## D-008 — DeepSeek execution
**Статус:** APPROVED.  
DeepSeek работает через Harness по задачам из `TASKS/active/`. Не меняет утверждённые спеки, зависимости, публичные интерфейсы и архитектурные границы без согласования.

## D-009 — TASK_001 Foundation Smoke Test
**Статус:** COMPLETED_WITH_LIMITATIONS; принят владельцем для Linux x86_64.  
Отчёт DeepSeek подтверждает успешные Linux Debug (6/6 CTest, 35 unit tests), Release (6/6 CTest) и Server-only Debug (5/5 CTest), target graph (13 first-party targets, 34 edges, 0 cycles), Jolt vehicle smoke, SDL3/SDL_GPU smoke через Vulkan llvmpipe и GNS loopback с двумя headless clients. Windows, macOS, FreeBSD, Android и iOS остаются `NOT_TESTED_IN_ENV`. GNS остаётся `PROPOSED`; SDL_GPU не считается проверенным на физическом GPU или остальных backend/platform combinations. Подробные доказательства — в фактических отчётах `REPORTS/` рабочего репозитория.

## D-010 — Approved dependencies for TASK_002
**Статус:** APPROVED.  
- GLM `1.0.3` — централизованная math dependency через `rr_core` / `rr::math`.
- nlohmann_json `3.12.0` — private implementation dependency сериализатора `rr_world`; JSON-типы не протекают в публичную модель карт.
- Точные immutable commit SHA или archive hashes, license files и транзитивные зависимости должны быть зафиксированы DeepSeek в Stage A TASK_002 до включения библиотек в сборку.

## D-011 — Coordinate and geometry conventions
**Статус:** APPROVED.  
World space: right-handed, `+Y` up, `+X` right, `+Z` forward; metres, seconds, radians, kilograms. Generated front faces use CCW winding as viewed from the intended outside/front side; horizontal top-surface normals point `+Y`. GPU clip-space conversion belongs to `rr_render`, not `rr_world`. GLM uses centralized configuration with `GLM_FORCE_DEPTH_ZERO_TO_ONE`; do not define obsolete `GLM_FORCE_RADIANS`.

## D-012 — TASK_002 geometry contracts
**Статус:** APPROVED.  
Centripetal Catmull–Rom (`alpha = 0.5`), arc-length lookup/sampling, adaptive tessellation, UV V based on cumulative metres, RMF/Parallel Transport, length-proportional holonomy correction for closed loops, banking, configurable left-shoulder/main-road/right-shoulder profile, `RenderMeshData` SubMesh ranges, per-triangle `CollisionMeshData::triangle_surface_ids`, and limited helper-spline/modular junctions. No general N-way boolean mesh merger. `rr_world` remains independent of Jolt and SDL_GPU. Full requirements are in TASK_002.

## D-013 — TASK_002 execution authorization
**Статус:** APPROVED / READY FOR EXECUTION.  
`TASK_002: World Data, Track Splines and Geometry` is approved for DeepSeek through Harness. Execute stages A → B → C → D → E with mandatory gates and reports. Start with legacy map inventory; do not port RTS/base-building entities or implement a legacy importer unless useful saved maps are found and the owner approves the migration approach.
