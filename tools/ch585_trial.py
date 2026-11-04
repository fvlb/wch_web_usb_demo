#!/usr/bin/env python3
"""Flash/readback/test one CH585 experiment; restore known-good firmware on failure.

Run the entire script under station-mutex for the CH585's own UUID resource.
The cabinet/station resource is unrelated to this demoboard.
"""
import argparse
import hashlib
import json
import pathlib
import signal
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=pathlib.Path, required=True)
    parser.add_argument("--fallback", type=pathlib.Path, required=True)
    parser.add_argument("--record", type=pathlib.Path, required=True)
    parser.add_argument("--minichlink", required=True)
    parser.add_argument("--bench", required=True)
    parser.add_argument("--queue", type=int, default=16)
    parser.add_argument("--request-bytes", type=int, default=65536)
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--pattern", action="store_true", help="Diagnostic static pattern, not gradient BMP")
    parser.add_argument("--uuid", default="df44848819707c3d")
    args = parser.parse_args()
    resource = "CH585-" + args.uuid
    mutex = pathlib.Path.home() / ".local/bin/station-mutex"
    subprocess.run([str(mutex), "check", resource], check=True)
    if not args.image.is_file() or not args.fallback.is_file():
        parser.error("Both candidate and known-good fallback must exist")
    args.record.parent.mkdir(parents=True, exist_ok=True)
    report = {"image": str(args.image), "fallback": str(args.fallback),
              "image_sha256": hashlib.sha256(args.image.read_bytes()).hexdigest(),
              "queue": args.queue, "request_bytes": args.request_bytes, "runs": args.runs,
              "uuid": args.uuid, "pattern": args.pattern, "commands": [], "success": False}

    def command(argv, timeout=90):
        run = subprocess.run(argv, text=True, capture_output=True, timeout=timeout)
        report["commands"].append({"argv": argv, "returncode": run.returncode,
                                   "stdout": run.stdout, "stderr": run.stderr})
        print(run.stdout, end="", flush=True)
        print(run.stderr, end="", file=sys.stderr, flush=True)
        run.check_returncode()
        return run

    def program(path, suffix):
        readback = args.record.with_suffix(f".{suffix}.readback.bin")
        command([str(mutex), "protect", "--label", "CH585 flash + readback", resource, "--",
                 args.minichlink, "-w", str(path), "0x0", "-r", str(readback), "flash",
                 str(path.stat().st_size), "-a", "-b"])
        if readback.read_bytes() != path.read_bytes():
            raise ValueError(f"Flash readback mismatch: {path}")
        time.sleep(1)

    def terminated(signum, frame):
        del signum, frame
        raise KeyboardInterrupt("Interrupted hardware experiment")

    signal.signal(signal.SIGTERM, terminated)
    attempted = False
    try:
        identity = command([args.minichlink, "-i", "-e"])
        identification = (identity.stdout + identity.stderr).lower().replace("-", "")
        if "detected ch585" not in identification or args.uuid.lower() not in identification:
            raise ValueError("Unexpected MCU/UUID; refusing to program it")
        attempted = True
        program(args.image, "candidate")
        bench = [args.bench, str(args.queue), str(args.request_bytes), str(args.runs)]
        if args.pattern:
            bench.append("pattern")
        command(bench, timeout=120)
        report["success"] = True
    except (Exception, KeyboardInterrupt) as error:
        report["error"] = str(error)
        print(f"Experiment failed: {error}", file=sys.stderr)
    finally:
        if attempted and not report["success"]:
            try:
                program(args.fallback, "restore")
                report["restored"] = True
            except Exception as error:
                report["restore_error"] = str(error)
                print(f"RESTORE FAILED: {error}", file=sys.stderr)
        elif not attempted:
            # Initialization can halt the chip even for an identity query.
            command([args.minichlink, "-e"])
        args.record.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
