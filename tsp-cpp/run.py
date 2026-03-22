from __future__ import annotations

import argparse
import logging

from python.runners.tsp_runner import run_tsp
from python.runners.mdmtsp_runner import run_mdmtsp_minmax


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Solve routing tasks with cpp solvers.")
    p.add_argument("--problem", type=str, default="tsp", choices=["tsp", "mdmtsp_minmax"],
                   help="Problem type.")
    p.add_argument("--task", type=str, required=True, help="Path to task txt/json.")
    p.add_argument("--coords", type=str, default="World_TSP.npz", help="Path to coordinates NPZ [idx, lat, lon].")
    p.add_argument("--metric", type=str, default="haversine", choices=["haversine", "euclidean"],
                   help="Metric for coordinate-based tasks.")
    p.add_argument("--checkpoint-file", type=str, default="", help="Path to best-so-far checkpoint JSON.")
    p.add_argument("--history-file", type=str, default="", help="Path to improvements history JSONL.")
    p.add_argument("--checkpoint-every-sec", type=float, default=30.0, help="Periodic checkpoint flush interval.")
    p.add_argument("--run-time-limit", type=float, default=-1, help="Global runtime limit in seconds.")
    return p.parse_known_args()


def main() -> None:
    args, cpp_args = parse_args()
    logging.basicConfig(level=logging.INFO, format="%(message)s")

    if args.problem == "mdmtsp_minmax":
        run_mdmtsp_minmax(args, cpp_args)
        return

    run_tsp(args, cpp_args)


if __name__ == "__main__":
    main()
