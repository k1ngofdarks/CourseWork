from __future__ import annotations

import argparse
import json
import logging
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

from python.cpp_updater import recompiles_if_necessary
from python.validate import validate_mdmtsp_routes, validate_tour


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Solve TSP / MDMTSP(min-max) from TXT or JSON task.")
    p.add_argument("--task", type=str, required=True, help="Path to task txt/json.")
    p.add_argument("--coords", type=str, default="World_TSP.npz", help="Default NPZ path for txt task mode.")
    p.add_argument("--log_mode", type=str, default="info", choices=["info", "debug"], help="Logger mode for C++ solver.")
    p.add_argument("--log_interval", type=int, default=5, help="Periodic logger flush interval (seconds).")
    return p.parse_known_args()


def read_task(task_path: Path) -> Tuple[int, List[int]]:
    lines = task_path.read_text(encoding="utf-8").strip().splitlines()
    n_nodes = int(lines[0].strip())
    ids = [int(x) for x in lines[1].strip().split()]
    if len(ids) != n_nodes:
        raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")
    return n_nodes, ids


def load_coords(coords_npz: Path) -> Dict[int, Tuple[float, float]]:
    data = np.load(coords_npz)["data"]
    return {int(row[0]): (float(row[1]), float(row[2])) for row in data}


def latlon_for_selected(ids: List[int], id_to_coord: Dict[int, Tuple[float, float]]) -> np.ndarray:
    return np.asarray([id_to_coord[sid] for sid in ids], dtype=np.float64)


def save_solution(task_path: Path, route_ids: List[int]) -> Path:
    out_path = task_path.with_name(task_path.stem + "_solution.txt")
    out_path.write_text(" ".join(str(x) for x in route_ids), encoding="utf-8")
    return out_path


def save_solution_json(task_path: Path, payload: Dict[str, Any]) -> Path:
    out_path = task_path.with_name(task_path.stem + "_solution.json")
    out_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return out_path


def detect_algorithm_name(cpp_args: List[str]) -> str:
    steps: List[str] = []
    for i, arg in enumerate(cpp_args):
        if arg == "--step" and i + 1 < len(cpp_args):
            steps.append(cpp_args[i + 1])
    if not steps:
        return "unknown"
    return " -> ".join(steps)


def _parse_json_npz_mode(task_json: Dict[str, Any], base_dir: Path, fallback_coords: Path) -> Tuple[str, int, List[int], str, List[int]]:
    ids = task_json["ids"]
    n_nodes = int(task_json.get("n", len(ids)))
    if len(ids) != n_nodes:
        raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")

    coords_arg = task_json.get("coords", str(fallback_coords))
    coords_path = Path(coords_arg)
    if not coords_path.is_absolute():
        coords_path = base_dir / coords_path
    id_to_coord = load_coords(coords_path)
    latlon = latlon_for_selected(ids, id_to_coord)

    problem = task_json.get("problem", "tsp")
    depots_idx: List[int] = []

    if problem == "mdmtsp_minmax":
        depots = task_json.get("depots", [])
        if not depots and "num_depots" in task_json:
            num_depots = task_json["num_depots"]
            depots_idx = np.random.choice(n_nodes, num_depots, replace=False).tolist()
        else:
            id_to_pos = {v: i for i, v in enumerate(ids)}
            depots_idx = [id_to_pos[d] for d in depots]

        payload_obj = {"n": n_nodes, "latlon": latlon.T.tolist(), "depots": depots_idx}
        return json.dumps(payload_obj), n_nodes, ids, problem, depots_idx

    payload_obj = {"n": n_nodes, "latlon": latlon.T.tolist()}
    return json.dumps(payload_obj), n_nodes, ids, problem, []


def build_payload_from_txt(task_path: Path, coords_path: Path) -> Tuple[str, int, List[int], str, List[int]]:
    n_nodes, ids = read_task(task_path)
    id_to_coord = load_coords(coords_path)
    latlon = latlon_for_selected(ids, id_to_coord)
    payload_obj = {"n": n_nodes, "latlon": latlon.T.tolist()}
    return json.dumps(payload_obj), n_nodes, ids, "tsp", []


