"""Read LUFS Meter JSON snapshots; standard library only, no OBS writes."""
from pathlib import Path
import argparse
import copy
import json
import math
import os
import sys
import time


class MeterReadError(ValueError):
    pass


def default_path():
    base = os.environ.get("APPDATA")
    if not base:
        raise MeterReadError("Use --file for this OBS installation.")
    return Path(base) / "obs-studio/plugin_config/obs-lufs-meter/readings.json"


def reject_constant(value):
    raise MeterReadError(f"Invalid JSON number: {value}")


def read_snapshot(path, max_age=2.0, allow_stale=False, now_ms=None):
    path = Path(path)
    if not 0.1 <= max_age <= 60 or not math.isfinite(max_age):
        raise MeterReadError("max-age must be between 0.1 and 60 seconds")
    try:
        with path.open("rb") as stream:
            payload = stream.read(2_000_001)
        if len(payload) > 2_000_000:
            raise MeterReadError("Snapshot is too large")
        data = json.loads(payload, parse_constant=reject_constant)
    except (OSError, ValueError) as error:
        raise MeterReadError(f"Cannot read meter snapshot: {error}") from error
    if not isinstance(data, dict) or data.get("schemaVersion") != 1 or data.get("app") != "LUFS Meter":
        raise MeterReadError("Unsupported meter snapshot schema")
    stamp = data.get("updatedAtUnixMs")
    if type(stamp) not in (float, int) or not math.isfinite(stamp):
        raise MeterReadError("Invalid snapshot timestamp")
    age = (time.time() * 1000 if now_ms is None else now_ms) - stamp
    fresh = -1000 <= age <= max_age * 1000
    active = data.get("exportEnabled") is True and data.get("status") == "running"
    if not allow_stale and (not fresh or not active):
        raise MeterReadError(f"Meter unavailable: status={data.get('status')}, ageMs={round(age)}")
    if not all(isinstance(data.get(key), list) for key in ("masters", "sources")):
        raise MeterReadError("Invalid meter rows")
    data["reader"] = {"fresh": fresh and active, "ageMs": round(age), "path": str(path.resolve())}
    return data


def select_rows(data, source=None, master=False):
    selected = copy.deepcopy(data)
    if source is not None:
        found = [row for row in data["sources"] if row.get("id") == source or row.get("name") == source]
        if len(found) != 1:
            raise MeterReadError(f"Expected one matching source; found {len(found)}. Use its UUID.")
        selected["sources"] = found
        selected["masters"] = []
    elif master:
        selected["sources"] = []
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", type=Path, help="Copy this path from Settings > External integration")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--source", help="Exact source UUID or unique name")
    selection.add_argument("--master", action="store_true")
    parser.add_argument("--max-age", type=float, default=2.0)
    parser.add_argument("--allow-stale", action="store_true", help="Inspect inactive data; reader.fresh will be false")
    sampling = parser.add_mutually_exclusive_group()
    sampling.add_argument("--samples", type=int, default=1)
    sampling.add_argument("--watch", action="store_true", help="Stream NDJSON until Ctrl+C")
    parser.add_argument("--interval", type=float, default=0.5)
    args = parser.parse_args()
    if not 1 <= args.samples <= 3600 or not 0.1 <= args.interval <= 60 or not math.isfinite(args.interval):
        parser.error("Use 1..3600 samples and a 0.1..60 second interval")
    try:
        path = args.file if args.file is not None else default_path()
        count = 0
        while args.watch or count < args.samples:
            data = read_snapshot(path, args.max_age, args.allow_stale)
            data = select_rows(data, args.source, args.master)
            print(json.dumps(data, ensure_ascii=False, allow_nan=False), flush=True)
            count += 1
            if args.watch or count < args.samples:
                time.sleep(args.interval)
        return 0
    except MeterReadError as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
