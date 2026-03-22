# src/tsp

TSP runtime:
- `instance.cpp` — построение матрицы расстояний (coords/matrix; haversine/euclidean).
- `tsp_runner.cpp` — запуск цепочки solver'ов, anytime logging, checkpoints.
- `solvers/` — конкретные алгоритмы TSP.
