from __future__ import annotations

import json
import logging
import subprocess
from pathlib import Path

from python.cpp_updater import recompiles_if_necessary
from python.io.formats import build_mdmtsp_payload


def run_mdmtsp_minmax(args, cpp_args):
    recompiles_if_necessary()

    payload = build_mdmtsp_payload(Path(args.task))
    p = subprocess.run(["build/src/tsp", "--problem", "mdmtsp_minmax"] + cpp_args,
                       input=json.dumps(payload), text=True, capture_output=True)
    if p.returncode != 0:
        raise RuntimeError(p.stderr)

    output = json.loads(p.stdout)
    logging.info("mdmtsp_minmax status: %s", output.get("status"))
    logging.info("solver: %s | max_route_length: %s", output.get("solver"), output.get("max_route_length"))
    logging.info("routes: %s", output.get("routes"))
