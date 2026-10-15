"""N02 measure/optimize/measure: frozen before/after native append and memory."""
from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent))
import measurement as m


def checked(command, cwd, timeout=240):
    row = m.run_process(command, cwd, timeout)
    if row["Status"] != "passed":
        raise RuntimeError(str(row))
    return row


def build(args, lane):
    folder = args.output.resolve() / lane
    folder.mkdir(parents=True, exist_ok=False)
    for name in ("bench.wio", "probe.h"):
        shutil.copyfile(HERE / name, folder / name)
    shutil.copyfile(HERE.parent / "native_costs/memory/probe.h", folder / "memory_probe.h")
    subprocess.run([str(args.wio.resolve()), str(folder / "bench.wio"), "--emit-cpp", "--include-dir", str(folder)],
                   env=dict(os.environ, WIO_ROOT=str(ROOT)), check=True, timeout=120)
    compiler = shutil.which(args.cxx)
    flags = ["-std=c++20", "-O2", "-ffp-contract=off", "-fsized-deallocation"]
    for kind in ("plain", "memory"):
        checked([compiler, *flags, *(["-DWIO_APPEND_MEMORY"] if kind == "memory" else []),
                 str(folder / "bench.wio.cpp"), f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}",
                 str(args.runtime.resolve()), *(["-lws2_32", "-lshell32", "-lpsapi"] if os.name == "nt" else ["-pthread"]),
                 "-o", str(folder / (kind + (".exe" if os.name == "nt" else "")))], folder)
    m.write_json(folder / "build.json", dict(Compiler=m.snapshot([args.wio])[0], Flags=flags,
                 Inputs=m.snapshot([HERE / "bench.wio", HERE / "probe.h", args.runtime, Path(compiler)]),
                 Artifacts=m.snapshot(p for p in folder.iterdir() if p.is_file())))
    return folder


def fingerprint(mode, count):
    data = (b"copy\0payload:0123456789abcdef0123456789abcdef" if mode == 1
            else "A🌍é日本語:payload:0123456789abcdef".encode()) * count
    result = 14695981039346656037
    for value in data:
        result = ((result ^ value) * 1099511628211) & ((1 << 64) - 1)
    return str(result)


def measure(args):
    if not 1 <= args.count <= 100000 or args.repetitions < 3:
        raise ValueError("Require count in [1,100000] and >=3 repetitions")
    directory = args.output.resolve()
    folders = {name: directory / name for name in ("before", "after")}
    for folder in folders.values():
        manifest = json.loads((folder / "build.json").read_text())
        for artifact in manifest["Artifacts"]:
            if m.snapshot([Path(artifact["Path"])])[0] != artifact:
                raise ValueError("Frozen artifact changed")
    for name in ("bench.wio", "probe.h", "memory_probe.h"):
        if (folders["before"] / name).read_bytes() != (folders["after"] / name).read_bytes():
            raise ValueError("Before/after benchmark inputs differ")
    inputs = [p for folder in folders.values() for p in folder.iterdir() if p.is_file()]
    inputs += [Path(__file__), Path(m.__file__)]
    before = m.snapshot(inputs)
    result = dict(Stage="N02", Samples=[], Correct=False, TimingQualified=False, Before=before, Failure=None,
                  EnvironmentBefore=m.environment(), Policy="No trimming or retries; instrumented memory separate from plain timings")
    try:
        with m.Affinity(args.cpu) as affinity:
            result["Affinity"] = affinity.info
            cases = [(lane, mode) for lane in folders for mode in (1, 2)] + [("control", 1)]
            for round_index in range(args.repetitions + 1):
                for lane, mode in m.order(cases, round_index):
                    folder = folders["before" if lane == "control" else lane]
                    process = checked([folder / ("plain.exe" if os.name == "nt" else "plain"), str(mode), str(args.count)], folder)
                    match = re.fullmatch(r"seconds=([0-9]+\.[0-9]+) checksum=([0-9]+)", process["Output"])
                    if (not match or match[2] != fingerprint(mode, args.count)
                            or not math.isfinite(float(match[1])) or float(match[1]) <= 0):
                        raise ValueError("Timing output/checksum mismatch")
                    result["Samples"].append(dict(Lane=lane, Mode=mode, Round=round_index,
                        Seconds=float(match[1]), Process=process))
                m.write_json(directory / "results.json", result)
            result["Summary"] = [dict(Lane=lane, Mode=mode,
                Timing=m.stability([s["Seconds"] for s in result["Samples"]
                                    if s["Lane"] == lane and s["Mode"] == mode and s["Round"] > 0], args.repetitions))
                for lane, mode in cases]
            result["Memory"] = []
            for lane, folder in folders.items():
                for mode in (1, 2):
                    row = checked([folder / ("memory.exe" if os.name == "nt" else "memory"), str(mode), str(args.count)], folder)
                    lines = row["Output"].splitlines()
                    checkpoints = [json.loads(line) for line in lines[:3]]
                    if (len(lines) != 4 or checkpoints[-1]["Checksum"] != fingerprint(mode, args.count)
                            or checkpoints[-1]["LiveBytes"] or checkpoints[-1]["LiveBlocks"]):
                        raise ValueError("Memory cleanup/checksum mismatch")
                    for checkpoint in checkpoints:
                        if (checkpoint["RequestedBytes"] - checkpoint["FreedBytes"] != checkpoint["LiveBytes"]
                                or checkpoint["Allocations"] - checkpoint["Frees"] != checkpoint["LiveBlocks"]
                                or checkpoint["PeakBytes"] < checkpoint["LiveBytes"]):
                            raise ValueError("Memory accounting invariant failed")
                    result["Memory"].append(dict(Lane=lane, Mode=mode, Checkpoints=checkpoints, Process=row))
            result["Correct"] = True
    except Exception as exc:
        result["Failure"] = str(exc)
    finally:
        result["After"] = m.snapshot(inputs)
        result["EnvironmentAfter"] = m.environment()
        power = result["EnvironmentBefore"]["PowerState"]
        result["PowerStateUnchanged"] = power is not None and power == result["EnvironmentAfter"]["PowerState"]
        result["Unchanged"] = before == result["After"]
        result["Correct"] &= result["Unchanged"]
        if result["Correct"]:
            a = result["Summary"][0]["Timing"]["Median"]
            b = result["Summary"][-1]["Timing"]["Median"]
            result["SameBinaryDifference"] = abs(a - b) / ((a + b) / 2)
            result["TimingQualified"] = (result["PowerStateUnchanged"] and
                all(r["Timing"]["Accepted"] for r in result["Summary"]) and result["SameBinaryDifference"] <= 0.1)
        m.write_json(directory / "results.json", result)
    print(json.dumps({k: result.get(k) for k in ("Correct", "TimingQualified", "Failure", "Summary")}))
    return result


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage", choices=("before", "after", "measure"))
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--wio", type=Path, default=ROOT / "build/app/Debug/wio.exe")
    p.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
    p.add_argument("--cxx", default="g++" if os.name == "nt" else "c++")
    p.add_argument("--count", type=int, default=2000)
    p.add_argument("--repetitions", type=int, default=15)
    p.add_argument("--cpu", type=int, default=2)
    args = p.parse_args()
    if args.stage == "measure":
        raise SystemExit(0 if measure(args)["Correct"] else 1)
    build(args, args.stage)
