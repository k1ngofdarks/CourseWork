from __future__ import annotations

import json
import logging
import subprocess

from python.cpp_updater import recompiles_if_necessary


def run_mdmtsp_minmax(args, cpp_args):
    recompiles_if_necessary()

    payload = {"problem": "mdmtsp_minmax"}
    p = subprocess.run(["build/src/tsp", "--problem", "mdmtsp_minmax"] + cpp_args,
                       input=json.dumps(payload), text=True, capture_output=True)
    if p.returncode != 0:
        raise RuntimeError(p.stderr)

    output = json.loads(p.stdout)
    logging.info("mdmtsp_minmax status: %s", output.get("status"))
    logging.info("message: %s", output.get("message"))
