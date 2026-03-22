from __future__ import annotations

import json
import logging
import subprocess
from pathlib import Path

from python.cpp_updater import recompiles_if_necessary
from python.io.formats import build_tsp_payload
from python.validate import validate_tour


def save_solution(task_path: Path, route_ids: list[int]) -> Path:
    out_path = task_path.with_name(task_path.stem + "_solution.txt")
    out_path.write_text(" ".join(str(x) for x in route_ids), encoding="utf-8")
    return out_path


def run_tsp(args, cpp_args):
    task_path = Path(args.task)
    coords_npz = Path(args.coords)

    payload, ids = build_tsp_payload(task_path, coords_npz, args.metric)

    recompiles_if_necessary()

    if args.history_file:
        cpp_args += ["--log_history_file", args.history_file]
    if args.checkpoint_file:
        cpp_args += ["--checkpoint_file", args.checkpoint_file]
    cpp_args += ["--checkpoint_every_sec", str(args.checkpoint_every_sec)]
    if args.run_time_limit > 0:
        cpp_args += ["--run_time_limit", str(args.run_time_limit)]

    p = subprocess.run(["build/src/tsp", "--problem", "tsp"] + cpp_args,
                       input=json.dumps(payload), text=True, capture_output=True)
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
