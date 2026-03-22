Runner для тестирования cpp солверов
==============

### Структура проекта

- Каталог `python` отвечает за скрипты на языке `python`
- Каталоги `include` и `src` за header и source файлы
- Подкаталог `src/tsp/solvers` отвечает за все кастомные TSP солверы

### Создание нового солвера

- Новый солвер следует разместить в папке `src/tsp/solvers`
- Солвер должен являться наследником класса `Solver`, он должен переопределить методы `Solve` и `Configure`
- Метод `Configure` позволяет настраивать параметры солвера через переданные аргументы командной строки
- Для доступа к графу следует использовать интерфейс синглтона `Instance`
- Для использования солвера следует зарегистрировать его в классе `SolverFactory` через `SolverFactory::RegisterSolver`
- Возможно последовательное применение нескольких солверов через аргументы `--step`

### Запуск солвера

Запуск следует производить, находясь в папке tsp-cpp
```bash
python3 run.py --task path/to/task --coords path/to/dataset --step solver1_name [--arg1 val1] --step solver2_name [...]
```

### Anytime / best-so-far логирование

Поддерживаются:
- `--run-time-limit <sec>`: глобальный лимит времени для всего запуска
- `--history-file <path.jsonl>`: журнал улучшений (best-so-far history)
- `--checkpoint-file <path.json>`: периодический checkpoint лучшего решения
- `--checkpoint-every-sec <sec>`: периодичность сохранения checkpoint (по умолчанию 30)

Пример:
```bash
python3 run.py \
  --task tasks/task_001.txt \
  --step ils --time 300 \
  --run-time-limit 300 \
  --history-file artifacts/tsp_history.jsonl \
  --checkpoint-file artifacts/tsp_best.json \
  --checkpoint-every-sec 30
```

Если процесс остановить `SIGINT/SIGTERM`, раннер сохранит последнее best-so-far в checkpoint.

### Форматы задач TSP

#### 1) Текущий TXT формат (обратная совместимость)
- строка 1: `n_nodes`
- строка 2: список id узлов
- координаты берутся из NPZ (`--coords`)

#### 2) JSON coords формат
```json
{
  "problem": "tsp",
  "format": "coords",
  "metric": "haversine",
  "coords": [[x1, x2, ...], [y1, y2, ...]]
}
```
`metric`: `haversine` или `euclidean`.

#### 3) JSON matrix формат
```json
{
  "problem": "tsp",
  "format": "matrix",
  "matrix": [[0, 1.2, ...], [1.2, 0, ...], ...]
}
```

### Рекомендуемый формат для будущего mdmtsp_minmax

(Схема для унификации, пока без отдельного исполняемого mdmtsp раннера)
```json
{
  "problem": "mdmtsp_minmax",
  "format": "coords",
  "metric": "euclidean",
  "coords": [[x...], [y...]],
  "depots": [0, 7, 11],
  "k_vehicles": 6,
  "depot_vehicle_limits": {"0": 2, "7": 2, "11": 2}
}
```

### LKH Lib

Использование:
```bash
python3 run.py --task tasks/task_001.txt --coords World_TSP.npz --step lkh --time_limit 10
```
