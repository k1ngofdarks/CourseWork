# include/mdmtsp_minmax

Типы и API для Min-Max Multi-Depot mTSP:
- `mdmtsp_instance.h` — входная задача и расстояния, включая `depots`, `k_vehicles` и `depot_vehicle_limits` (`m_i`).
- `mdmtsp_solution.h` — набор маршрутов по depot.
- `mdmtsp_objective.h` — целевая функция max длины маршрута.
- `mdmtsp_solver_api.h` — сигнатуры baseline solver'ов (`greedy_seed`, `random_template`).
- `mdmtsp_runner.h` — entrypoint problem runner.
