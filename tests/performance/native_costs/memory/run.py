"""N01 Wio live/peak requested-byte diagnostics; never a throughput benchmark."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE.parents[1]))
import measurement as m

COUNTERS = ("Allocations Frees RequestedBytes FreedBytes LiveBytes PeakBytes LiveBlocks "
            "PeakBlocks AlignedAllocations Failures").split()
PROCESS = "ResidentBytes ProcessLifetimePeakResidentBytes PrivateCommitBytes".split()
CASES = ("control", "aligned", "weak", "object", "string", "bytes")


def checksum(kind, count):
    if kind == "control":
        return str(192 * count + 4)
    if kind == "aligned":
        return str(count + 8499)
    if kind == "weak":
        return str(count + 1)
    result = 0
    for index in range(count):
        value = index % 256 if kind == "bytes" else index
        if kind == "string":
            value = 14695981039346656037
            for byte in f"entry:{index}:0123456789abcdef0123456789abcdef".encode("utf-8"):
                value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
        result = (result * 33 + value) & ((1 << 64) - 1)
    return str(result)


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("Duplicate JSON key")
        value[key] = item
    return value


def parse(output, kind, count):
    def invalid_constant(value):
        raise ValueError(f"Non-finite JSON constant: {value}")

    rows = [json.loads(line, object_pairs_hook=unique_object, parse_constant=invalid_constant)
            for line in output.splitlines()]
    if (len(rows) != 3 or not all(isinstance(r, dict) for r in rows)
            or [r.get("Phase") for r in rows] != ["baseline", "live", "released"]):
        raise ValueError("Expected exactly baseline/live/released checkpoints")
    keys = set(COUNTERS + PROCESS + ["Phase", "Checksum", "MemorySource", "MemoryAvailable"])
    for row in rows:
        if set(row) != keys:
            raise ValueError("Memory protocol fields do not match schema")
        for key in COUNTERS:
            if type(row[key]) is not int or not 0 <= row[key] <= (1 << 64) - 1:
                raise ValueError(f"Invalid unsigned counter: {key}")
        if type(row["MemoryAvailable"]) is not bool or row["MemorySource"] not in (
                "win32-working-set", "linux-proc-status-approximate", "darwin-mach-task-basic-info", "unsupported"):
            raise ValueError("Invalid process-memory source metadata")
        for key in PROCESS:
            value = row[key]
            if value is not None and (type(value) is not int or not 0 <= value <= (1 << 64) - 1):
                raise ValueError("Invalid OS memory metric")
            if not row["MemoryAvailable"] and value is not None:
                raise ValueError("Unavailable memory metric must be null, not zero")
        if row["MemoryAvailable"] and any(row[k] is None for k in PROCESS[:2]):
            raise ValueError("Available resident-memory snapshots must contain both values")
        if row["MemorySource"] == "unsupported" and row["MemoryAvailable"]:
            raise ValueError("Unsupported source cannot report available measurements")
        if row["MemorySource"] != "win32-working-set" and row["PrivateCommitBytes"] is not None:
            raise ValueError("Private commit is only measured on Windows")
        if (row["RequestedBytes"] - row["FreedBytes"] != row["LiveBytes"]
                or row["Allocations"] - row["Frees"] != row["LiveBlocks"]
                or row["PeakBytes"] < row["LiveBytes"] or row["PeakBlocks"] < row["LiveBlocks"]
                or row["AlignedAllocations"] > row["Allocations"]):
            raise ValueError("Allocation accounting invariant failed")
    if any(rows[0][key] for key in COUNTERS):
        raise ValueError("Baseline must precede the allocation window")
    for previous, current in zip(rows, rows[1:]):
        if any(current[key] < previous[key] for key in COUNTERS if key not in ("LiveBytes", "LiveBlocks")):
            raise ValueError("Cumulative counter decreased")
    if (rows[0]["Checksum"] != "0" or rows[1]["Checksum"] != "0"
            or rows[2]["Checksum"] != checksum(kind, count)):
        raise ValueError("Independent Wio checksum mismatch")
    if rows[2]["LiveBytes"] or rows[2]["LiveBlocks"]:
        raise ValueError("Window-owned allocations remain after Wio scope cleanup")
    if not rows[1]["LiveBlocks"] or not rows[1]["LiveBytes"]:
        raise ValueError("Live checkpoint failed to retain workload storage")
    if kind in ("control", "aligned"):
        expected = int(checksum(kind, count))
        allocations, aligned, failures = (2, 0, 0) if kind == "control" else (6, 3, 2)
        for row in rows[1:]:
            if (row["Allocations"] != allocations or row["RequestedBytes"] != expected
                    or row["PeakBytes"] != expected or row["PeakBlocks"] != allocations
                    or row["AlignedAllocations"] != aligned or row["Failures"] != failures):
                raise ValueError("Native allocator calibration failed")
    return rows


def checked(command, directory, timeout=120):
    row = m.run_process(command, directory, timeout)
    if row["Status"] != "passed":
        raise RuntimeError(f"{command}: {row}")
    return row


def run(args):
    if (not args.counts or len(set(args.counts)) != len(args.counts)
            or any(not 1 <= count <= 100000 for count in args.counts)):
        raise ValueError("Require unique counts in [1,100000]")
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which(args.cxx)
    if not compiler:
        raise ValueError("Native compiler not found")
    wio, runtime = args.wio.resolve(), args.runtime.resolve()
    paths = [Path(__file__), Path(m.__file__), Path(compiler), wio, runtime, HERE / "costs.wio", HERE / "probe.h"]
    paths += [p for tree in (ROOT / "runtime/include", ROOT / "sdk/include", ROOT / "std")
              for p in tree.rglob("*") if p.is_file()]
    before = m.snapshot(paths)
    for name in ("costs.wio", "probe.h"):
        shutil.copyfile(HERE / name, directory / name)
    env = dict(os.environ, WIO_ROOT=str(ROOT))
    subprocess.run([str(wio), str(directory / "costs.wio"), "--emit-cpp", "--include-dir", str(directory)],
                   check=True, env=env, timeout=120)
    generated = directory / "costs.wio.cpp"
    program = directory / ("memory.exe" if os.name == "nt" else "memory")
    flags = ["-std=c++20", "-O2", "-fsized-deallocation"]
    compilation = checked([
        compiler, *flags, str(generated), f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}",
        str(runtime), *(["-lws2_32", "-lshell32", "-lpsapi"] if os.name == "nt" else ["-pthread"]),
        "-o", str(program)], directory)
    artifacts = [directory / "costs.wio", directory / "probe.h", generated, program]
    artifact_before = m.snapshot(artifacts)
    result = dict(Schema=1, Stage="N01", DiagnosticOnly=True, TimingQualified=False,
                  Flags=flags, SourcesBefore=before, ArtifactsBefore=artifact_before,
                  BuildProcess=compilation,
                  Revision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  Worktree=subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
                  Samples=[], Correct=False, Failure=None,
                  Policy="One process/window per case; requested-byte peaks are not RSS; no timing claim")
    try:
        for count in args.counts:
            for kind in CASES:
                process = checked([program, kind, str(count)], directory)
                sample = dict(Kind=kind, Count=count, Process=process, Checkpoints=None)
                result["Samples"].append(sample)
                sample["Checkpoints"] = parse(process["Output"], kind, count)
                live = sample["Checkpoints"][1]
                released = sample["Checkpoints"][2]
                print(f'{kind}/{count}: live={live["LiveBytes"]}, peak={released["PeakBytes"]}, '
                      f'allocs={released["Allocations"]}, released={released["LiveBytes"]}', flush=True)
        result["Correct"] = True
    except Exception as exc:
        result["Failure"] = str(exc)
    finally:
        result["SourcesAfter"] = m.snapshot(paths)
        result["ArtifactsAfter"] = m.snapshot(artifacts)
        result["SourcesUnchanged"] = before == result["SourcesAfter"]
        result["ArtifactsUnchanged"] = artifact_before == result["ArtifactsAfter"]
        result["Correct"] &= result["SourcesUnchanged"] and result["ArtifactsUnchanged"]
        if not result["SourcesUnchanged"] or not result["ArtifactsUnchanged"]:
            result["Failure"] = result["Failure"] or "Sources or immutable artifacts changed"
        m.write_json(directory / "results.json", result)
    return result


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--wio", type=Path, default=ROOT / "build/app/Debug/wio.exe")
    p.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
    p.add_argument("--cxx", default="g++" if os.name == "nt" else "c++")
    p.add_argument("--counts", type=int, nargs="+", default=[1, 17, 2000])
    return p


if __name__ == "__main__":
    args = parser().parse_args()
    try:
        result = run(args)
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(str(exc), file=sys.stderr)
        if args.output.is_dir():
            m.write_json(args.output / "failure.json", dict(Correct=False, Failure=str(exc)))
        raise SystemExit(2)
    if not result["Correct"]:
        print(result["Failure"], file=sys.stderr)
    raise SystemExit(0 if result["Correct"] else 1)
