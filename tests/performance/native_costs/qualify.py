"""Measure the existing Wio workloads; Python handles orchestration only."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import measurement as m

ROOT = Path(__file__).resolve().parents[3]
CORE = "concat builder format bytes array component object native_call result".split()
EXTENDED = "unicode dictionary result_error weak closure async_ready".split()
GOLDENS = {
    1: [17445855498045112857, 99, 145247952907163005, 1, 1, 1],
    17: [16810242653416937817, 10007309852365595643, 2535633178212605909] + [3335769950788531865] * 3,
    2000: [11864642420176368741, 16071255685101081899, 7352865453926614568] + [11505999919603546472] * 3,
}


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--artifacts", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--repetitions", type=int, default=15)
    p.add_argument("--timeout", type=float, default=120)
    p.add_argument("--cpu", type=int, default=-1)
    p.add_argument("--max-range", type=float, default=0.15)
    p.add_argument("--max-drift", type=float, default=0.10)
    p.add_argument("--allow-unpinned", action="store_true")
    p.add_argument("--legacy-instrumented-core", action="store_true")
    return p


def load_build(directory, legacy=False):
    provenance = directory / "build.json"
    if not provenance.exists():
        provenance = directory / "results.json"
    build = json.loads(provenance.read_text(encoding="utf-8-sig"))
    if not isinstance(build, dict):
        raise ValueError("Build provenance must be a JSON object")
    suite, counters, count = build.get("Suite"), build.get("CountersEnabled"), build.get("Count")
    if legacy:
        if suite is not None or counters is not None or not build.get("Scope", "").startswith("instrumented ordinary new/new") or any("WIO_COST_NO_COUNTERS" in flag for flag in build.get("Flags", [])):
            raise ValueError("Legacy override only accepts original instrumented core provenance")
        suite, counters = "core", True
    if suite not in ("core", "extended") or type(counters) is not bool or type(count) is not int or not 1 <= count <= 100000:
        raise ValueError("Invalid suite/counter/count provenance")
    programs = build.get("Programs", {"cpp": "cpp.exe", "wio": "wio.exe"})
    if not isinstance(programs, dict) or set(programs) != {"cpp", "wio"} or not all(isinstance(v, str) for v in programs.values()):
        raise ValueError("Invalid executable program mapping")
    records = {}
    items = build.get("Artifacts", [])
    if not isinstance(items, list):
        raise ValueError("Artifacts must be a list")
    for item in items:
        if not isinstance(item, dict):
            raise ValueError("Artifact must be an object")
        name = item.get("Path", "")
        if not isinstance(name, str) or not name or "/" in name or "\\" in name or name in (".", "..") or name.lower() in records:
            raise ValueError("Provenance requires unique local artifact filenames")
        recorded_hash = item.get("Hash", "")
        if not isinstance(recorded_hash, str) or m.snapshot([directory / name])[0]["Hash"].upper() != recorded_hash.upper():
            raise ValueError(f"Frozen artifact does not match provenance: {name}")
        records[name.lower()] = item
    if any(name.lower() not in records for name in programs.values()):
        raise ValueError("Provenance must include both executable programs")
    return provenance, suite, counters, count, programs


def qualify(args, runner=None):
    runner = runner or m.run_process
    if args.repetitions < 3 or not math.isfinite(args.timeout) or args.timeout <= 0:
        raise ValueError("Require >=3 repetitions and finite positive timeout")
    m.stability([], args.repetitions, args.max_range, args.max_drift)  # validate limits before any launch
    directory, output = args.artifacts.resolve(), args.output.resolve()
    if output.exists():
        raise ValueError("Choose a fresh output directory")
    provenance, suite, counters, count, programs = load_build(directory, args.legacy_instrumented_core)
    paths = [Path(__file__), Path(m.__file__), provenance]
    paths += [p for p in directory.iterdir() if p.is_file() and (p.suffix in (".exe", ".wio", ".cpp", ".h") or p.name in programs.values())]
    before = m.snapshot(paths)
    kinds = CORE if suite == "core" else EXTENDED
    goldens = dict(zip(EXTENDED, map(str, GOLDENS[count]))) if suite == "extended" and count in GOLDENS else {}
    cases = [(kind, variant, programs[variant]) for kind in kinds for variant in ("cpp", "wio")]
    cases.append((kinds[0], "cpp-control", programs["cpp"]))
    expected, samples, summary = {}, [], []
    result = dict(Schema=3, Harness="python", Stage="N01", ArtifactDirectory=str(directory), Rebuilt=False,
                  Suite=suite, Count=count, CountersEnabled=counters, Repetitions=args.repetitions,
                  TimeoutSeconds=args.timeout, LegacyInstrumentedCore=args.legacy_instrumented_core,
                  Limits=dict(RelativeRange=args.max_range, RelativeDrift=args.max_drift),
                  Policy="Excluded warm-ups; rotating/reversing order; same-executable A/A; no trimming or retries",
                  ChecksumOracle="Independent goldens plus C++ parity" if goldens else "C++ warm-up parity, not independent algorithm proof",
                  Revision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  Worktree=subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
                  ArtifactsBefore=before, ArtifactsAfter=[], ArtifactsUnchanged=False, Samples=samples,
                  Summary=summary, Affinity=None, EnvironmentBefore=m.environment(), EnvironmentAfter=None,
                  PowerStateUnchanged=False, OutputsCorrect=False, TimingQualified=False, Accepted=False,
                  Failure=None, SameBinaryControl=None)
    output.mkdir(parents=True)

    def save():
        m.write_json(output / "results.json", result)

    try:
        with m.Affinity(args.cpu, args.allow_unpinned) as affinity:
            result["Affinity"] = affinity.info
            save()
            for round_index in range(args.repetitions + 1):
                for kind, variant, program in m.order(cases, round_index):
                    process = runner([directory / program, kind, str(count)], directory, args.timeout)
                    parsed = m.parse_cost(process["Output"])
                    status = process["Status"]
                    if status == "passed":
                        status = parsed["Status"]
                        if status == "passed":
                            if not counters and (parsed["Allocations"] or parsed["RequestedBytes"]):
                                status = "counter-mode-mismatch"
                            elif kind in goldens and parsed["Checksum"] != goldens[kind]:
                                status = "independent-checksum-mismatch"
                            else:
                                if round_index == 0 and variant == "cpp":
                                    expected[kind] = parsed["Checksum"]
                                if parsed["Checksum"] != expected.get(kind):
                                    status = "checksum-mismatch"
                    row = dict(process, **{key: value for key, value in parsed.items() if key != "Status"})
                    row.update(Case=kind, Variant=variant, Round=round_index, Warmup=round_index == 0, Status=status)
                    if not counters:
                        row.update(Allocations=None, RequestedBytes=None)
                    samples.append(row)
                    save()
                    if status != "passed":
                        raise RuntimeError(f"{kind}/{variant}: {status}")
                print(f"Round {round_index}: {len(cases)} checked executions", flush=True)
            for kind, variant, _ in cases:
                rows = [r for r in samples if r["Case"] == kind and r["Variant"] == variant and not r["Warmup"]]
                summary.append(dict(Case=kind, Variant=variant,
                                    Timing=m.stability([r["Seconds"] for r in rows], args.repetitions, args.max_range, args.max_drift),
                                    Allocations=sorted({r["Allocations"] for r in rows}) if counters else None,
                                    RequestedBytes=sorted({r["RequestedBytes"] for r in rows}) if counters else None))
            a, b = [next(r["Timing"]["Median"] for r in summary if r["Case"] == kinds[0] and r["Variant"] == v) for v in ("cpp", "cpp-control")]
            difference = abs(a - b) / ((a + b) / 2)
            result["SameBinaryControl"] = dict(SameExecutable=True, RelativeMedianDifference=difference, Accepted=difference <= args.max_drift)
    except Exception as exc:
        result["Failure"] = str(exc)
    finally:
        result["EnvironmentAfter"] = m.environment()
        try:
            result["ArtifactsAfter"] = m.snapshot(paths)
            result["ArtifactsUnchanged"] = before == result["ArtifactsAfter"]
        except OSError as exc:
            result["Failure"] = f"Artifact verification: {exc}"
        power = result["EnvironmentBefore"]["PowerState"]
        result["PowerStateUnchanged"] = power is not None and power == result["EnvironmentAfter"]["PowerState"]
        result["OutputsCorrect"] = not result["Failure"] and result["ArtifactsUnchanged"] and len(samples) == len(cases) * (args.repetitions + 1) and all(r["Status"] == "passed" for r in samples)
        result["TimingQualified"] = bool(result["OutputsCorrect"] and result["Affinity"] and result["Affinity"]["Supported"] and result["PowerStateUnchanged"] and result["SameBinaryControl"]["Accepted"] and all(r["Timing"]["Accepted"] for r in summary))
        result["Accepted"] = result["TimingQualified"]
        save()
        m.write_json(output / "summary.json", summary)
    return result


def main():
    args = parser().parse_args()
    try:
        result = qualify(args)
    except (ValueError, OSError, KeyError) as exc:
        print(str(exc), file=sys.stderr)
        return 2
    for row in result["Summary"]:
        print(f'{row["Case"]}/{row["Variant"]}: {row["Timing"]["Median"] * 1000:.6f} ms, stable={row["Timing"]["Accepted"]}')
    print(f'OutputsCorrect={result["OutputsCorrect"]}; TimingQualified={result["TimingQualified"]}; {args.output / "results.json"}')
    return 0 if result["TimingQualified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
