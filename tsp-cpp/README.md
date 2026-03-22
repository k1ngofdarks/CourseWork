Runner для TSP и Min-Max Multi-Depot mTSP
==============

## Структура (header рядом с C++)

```text
tsp-cpp/
  include/
    json.hpp

  src/
    main.cpp

    tsp/
      instance.h / instance.cpp
      solver.h
      factory.h
      progress.h
      stop_condition.h / stop_condition.cpp
      best_store.h
      tsp_runner.h / tsp_runner.cpp
      solvers/
        nearest_neighbour.cpp
        two_opt.cpp
        ils.cpp
        gls.cpp
        lkh.cpp

    mdmtsp_minmax/
      mdmtsp_instance.h / mdmtsp_instance.cpp
      mdmtsp_solution.h
      mdmtsp_solver_api.h
      mdmtsp_objective.h
      mdmtsp_runner.h / mdmtsp_runner.cpp
      solvers/
        greedy_seed.cpp
        vns_anytime.cpp
        relocate_local_search.cpp

  python/
    run.py
    io/formats.py
    runners/tsp_runner.py
    runners/mdmtsp_runner.py
```

## TSP anytime / checkpoint

Поддерживаются:
- `--run-time-limit <sec>`
- `--history-file <path.jsonl>`
- `--checkpoint-file <path.json>`
- `--checkpoint-every-sec <sec>`

Пример:
```bash
python3 run.py \
  --problem tsp \
  --task tasks/task_001.txt \
  --step ils --time 300 \
  --run-time-limit 300 \
  --history-file artifacts/tsp_history.jsonl \
  --checkpoint-file artifacts/tsp_best.json
```

## Форматы TSP

### TXT (обратная совместимость)
- строка 1: `n_nodes`
- строка 2: ids узлов
- координаты берутся из `--coords`

### JSON coords
```json
{
  "problem": "tsp",
  "format": "coords",
  "metric": "euclidean",
  "coords": [[x1, x2, ...], [y1, y2, ...]]
}
```

### JSON matrix
```json
{
  "problem": "tsp",
  "format": "matrix",
  "matrix": [[0, 1.2], [1.2, 0]]
}
```

## mdmtsp_minmax

Поддержаны baseline solver'ы:
- `--step greedy_seed`
- `--step random --iters 100 --seed 42`

### Про депо и агентов
- `depots`: массив depot-вершин (k депо)
- `depot_vehicle_limits`: классический вариант `m_i` агентов для каждого депо
- `k_vehicles`: суммарное число агентов (если `depot_vehicle_limits` не задан)

`depot_vehicle_limits` имеет приоритет. Если задан только `k_vehicles`, выполняется round-robin распределение по депо.

### JSON пример на 5 вершинах и 2 депо
Файл: `tasks/md_example_5_nodes.json`

```json
{
  "problem": "mdmtsp_minmax",
  "format": "matrix",
  "matrix": [
    [0, 7, 3, 8, 6],
    [7, 0, 6, 4, 5],
    [3, 6, 0, 5, 7],
    [8, 4, 5, 0, 3],
    [6, 5, 7, 3, 0]
  ],
  "depots": [0, 1],
  "depot_vehicle_limits": [2, 1]
}
```

Запуск:
```bash
python3 run.py --problem mdmtsp_minmax --task tasks/md_example_5_nodes.json --step greedy_seed
python3 run.py --problem mdmtsp_minmax --task tasks/md_example_5_nodes.json --step random --iters 100
```