def build_payload_from_json(task_json: Dict[str, Any], task_path: Path, fallback_coords: Path) -> Tuple[str, int, List[int], str, List[int]]:
    problem = task_json.get("problem", "tsp")

    # 1. Если задано количество случайных точек
    if "num_customers" in task_json:
        n_nodes = task_json["num_customers"]
        ids = task_json.get("ids", list(range(n_nodes)))
        if len(ids) != n_nodes:
            raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")

        # Генерируем случайные координаты [0, 1]^2
        coords = np.random.rand(n_nodes, 2).tolist()
        payload_obj = {
            "coordinates": coords,
            "metric": task_json.get("metric", "euclidean"),
        }

    # 2. Если задана матрица расстояний
    elif "matrix" in task_json:
        n_nodes = len(task_json["matrix"])
        ids = task_json.get("ids", list(range(n_nodes)))
        if len(ids) != n_nodes:
            raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")
        payload_obj = {"matrix": task_json["matrix"]}

    # 3. Если жестко заданы координаты
    elif "coordinates" in task_json:
        coordinates = task_json["coordinates"]
        n_nodes = len(coordinates)
        ids = task_json.get("ids", list(range(n_nodes)))
        if len(ids) != n_nodes:
            raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")
        payload_obj = {
            "coordinates": coordinates,
            "metric": task_json.get("metric", "euclidean"),
        }

    # 4. Режим NPZ (через IDs)
    elif "ids" in task_json:
        return _parse_json_npz_mode(task_json, task_path.parent, fallback_coords)
    else:
        raise ValueError("JSON task must contain one of: num_customers, matrix, coordinates, or ids.")

    # Обработка депо для MDMTSP
    depots_idx: List[int] = []
    if problem == "mdmtsp_minmax":
        depots = task_json.get("depots", [])
        # Если депо не заданы жестко, но задано их количество - генерим случайные индексы
        if not depots and "num_depots" in task_json:
            num_depots = task_json["num_depots"]
            depots_idx = np.random.choice(n_nodes, num_depots, replace=False).tolist()
        else:
            depots_idx = depots
        payload_obj["depots"] = depots_idx

    return json.dumps(payload_obj), n_nodes, ids, problem, depots_idx


def ensure_problem_arg(cpp_args: List[str], problem: str) -> List[str]:
    for i, arg in enumerate(cpp_args[:-1]):
        if arg == "--problem":
            cpp_args[i + 1] = problem
            return cpp_args
    return ["--problem", problem] + cpp_args


def ensure_key_arg(cpp_args: List[str], key: str, value: str) -> List[str]:
    flag = f"--{key}"
    for i, arg in enumerate(cpp_args[:-1]):
        if arg == flag:
            cpp_args[i + 1] = value
            return cpp_args
    return [flag, value] + cpp_args


def configure_python_logger(log_mode: str) -> None:
    level = logging.DEBUG if log_mode == "debug" else logging.INFO
    logging.basicConfig(level=level, format="%(message)s")


def summarize_route(route_ids: List[int], limit: int = 10) -> str:
    preview = route_ids[:limit]
    suffix = " ..." if len(route_ids) > limit else ""
    return f"{preview}{suffix}"


def summarize_routes(routes_ids: List[List[int]], limit_routes: int = 3, limit_nodes: int = 6) -> str:
    chunks = []
    for idx, route in enumerate(routes_ids[:limit_routes], start=1):
        preview = route[:limit_nodes]
        suffix = " ..." if len(route) > limit_nodes else ""
        chunks.append(f"r{idx}={preview}{suffix}")
    if len(routes_ids) > limit_routes:
        chunks.append("...")
    return " | ".join(chunks)


