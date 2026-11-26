"""Frozen-artifact Mandelbrot tracking through the common Python controller."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import measurement as m

ROOT = Path(__file__).resolve().parents[3]
PPM_BYTES = 5156528
PPM_HASH = "1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733"


def compute_seconds(output):
    match = re.fullmatch(r"seconds=([0-9]+\.[0-9]+) iterations=11613930 weighted=3721766287256 interior=94440", output)
    if not match:
        raise ValueError("Compute fingerprint mismatch")
    value = float(match[1])
    if not 0 < value < float("inf"):
        raise ValueError("Invalid compute duration")
    return value


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--artifacts", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--vm-host", type=Path, default=ROOT / "build/vm/Release/wio_mandelbrot_vm_benchmark.exe")
    p.add_argument("--repetitions", type=int, default=5)
    p.add_argument("--vm-repetitions", type=int, default=3)
    p.add_argument("--timeout", type=float, default=180)
    p.add_argument("--cpu", type=int, default=-1)
    p.add_argument("--native-only", action="store_true")
    p.add_argument("--allow-unpinned", action="store_true")
    args = p.parse_args()
    if min(args.repetitions, args.vm_repetitions) < 3 or not 0 < args.timeout < float("inf"):
        p.error("Require >=3 samples per backend and finite positive timeout")
    directory, output, host = args.artifacts.resolve(), args.output.resolve(), args.vm_host.resolve()
    if output.exists():
        p.error("Choose a fresh output directory")
    cases = []
    paths = [Path(__file__), Path(m.__file__)]
    for kind in ("compute", "full"):
        folder = directory / kind
        if not folder.exists():
            if kind == "compute":
                p.error("Compute artifacts required")
            continue
        native = folder / ("native.exe" if (folder / "native.exe").exists() else "native")
        cases.append((kind + "-native", kind, [native], folder, args.repetitions))
        if kind == "compute":
            cases.append(("compute-native-control", kind, [native], folder, args.repetitions))
        previous = folder / "previous.exe"
        if kind == "full" and previous.exists():
            cases.append(("full-previous", kind, [previous], folder, args.repetitions))
        if not args.native_only:
            cases.append((kind + "-vm", kind, [host, folder / "main.wiob", folder / "bridge.dll"], folder, args.vm_repetitions))
        paths += [f for f in folder.iterdir() if f.is_file() and f.suffix in (".exe", ".wiob", ".dll", ".cpp", ".wio", ".h")]
        paths.append(native)
    paths += [Path(program) for _, _, command, _, _ in cases for program in command]
    paths += [directory / f for f in ("build.json", "results.json") if (directory / f).exists()]
    before = m.snapshot(paths)
    result = dict(Schema=3, Harness="python", Stage="N01", Rebuilt=False, ArtifactsBefore=before,
                  Samples=[], Summary=[], Failure=None, Affinity=None, ArtifactsUnchanged=False,
                  EnvironmentBefore=m.environment(), EnvironmentAfter=None, OutputsCorrect=False,
                  TimingQualified=False, Accepted=False, PowerStateUnchanged=False,
                  Policy="Warm-up per lane; rotate/reverse; no trimming/retries; internal kernel unchanged; child wall excludes worker startup")
    output.mkdir(parents=True)
    def save():
        m.write_json(output / "results.json", result)
    try:
        with m.Affinity(args.cpu, args.allow_unpinned) as affinity:
            result["Affinity"] = affinity.info
            for round_index in range(max(args.repetitions, 0 if args.native_only else args.vm_repetitions) + 1):
                for name, kind, command, folder, count in m.order(cases, round_index):
                    if round_index > count:
                        continue
                    ppm = folder / "mandelbrot.ppm"
                    old_stamp = ppm.stat().st_mtime_ns if ppm.exists() else None
                    row = m.run_process(command, folder, args.timeout)
                    row.update(Name=name, Round=round_index, Warmup=round_index == 0, KernelSeconds=None)
                    result["Samples"].append(row)
                    if row["Status"] == "passed":
                        try:
                            if kind == "compute":
                                row["KernelSeconds"] = compute_seconds(row["Output"])
                            else:
                                if not ppm.exists() or ppm.stat().st_mtime_ns == old_stamp:
                                    raise ValueError("Stale PPM output")
                                fingerprint = m.snapshot([ppm])[0]
                                row.update(Bytes=fingerprint["Bytes"], Hash=fingerprint["Hash"])
                                if row["Bytes"] != PPM_BYTES or row["Hash"] != PPM_HASH:
                                    raise ValueError("PPM fingerprint mismatch")
                        except ValueError as exc:
                            row.update(Status="checksum-mismatch", Error=str(exc))
                    save()
                    print(f'{name} [{round_index}] {row["Status"]}: kernel={row["KernelSeconds"]}, child-wall={row["WallSeconds"]}', flush=True)
                    if row["Status"] != "passed":
                        raise RuntimeError(f'{name}: {row["Status"]}')
            for name, kind, _, _, count in cases:
                metric = "KernelSeconds" if kind == "compute" else "WallSeconds"
                values = [r[metric] for r in result["Samples"] if r["Name"] == name and not r["Warmup"]]
                result["Summary"].append(dict(Name=name, Metric=metric, Stability=m.stability(values, count)))
            a, b = [next(r["Stability"]["Median"] for r in result["Summary"] if r["Name"] == name) for name in ("compute-native", "compute-native-control")]
            difference = abs(a - b) / ((a + b) / 2)
            result["SameBinaryControl"] = dict(RelativeMedianDifference=difference, Accepted=difference <= 0.10)
    except Exception as exc:
        result["Failure"] = str(exc)
    finally:
        result["EnvironmentAfter"] = m.environment()
        try:
            result["ArtifactsAfter"] = m.snapshot(paths)
            result["ArtifactsUnchanged"] = before == result["ArtifactsAfter"]
        except OSError as exc:
            result["Failure"] = str(exc)
        power = result["EnvironmentBefore"]["PowerState"]
        result["PowerStateUnchanged"] = power is not None and power == result["EnvironmentAfter"]["PowerState"]
        result["OutputsCorrect"] = not result["Failure"] and result["ArtifactsUnchanged"] and len(result["Samples"]) == sum(c[4] + 1 for c in cases)
        result["TimingQualified"] = bool(result["OutputsCorrect"] and result["Affinity"]["Supported"] and result["PowerStateUnchanged"] and result["SameBinaryControl"]["Accepted"] and all(r["Stability"]["Accepted"] for r in result["Summary"]))
        result["Accepted"] = result["TimingQualified"]
        save()
    return 0 if result["TimingQualified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
