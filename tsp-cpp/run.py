from __future__ import annotations

import argparse
import logging
import json
import subprocess
from pathlib import Path
from typing import List, Tuple, Dict

import numpy as np

from python.validate import validate_tour
from python.cpp_updater import recompiles_if_necessary

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Solve Metric TSP from a task file (n_nodes + ids).")
    p.add_argument("--task", type=str, required=True, help="Path to task txt (line1: n_nodes, line2: ids).")
    p.add_argument("--coords", type=str, default="World_TSP.npz", help="Path to coordinates NPZ [idx, lat, lon].")
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


def main() -> None:
    args, cpp_args = parse_args()
    logging.basicConfig(level=logging.INFO, format="%(message)s")

    task_path = Path(args.task)
    coords_npz = Path(args.coords)

    n_nodes, ids = read_task(task_path)
    id_to_coord = load_coords(coords_npz)
    latlon = latlon_for_selected(ids, id_to_coord)
    payload = json.dumps({"n": n_nodes, "latlon": latlon.T.tolist()})

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

    logging.info(f"Valid: {ok} ({msg}) | Length: {length_km:.6f} km | Time: {real_time:.4f} s")
    logging.info(f"Solution saved: {out_path}")
    logging.info(f"Route (first 25 ids): {route_ids[:25]} ...")


if __name__ == "__main__":
    main()


