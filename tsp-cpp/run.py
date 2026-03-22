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
    p = argparse.ArgumentParser(description="Solve Metric TSP from TXT or JSON task.")
    p.add_argument("--task", type=str, required=True, help="Path to task txt/json.")
    p.add_argument("--coords", type=str, default="World_TSP.npz", help="Default NPZ path for txt task mode.")
    return p.parse_known_args()


def read_task(task_path: Path) -> Tuple[int, List[int]]:
    lines = task_path.read_text(encoding="utf-8").strip().splitlines()
    n_nodes = int(lines[0].strip())
    ids = [int(x) for x in lines[1].strip().split()]
    if len(ids) != n_nodes:
        raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")
    return n_nodes, ids


def load_coords(coords_npz: Path) -> Dict[int, Tuple[float, float]]:
    data = np.load(coords_npz)["data"]  # (N,3): [idx, lat, lon]
    return {int(row[0]): (float(row[1]), float(row[2])) for row in data}


def latlon_for_selected(ids: List[int], id_to_coord: Dict[int, Tuple[float, float]]) -> np.ndarray:
    return np.asarray([id_to_coord[sid] for sid in ids], dtype=np.float64)


def save_solution(task_path: Path, route_ids: List[int]) -> Path:
    out_path = task_path.with_name(task_path.stem + "_solution.txt")
    out_path.write_text(" ".join(str(x) for x in route_ids), encoding="utf-8")
    return out_path


def save_solution_json(
    task_path: Path,
    algorithm: str,
    cost: float,
    route_ids: List[int],
    real_time: float,
) -> Path:
    out_path = task_path.with_name(task_path.stem + "_solution.json")
    out_payload = {
        "algorithm": algorithm,
        "cost": cost,
        "optimal_route": route_ids,
        "time": real_time,
    }
    out_path.write_text(
        json.dumps(out_payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return out_path


def detect_algorithm_name(cpp_args: List[str]) -> str:
    steps: List[str] = []
    for i, arg in enumerate(cpp_args):
        if arg == "--step" and i + 1 < len(cpp_args):
            steps.append(cpp_args[i + 1])
    if not steps:
        return "unknown"
    return " -> ".join(steps)


def build_payload_from_txt(task_path: Path, coords_path: Path) -> Tuple[str, int, List[int]]:
    n_nodes, ids = read_task(task_path)
    id_to_coord = load_coords(coords_path)
    latlon = latlon_for_selected(ids, id_to_coord)
    payload_obj = {"n": n_nodes, "latlon": latlon.T.tolist()}
    return json.dumps(payload_obj), n_nodes, ids


def _parse_json_npz_mode(task_json: Dict[str, Any], base_dir: Path, fallback_coords: Path) -> Tuple[str, int, List[int]]:
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
    payload_obj = {"n": n_nodes, "latlon": latlon.T.tolist()}
    return json.dumps(payload_obj), n_nodes, ids


def build_payload_from_json(task_path: Path, fallback_coords: Path) -> Tuple[str, int, List[int]]:
    task_json = json.loads(task_path.read_text(encoding="utf-8"))

    if "matrix" in task_json:
        n_nodes = len(task_json["matrix"])
        ids = list(range(n_nodes))
        payload_obj = {"matrix": task_json["matrix"]}
        return json.dumps(payload_obj), n_nodes, ids

    if "coordinates" in task_json:
        coordinates = task_json["coordinates"]
        n_nodes = len(coordinates)
        ids = task_json.get("ids", list(range(n_nodes)))
        if len(ids) != n_nodes:
            raise ValueError(f"ids length {len(ids)} does not match n_nodes {n_nodes}")
        payload_obj = {
            "coordinates": coordinates,
            "metric": task_json.get("metric", "euclidean"),
        }
        return json.dumps(payload_obj), n_nodes, ids

    if "ids" in task_json:
        return _parse_json_npz_mode(task_json, task_path.parent, fallback_coords)

    raise ValueError(
        "JSON task must contain one of: matrix, coordinates, or ids (for NPZ mode)."
    )


def main() -> None:
    args, cpp_args = parse_args()
    logging.basicConfig(level=logging.INFO, format="%(message)s")

    task_path = Path(args.task)
    coords_npz = Path(args.coords)

    if task_path.suffix.lower() == ".json":
        payload, n_nodes, ids = build_payload_from_json(task_path, coords_npz)
    else:
        payload, n_nodes, ids = build_payload_from_txt(task_path, coords_npz)

    recompiles_if_necessary()

    p = subprocess.run(["build/src/tsp"] + cpp_args, input=payload, text=True, capture_output=True)

    if p.returncode != 0:
        raise RuntimeError(p.stderr)
    output = json.loads(p.stdout)
    route_pos = output["route"]
    real_time = output["time"]
    length_km = output["len"]

    ok, msg = validate_tour(route_pos, n_nodes)

    # Map back to dataset IDs
    route_ids = [ids[i] for i in route_pos]
    out_path = save_solution(task_path, route_ids)
    out_json_path = save_solution_json(
        task_path=task_path,
        algorithm=detect_algorithm_name(cpp_args),
        cost=length_km,
        route_ids=route_ids,
        real_time=real_time,
    )

    logging.info(f"Valid: {ok} ({msg}) | Length: {length_km:.6f} km | Time: {real_time:.4f} s")
    logging.info(f"Solution saved: {out_path}")
    logging.info(f"Solution JSON saved: {out_json_path}")
    logging.info(f"Route (first 25 ids): {route_ids[:25]} ...")


if __name__ == "__main__":
    main()
