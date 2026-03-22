Runner для тестирования solvers (TSP + scaffold для mdmtsp_minmax)
==============

## Структура проекта

```text
tsp-cpp/
  include/
    core/
      run_context.h
      stop_condition.h
      progress_event.h
      best_store.h
      solver_base.h
      factory.h
    tsp/
      tsp_instance.h
      tsp_solution.h
      tsp_objective.h
      tsp_solver_api.h
      tsp_runner.h
    mdmtsp_minmax/
      mdmtsp_instance.h
      mdmtsp_solution.h
      mdmtsp_objective.h
      mdmtsp_solver_api.h
      mdmtsp_runner.h

  src/
    core/
      runner.cpp
      stop_condition.cpp
      progress_logger.cpp
      checkpoint_writer.cpp
      factory.cpp
      best_store.cpp

    tsp/
      tsp_runner.cpp
      instance.cpp
      solvers/
        nearest_neighbour.cpp
        two_opt.cpp
        ils.cpp
        gls.cpp
        lkh.cpp

    mdmtsp_minmax/
      mdmtsp_runner.cpp
      mdmtsp_instance.cpp
      solvers/
        greedy_seed.cpp
        relocate_local_search.cpp
        vns_anytime.cpp

    main.cpp

  python/
    run.py
    runners/
      tsp_runner.py
      mdmtsp_runner.py
    io/
      formats.py
```

## Anytime / best-so-far логирование

Поддерживаются:
- `--run-time-limit <sec>`: глобальный лимит времени для всего запуска
- `--history-file <path.jsonl>`: журнал улучшений (best-so-far history)
- `--checkpoint-file <path.json>`: периодический checkpoint лучшего решения
- `--checkpoint-every-sec <sec>`: периодичность сохранения checkpoint (по умолчанию 30)

Пример:
```bash
python3 run.py \
  --problem tsp \
  --task tasks/task_001.txt \
  --step ils --time 300 \
  --run-time-limit 300 \
  --history-file artifacts/tsp_history.jsonl \
  --checkpoint-file artifacts/tsp_best.json \
  --checkpoint-every-sec 30
```

Если процесс остановить `SIGINT/SIGTERM`, раннер сохраняет последнее best-so-far в checkpoint.

## Форматы задач TSP

### 1) TXT формат (обратная совместимость)
- строка 1: `n_nodes`
- строка 2: список id узлов
- координаты берутся из NPZ (`--coords`)

### 2) JSON coords формат
```json
{
  "problem": "tsp",
  "format": "coords",
  "metric": "haversine",
  "coords": [[x1, x2, ...], [y1, y2, ...]]
}
```
`metric`: `haversine` или `euclidean`.

### 3) JSON matrix формат
```json
{
  "problem": "tsp",
  "format": "matrix",
  "matrix": [[0, 1.2, ...], [1.2, 0, ...], ...]
}
```

## mdmtsp_minmax

Поддержаны два простых baseline solver'а:
- `greedy_seed`
- `random` (template: случайные раскладки, по умолчанию 100 итераций)

### Что такое `k_vehicles` и `depot_vehicle_limits`
- `depots` — массив depot-вершин (их может быть k штук).
- `k_vehicles` — **общее** число агентов (суммарно по всем depot).
- `depot_vehicle_limits` — явное распределение агентов по depot (классический вариант с `m_i`).

Если задан только `k_vehicles`, раннер распределяет агентов по depot равномерно round-robin.
Если задан `depot_vehicle_limits`, он имеет приоритет и задает точные `m_i` по depot.

### JSON формат mdmtsp_minmax

```json
{
  "problem": "mdmtsp_minmax",
  "format": "matrix",
  "matrix": [[0, 10, 4], [10, 0, 7], [4, 7, 0]],
  "depots": [0],
  "k_vehicles": 2,
  "depot_vehicle_limits": [1]
}
```

Поддерживаемые шаги для `--problem mdmtsp_minmax`:
- `--step greedy_seed`
- `--step random --iters 100 --seed 42`

Пример запуска random template:
```bash
python3 run.py --problem mdmtsp_minmax --task tasks/md_example.json --step random --iters 100
```
