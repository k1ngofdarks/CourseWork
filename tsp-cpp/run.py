from __future__ import annotations

import argparse
import logging
import json
import subprocess
from pathlib import Path
from typing import List, Tuple, Dict, Any

import numpy as np

from python.validate import validate_tour
from python.cpp_updater import recompiles_if_necessary


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Solve routing tasks with cpp solvers.")
    p.add_argument("--task", type=str, required=True, help="Path to task txt/json.")
    p.add_argument("--coords", type=str, default="World_TSP.npz", help="Path to coordinates NPZ [idx, lat, lon].")
    p.add_argument("--metric", type=str, default="haversine", choices=["haversine", "euclidean"],
                   help="Metric for coordinate-based tasks.")
    p.add_argument("--checkpoint-file", type=str, default="", help="Path to best-so-far checkpoint JSON.")
    p.add_argument("--history-file", type=str, default="", help="Path to improvements history JSONL.")
    p.add_argument("--checkpoint-every-sec", type=float, default=30.0, help="Periodic checkpoint flush interval.")
    p.add_argument("--run-time-limit", type=float, default=-1, help="Global runtime limit in seconds.")
    return p.parse_known_args()


def read_task_txt(task_path: Path) -> Tuple[int, List[int]]:
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


def build_payload(task_path: Path, coords_npz: Path, metric: str) -> Tuple[dict, List[int]]:
    if task_path.suffix.lower() == ".json":
        task = json.loads(task_path.read_text(encoding="utf-8"))
        if task.get("problem", "tsp") != "tsp":
            raise ValueError("This runner currently executes only tsp problem type")
        fmt = task.get("format", "coords")
        if fmt == "matrix":
            matrix = task["matrix"]
            n = len(matrix)
            return {"n": n, "format": "matrix", "matrix": matrix}, list(range(n))
        coords = task.get("coords")
        if coords is None:
            raise ValueError("JSON task with format=coords must contain 'coords': [[x...],[y...]]")
        n = len(coords[0])
        return {"n": n, "format": "coords", "coords": coords, "metric": task.get("metric", metric)}, list(range(n))

    n_nodes, ids = read_task_txt(task_path)
    id_to_coord = load_coords(coords_npz)
    latlon = latlon_for_selected(ids, id_to_coord)
    return {"n": n_nodes, "format": "coords", "coords": latlon.T.tolist(), "metric": metric}, ids


def save_solution(task_path: Path, route_ids: List[int]) -> Path:
    out_path = task_path.with_name(task_path.stem + "_solution.txt")
    out_path.write_text(" ".join(str(x) for x in route_ids), encoding="utf-8")
    return out_path


def main() -> None:
    args, cpp_args = parse_args()
    logging.basicConfig(level=logging.INFO, format="%(message)s")

    task_path = Path(args.task)
    coords_npz = Path(args.coords)

    payload, ids = build_payload(task_path, coords_npz, args.metric)

    recompiles_if_necessary()

    if args.history_file:
        cpp_args += ["--log_history_file", args.history_file]
    if args.checkpoint_file:
        cpp_args += ["--checkpoint_file", args.checkpoint_file]
    cpp_args += ["--checkpoint_every_sec", str(args.checkpoint_every_sec)]
    if args.run_time_limit > 0:
        cpp_args += ["--run_time_limit", str(args.run_time_limit)]

    p = subprocess.run(["build/src/tsp"] + cpp_args, input=json.dumps(payload), text=True, capture_output=True)

    if p.returncode != 0:
        raise RuntimeError(p.stderr)

    output = json.loads(p.stdout)
    route_pos = output["route"]
    real_time = output["time"]
    length = output["len"]

    ok, msg = validate_tour(route_pos, payload["n"])

    route_ids = [ids[i] for i in route_pos]
    out_path = save_solution(task_path, route_ids)

    logging.info(f"Valid: {ok} ({msg}) | Length: {length:.6f} | Time: {real_time:.4f} s")
    logging.info(
        f"Best found at: {output.get('best_found_time', real_time):.4f}s | "
        f"Iter: {output.get('best_iter', 0)} | Solver: {output.get('best_solver', '')}"
    )
    if output.get("stopped_by_signal"):
        logging.info("Stopped by signal: best checkpoint should contain latest best-so-far.")
    logging.info(f"Solution saved: {out_path}")
    if args.history_file:
        logging.info(f"History file: {args.history_file}")
    if args.checkpoint_file:
        logging.info(f"Checkpoint file: {args.checkpoint_file}")


if __name__ == "__main__":
    main()
