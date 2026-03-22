# src/mdmtsp_minmax/solvers

Шаблонные solver'ы для Min-Max Multi-Depot mTSP:
- `greedy_seed.cpp` — жадное распределение клиентов по маршрутам с минимумом текущего max.
- `vns_anytime.cpp` — random-template (100+ итераций случайных раскладок, выбор лучшей).
- `relocate_local_search.cpp` — зарезервировано под локальный поиск.
