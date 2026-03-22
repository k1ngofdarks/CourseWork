from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np


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


def build_tsp_payload(task_path: Path, coords_npz: Path, metric: str) -> Tuple[dict, List[int]]:
    if task_path.suffix.lower() == ".json":
        task = json.loads(task_path.read_text(encoding="utf-8"))
        if task.get("problem", "tsp") != "tsp":
            raise ValueError("This runner executes only tsp problem type")
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


def build_mdmtsp_payload(task_path: Path) -> dict:
    if task_path.suffix.lower() != ".json":
        raise ValueError("mdmtsp_minmax runner expects JSON task format")
    task = json.loads(task_path.read_text(encoding="utf-8"))
    if task.get("problem") != "mdmtsp_minmax":
        raise ValueError("JSON task must have problem=mdmtsp_minmax")
    if "depots" not in task or "k_vehicles" not in task:
        raise ValueError("mdmtsp_minmax task requires depots and k_vehicles")
    return task
