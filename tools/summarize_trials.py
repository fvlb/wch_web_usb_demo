#!/usr/bin/env python3
"""Summarize saved CH585 trial JSON, keeping failed runs visible."""
import argparse
import json
import pathlib
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=pathlib.Path)
    args = parser.parse_args()
    for path in sorted(args.directory.glob("*.json")):
        record = json.loads(path.read_text())
        samples = []
        for command in record.get("commands", []):
            for line in command.get("stdout", "").splitlines():
                if line.startswith('{"run":'):
                    samples.append(json.loads(line))
        if not samples:
            continue
        print(json.dumps({
            "trial": path.stem, "success": record["success"],
            "runs": len(samples), "verified": sum(s["verified"] for s in samples),
            "MB_s_min": min(s["MB_s"] for s in samples),
            "MB_s_median": statistics.median(s["MB_s"] for s in samples),
            "MB_s_max": max(s["MB_s"] for s in samples),
            "MB_s_aggregate": sum(s["bytes"] for s in samples) / sum(s["seconds"] for s in samples) / 1e6,
            "bytes": sum(s["bytes"] for s in samples),
            "image_sha256": record["image_sha256"],
        }))


if __name__ == "__main__":
    main()