def save_log_payloads(task_path: Path, problem: str, payload: Dict[str, Any]) -> None:
    logs_dir = Path(__file__).resolve().parent / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)
    prefix = f"{problem}_{task_path.stem}"
    (logs_dir / f"{prefix}_best_solution_payload.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    if problem == "mdmtsp_minmax" and "routes" in payload:
        (logs_dir / f"{prefix}_best_routes.json").write_text(
            json.dumps(payload["routes"], ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
    if problem == "tsp" and "optimal_route" in payload:
        (logs_dir / f"{prefix}_best_route.json").write_text(
            json.dumps(payload["optimal_route"], ensure_ascii=False, indent=2),
            encoding="utf-8",
        )


def main() -> None:
    args, cpp_args = parse_args()
    configure_python_logger(args.log_mode)

    task_path = Path(args.task)
    coords_npz = Path(args.coords)
    num_runs = 1
    task_json = None

    if task_path.suffix.lower() == ".json":
        task_json = json.loads(task_path.read_text(encoding="utf-8"))
        num_runs = task_json.get("num_runs", 1)

    cpp_args_prepared = False
    algorithm = "unknown"

    total_cost = 0.0
    total_time = 0.0
    best_cost = float('inf')
    best_solution_payload = {}
    best_route_ids = []

    logging.info(
        "Start run | task=%s | log_mode=%s",
        task_path,
        args.log_mode,
    )
    logging.debug("Initial CLI args for solver: %s", cpp_args)

    for run_idx in range(num_runs):
        if task_json is not None:
            # Парсинг/Генерация вызывается каждую итерацию для обеспечения уникальных данных (координаты/депо)
            payload, n_nodes, ids, problem, depots = build_payload_from_json(task_json, task_path, coords_npz)
        else:
            payload, n_nodes, ids, problem, depots = build_payload_from_txt(task_path, coords_npz)

        if not cpp_args_prepared:
            cpp_args = ensure_problem_arg(cpp_args, problem)
            cpp_args = ensure_key_arg(cpp_args, "task_name", task_path.stem)
            cpp_args = ensure_key_arg(cpp_args, "log_mode", args.log_mode)
            cpp_args = ensure_key_arg(cpp_args, "log_interval", str(max(1, args.log_interval)))
            recompiles_if_necessary()
            algorithm = detect_algorithm_name(cpp_args)
            cpp_args_prepared = True
            logging.info(
                "Prepared solver | problem=%s | algorithm=%s | runs=%d",
                problem,
                algorithm,
                num_runs,
            )
            logging.debug("Prepared solver args: %s", cpp_args)

        logging.debug(
            "Run %d/%d | nodes=%d | depots=%s",
            run_idx + 1,
            num_runs,
            n_nodes,
            depots if depots else [],
        )

        p = subprocess.run(["build/src/tsp"] + cpp_args, input=payload, text=True, capture_output=True)
        if p.returncode != 0:
            raise RuntimeError(p.stderr)

        output = json.loads(p.stdout)

        if problem == "mdmtsp_minmax":
            routes_pos = output["routes"]
            real_time = output["time"]
            max_len = output["max_len"]
            lens = output.get("lens", [])

            ok, msg = validate_mdmtsp_routes(routes_pos, n_nodes, depots)
            routes_ids = [[ids[v] for v in route] for route in routes_pos]

            cost = max_len
            total_cost += cost
            total_time += real_time

            logging.info(f"Run {run_idx+1}/{num_runs} | Valid: {ok} | Cost (Max len): {cost:.6f} | Time: {real_time:.4f} s")
            logging.debug("Run %d validation message: %s", run_idx + 1, msg)
            logging.debug("Run %d routes preview: %s", run_idx + 1, summarize_routes(routes_ids))

            if cost < best_cost:
                best_cost = cost
                best_solution_payload = {
                    "problem": "mdmtsp_minmax",
                    "algorithm": algorithm,
                    "best_max_cost": max_len,
                    "route_costs": lens,
                    "routes": routes_ids,
                    "depots": depots,
                    "best_run_idx": run_idx + 1
                }
                logging.info(
                    "New best solution | run=%d | max_len=%.6f | route_costs=%s",
                    run_idx + 1,
                    max_len,
                    lens,
                )
                logging.debug("Best routes preview: %s", summarize_routes(routes_ids))
        else:
            route_pos = output["route"]
            real_time = output["time"]
            length_km = output["len"]

            ok, msg = validate_tour(route_pos, n_nodes)
            route_ids = [ids[i] for i in route_pos]

            cost = length_km
            total_cost += cost
            total_time += real_time

            logging.info(f"Run {run_idx+1}/{num_runs} | Valid: {ok} | Cost: {cost:.6f} | Time: {real_time:.4f} s")
            logging.debug("Run %d validation message: %s", run_idx + 1, msg)
            logging.debug("Run %d route preview: %s", run_idx + 1, summarize_route(route_ids))

            if cost < best_cost:
                best_cost = cost
                best_route_ids = route_ids
                best_solution_payload = {
                    "problem": "tsp",
                    "algorithm": algorithm,
                    "best_cost": length_km,
                    "optimal_route": route_ids,
                    "best_run_idx": run_idx + 1
                }
                logging.info(
                    "New best solution | run=%d | cost=%.6f",
                    run_idx + 1,
                    length_km,
                )
                logging.debug("Best route preview: %s", summarize_route(route_ids))

    # Вычисляем среднее и сохраняем результаты по завершении всех запусков
    avg_cost = total_cost / num_runs
    avg_time = total_time / num_runs

    best_solution_payload["avg_cost"] = avg_cost
    best_solution_payload["avg_time"] = avg_time
    best_solution_payload["num_runs"] = num_runs

    out_json_path = save_solution_json(task_path, best_solution_payload)
    save_log_payloads(task_path, best_solution_payload["problem"], best_solution_payload)

    logging.info("-" * 40)
    logging.info(f"Total Runs: {num_runs}")
    logging.info(f"Average Cost: {avg_cost:.6f} | Average Time: {avg_time:.4f} s")
    logging.info(f"Best Cost: {best_cost:.6f} (found at run {best_solution_payload.get('best_run_idx')})")
    logging.info(f"Solution JSON saved: {out_json_path}")
    logging.debug("Best solution payload saved to logs directory")

    if best_solution_payload.get("problem") != "mdmtsp_minmax":
        #out_path = save_solution(task_path, best_route_ids) ignore TXT
        #logging.info(f"Solution TXT saved: {out_path}")
        logging.info(f"Best Route (first 25 ids): {best_route_ids[:25]} ...")
    else:
        logging.info(
            "Best routes summary: %s",
            summarize_routes(best_solution_payload.get("routes", [])),
        )


if __name__ == "__main__":
    main()
