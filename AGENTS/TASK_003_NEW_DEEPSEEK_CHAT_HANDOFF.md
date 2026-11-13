# Новый чат DeepSeek — передача TASK_003

Скопировать этот текст первым сообщением нового чата DeepSeek после установки пакета в проект.

---

Продолжаем разработку RustyRage. Это новый рабочий чат; не полагайся на старую переписку как на источник требований.

Сначала прочитай полностью:

1. `TASKS/draft/TASK_003_FIRST_PLAYABLE.md`
2. `TASKS/draft/TASK_003_OWNER_DECISIONS.md`
3. `AGENTS/TASK_003_EXECUTOR_DEEPSEEK_HARNESS.md`
4. `AGENTS/TASK_003_GATE_REPORT_TEMPLATE.md`

Затем прочитай `TASK_002` итоговые артефакты и реальный checkout настолько, насколько доступно, но не изменяй их.

**Текущее разрешение:** только inspection и подготовка отчёта. Реализация НЕ разрешена. Не создавай `TASKS/active/TASK_003_FIRST_PLAYABLE.md`, не изменяй исходный код, CMake, tests или assets. Не изменяй TASK_002.

Подготовь один baseline report:

- Git commit и состояние рабочего дерева;
- подтверждение owner acceptance TASK_002 либо статус `UNCONFIRMED`;
- реальная структура targets и dependency graph;
- фактические публичные API `rr_world`, `rr_physics_api`, `rr_sim`, renderer и platform input;
- существующие tests и доступные build presets;
- точная сверка TASK_003 draft с реальным checkout;
- несовпадения, технические риски и неизвестные API;
- список owner decisions, которые ещё не утверждены;
- рекомендация о готовности к активации, без самостоятельной активации.

Не исправляй draft по своему усмотрению. Если обнаружено противоречие — опиши его и дождись решения владельца.

После baseline report остановись. Следующая команда должна прийти от владельца.
