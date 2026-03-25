Runner для тестирования cpp солверов
==============

### Структура проекта

- Каталог `python` отвечает за скрипты на языке `python`
- Каталоги `icnlude` и `src` за header и source файлы
- Подкаталог `src/solvers` отвечает за все кастомные солверы: `src/solvers/tsp` и `src/solvers/mdmtsp_minmax`

### Создание нового солвера

- Новый солвер следует разместить в папке `src/solvers`
- Солвер должен являться наследником класса `Solver`, он должен переопределить методы `Solve` и `Configure`
- Метод `Configure` позволяет настраивать параметры солвера через переданные аргументы командной строки
- Для доступа к графу следует использовать интерфейс синглтона `Instance`
- Для использования солвера следует зарегистрировать его в классе `SolverFactory` через `SolverFactory::RegisterSolver`, далее можно использовать через `SolverFactory::Create` или же создавая экземпляры напрямую (для большей гибкости)
- Возможно последовательное применение нескольких солверов. В этом случае результат одного солвера будет инпутом для следующего. Для этого нужно указать несколько аргументов `--step` и соответствующие аргументы далее, которые будут переданы в `Configure`

### Компиляция
- Проект поддерживает автоматическую компиляцию при обнаружении изменения любого соответствующего файла
- Проект создает каталог `build` и считает `build/src/tsp` исполняемым файлом по умолчанию

### Запуск солвера
Запуск следует производить, находясь в папке tsp-cpp.

Поддерживаются два формата задач:
- `.txt` — старый формат (n + список id), координаты берутся из `--coords`.
- `.json` — новый формат с тремя режимами:
  - через `ids` + `coords` (`.npz`, как раньше),
  - через готовую матрицу расстояний `matrix`,
  - через `coordinates` и метрику `metric` (`euclidean` или `sphere`).

```bash
python3 run.py --task path/to/task --coords path/to/dataset --step solver1_name [--arg1 val1 --arg2 val2 ...] --step solver2_name [...]
```

Например: 
```bash
python3 run.py --task tasks/task_001.txt --coords World_TSP.npz --step nearest --start 10
```

Примеры JSON:

```json
{
  "ids": [1088888, 78098, 73387],
  "coords": "World_TSP.npz"
}
```

```json
{
  "matrix": [
    [0.0, 4.0, 7.0],
    [4.0, 0.0, 2.0],
    [7.0, 2.0, 0.0]
  ]
}
```

```json
{
  "coordinates": [[55.75, 37.61], [59.93, 30.31], [56.84, 60.61]],
  "metric": "sphere"
}
```

### Вывод:
- Проверяет структуру (корректность гамильтонова цикла)
- Выводит длину (км), время работы алгоритма (сек) и первые узлы маршрута
- Сохраняет решение в tasks/{task_filename}_solution.txt
- Дополнительно сохраняет JSON в `tasks/{task_filename}_solution.json` c полями:
  - `algorithm` — имя (или цепочка) солвера из `--step`
  - `cost` — длина маршрута
  - `optimal_route` — найденный маршрут
  - `time` — время работы


### LKH Lib

Использование: 
```bash
python3 run.py --task tasks/task_001.txt --coords World_TSP.npz --step lkh --time_limit 10
```

`TODO: fix in task_003` 

### Логирование (INFO/DEBUG)

- Логгер встроен в C++ раннер как singleton и поддерживает режимы:
  - `info` — только базовые события и метрики,
  - `debug` — дополнительно debug-сообщения.
- Логгер принимает каждое новое решение через `AddNewSolution` (решение может быть не оптимальным), хранит историю улучшений и текущее лучшее значение целевой функции.
- Маршрут в лог не пишется (только метрики).
- Каждые `N` секунд пишется snapshot в файл `logs/{task_type}_{task_name}.log`.

Параметры:
```bash
python3 run.py --task ... --log_mode info --log_interval 5 --step nearest
python3 run.py --task ... --log_mode debug --log_interval 2 --step random --iter 1000
```

#### API логгера (C++)
- `Configure(task_type, task_name, mode, flush_interval_sec)` — инициализирует логгер и запускает периодическую запись.
- `AddInfo(message)` — пишет событие уровня INFO.
- `AddDebug(message)` — пишет событие DEBUG (только в режиме `debug`).
- `AddNewSolution(source, objective_value)` — регистрирует очередное решение, обновляет текущее лучшее и историю улучшений.
- `Shutdown()` — завершает фонового воркера и делает финальный flush.

#### Что логируют алгоритмы
- `ils` (TSP): INFO при улучшении текущего best, DEBUG по итерациям (каждые 50 шагов).
- `random` (MDMTSP Min-Max): INFO при улучшении текущего `best_max`, DEBUG по итерациям (каждые 100 шагов).

#### Пример вида лога
```text
===== SNAPSHOT 2026-03-25 22:30:00 =====
best_objective=1234.56
improvements_count=3
  * 2026-03-25 22:29:58 | tsp_step_1 | 1450.1
  * 2026-03-25 22:29:59 | ils | 1300.4
  * 2026-03-25 22:30:00 | ils | 1234.56
events:
  [INFO] 2026-03-25 22:29:58 (+0.001s) | Logger started for tsp/task_001 (interval=2s)
  [INFO] 2026-03-25 22:29:59 (+1.233s) | [ils] improved at iter=12, best=1300.400000
  [DEBUG] 2026-03-25 22:30:00 (+2.104s) | [ils] iter=50, candidate=1310.200000, best=1300.400000
```


### MDMTSP Min-Max

Для запуска режима MDMTSP Min-Max в JSON задаче нужно указать:
- `problem: "mdmtsp_minmax"`
- `depots`: список депо (индексы вершин, допустимы повторы, длина списка = число маршрутов)

Поддерживаются те же 3 формата входа (`ids`+`coords`, `matrix`, `coordinates`+`metric`).

Пример:

```json
{
  "problem": "mdmtsp_minmax",
  "coordinates": [[0,0],[2,0],[4,0],[0,2],[2,2],[4,2]],
  "metric": "euclidean",
  "depots": [0, 3, 3]
}
```

Запуск random-солвера:

```bash
python3 run.py --task tasks/mdmtsp_minmax/example1.json --step random --iter 1000
```

Поля выходного JSON для MDMTSP:
- `problem` = `mdmtsp_minmax`
- `algorithm`
- `max_cost` — минимизированный максимум по длинам маршрутов
- `route_costs` — длина каждого маршрута
- `routes` — список маршрутов (в id исходных данных)
- `time`
