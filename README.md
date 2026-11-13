# RustyRage — стартовый пакет DeepSeek / Harness

**Архитектура:** `0.1` — APPROVED / FROZEN  
**Foundation:** TASK_001 completed and accepted for Linux x86_64 with platform limitations  
**Текущая задача:** `TASK_002: World Data, Track Splines and Geometry` — APPROVED / READY FOR EXECUTION

## С чего начать

1. Прочитать [`AGENTS.md`](AGENTS.md).
2. Прочитать [`PROJECT_CONTEXT/README.md`](PROJECT_CONTEXT/README.md) и [`PROJECT_CONTEXT/ARCHITECTURE_STATUS.md`](PROJECT_CONTEXT/ARCHITECTURE_STATUS.md).
3. Прочитать [`DECISIONS.md`](DECISIONS.md) и [`SPECIFICATIONS/README.md`](SPECIFICATIONS/README.md).
4. Выполнять только активную задачу [`TASKS/active/TASK_002_WORLD_DATA_TRACK_SPLINES.md`](TASKS/active/TASK_002_WORLD_DATA_TRACK_SPLINES.md).
5. Использовать `TASKS/completed/TASK_001_FOUNDATION_SMOKE_TEST.md` для истории и ограничений Foundation.
6. Записывать фактические результаты и логи в `REPORTS/`.

## Важно

- Это пакет документации для передачи DeepSeek через **Harness**. Это не инструкция для Cline и не набор `.clinerules`.
- Архитектура `0.1` заморожена. Изменение утверждённых решений требует согласования владельца проекта.
- `PROPOSED` не означает, что библиотека проверена или обязана быть использована.
- TASK_001 подтверждена только в Linux x86_64; Windows, macOS, FreeBSD, Android и iOS остаются `NOT_TESTED_IN_ENV`.
- GNS остаётся `PROPOSED` до portability smoke test, который обязателен до начала TASK_004.
- Не заявлять `PASS` без фактического запуска на соответствующей платформе.
- TASK_002 создаёт world/map/track data и geometry. Она ещё не создаёт playable racing game; первый управляемый автомобиль ожидается в TASK_003.
- Существующие legacy исходники и assets сохраняются; перенос выполняется только через явные задачи и mapping.
