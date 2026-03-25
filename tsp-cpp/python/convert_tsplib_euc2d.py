#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def parse_tsplib_euc2d(path: Path) -> list[list[float]]:
    coordinates: list[list[float]] = []
    in_section = False
    edge_weight_type = None

    with path.open() as source:
        for raw_line in source:
            line = raw_line.strip()
            if not line:
                continue

            if not in_section:
                if ":" in line:
                    key, value = [part.strip() for part in line.split(":", 1)]
                    if key == "EDGE_WEIGHT_TYPE":
                        edge_weight_type = value
                if line == "NODE_COORD_SECTION":
                    in_section = True
                continue

            if line == "EOF":
                break

            parts = line.split()
            if len(parts) < 3:
                raise ValueError(f"Invalid coordinate row in {path}: {line}")
            coordinates.append([float(parts[1]), float(parts[2])])

    if edge_weight_type != "EUC_2D":
        raise ValueError(f"{path} is not EUC_2D, got {edge_weight_type!r}")
    if not coordinates:
        raise ValueError(f"No coordinates found in {path}")

    return coordinates


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert TSPLIB EUC_2D instance to project JSON")
    parser.add_argument("input", type=Path, help="Path to .tsp file")
    parser.add_argument("output", type=Path, help="Path to output .json file")
    args = parser.parse_args()

    payload = {
        "coordinates": parse_tsplib_euc2d(args.input),
        "metric": "euclidean",
    }

    args.output.write_text(json.dumps(payload, ensure_ascii=True, indent=2) + "\n")


if __name__ == "__main__":
    main()
