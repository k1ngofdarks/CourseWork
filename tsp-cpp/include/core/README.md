# include/core

Общие интерфейсы платформы:
- `solver_base.h` — базовый интерфейс solver.
- `factory.h` — реестр и создание solver'ов.
- `run_context.h` — callbacks и общие runtime-параметры.
- `stop_condition.h` — единый stop-condition (время/сигналы).
- `best_store.h` — best-so-far контейнер.
- `progress_event.h` — структура события прогресса.
