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

### Логирование и graceful stop (TSP + MDMTSP)

Поддерживаются глобальные опции логирования (применяются ко всем `--step`):

- `--log_file <path>` — текстовый лог (`INFO`/`DEBUG`)
- `--csv_file <path>` — CSV лог событий (`solver,event,best_length,seconds,route`)
- `--log_interval <seconds>` — запись текущего лучшего решения каждые N секунд
- `--debug <true|false|1|0>` — включить debug-логи
- `--console_log <true|false|1|0>` — вывод логов в консоль (`stderr`), по умолчанию `false`
- `--stop_file <path>` — graceful stop: при появлении файла алгоритмы завершают текущую итерацию и выходят

Пример:

```bash
python3 run.py \
  --task tasks/tsp/example1.json \
  --step nearest \
  --step 2-opt --time 30 \
  --log_file logs/tsp.log \
  --csv_file logs/tsp.csv \
  --log_interval 5 \
  --debug true \
  --console_log false \
  --stop_file /tmp/tsp.stop
```

Для MDMTSP Min-Max используются те же параметры:

```bash
python3 run.py \
  --task tasks/mdmtsp_minmax/example1.json \
  --step ant --n_iter 50 \
  --log_file logs/md.log \
  --csv_file logs/md.csv \
  --log_interval 3
```

### Если падает сборка LKH (`OBJ/*.o: No such file or directory`)

Перед сборкой можно подготовить папку объектных файлов и собрать LKH вручную:

```bash
cd LKH-2.0.11
mkdir -p SRC/OBJ
make all
```

Также CMake-таргет `extern_lib` теперь автоматически создает `LKH-2.0.11/SRC/OBJ` перед `make`.
