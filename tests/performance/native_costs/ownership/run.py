"""N01 diagnostic RC attribution from real Wio programs, not a speed benchmark."""
from __future__ import annotations

import argparse
import gzip
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE.parents[1]))
import measurement as m


def checked(command, directory, timeout=120):
    row = m.run_process(command, directory, timeout)
    if row["Status"] != "passed":
        raise RuntimeError(f"{command}: {row}")
    return row


def functions(document, header):
    matches = [entry for entry in document["files"]
               if Path(entry["file"]).resolve() == header.resolve()]
    if len(matches) != 1:
        raise ValueError("Expected exactly one ref.h coverage entry")
    rows = matches[0]["functions"]
    names = [row["name"] for row in rows]
    invalid_counts = any(type(row["execution_count"]) is not int or row["execution_count"] < 0 for row in rows)
    if len(names) != len(set(names)) or invalid_counts:
        raise ValueError("Invalid or duplicate coverage function counters")
    return rows


def summarize(rows):
    # GCC's JSON preserves template instances separately. Include all instances,
    # not just the source line's merged/max execution count.
    result = {}
    for method in ("IncStrong", "DecStrong", "IncWeak", "DecWeak", "TryIncStrong", "DestroyInternal"):
        selected = [r for r in rows if f"RefCountedObject::{method}(" in r["demangled_name"]]
        if len(selected) != 1:
            raise ValueError(f"Missing/ambiguous coverage function: {method}")
        result[method] = selected[0]["execution_count"]
    return result


def transfers(rows):
    patterns = dict(CopyConstruct=r"::Ref\(.* const&\)$", MoveConstruct=r"::Ref\(.*&&\)$",
                    CopyAssign=r"::operator=\(.* const&\)$", MoveAssign=r"::operator=\(.*&&\)$")
    return {key: sum(r["execution_count"] for r in rows
                     if r["demangled_name"].startswith("wio::runtime::Ref<")
                     and re.search(pattern, r["demangled_name"])) for key, pattern in patterns.items()}


def expected_checksum(kind, count):
    value = 0
    for index in range(count):
        value = (value * 33 + index + (kind in ("weak", "control"))) & ((1 << 64) - 1)
    return str(value)


def run(args):
    if (not args.counts or len(set(args.counts)) != len(args.counts)
            or any(not 1 <= n <= 100000 for n in args.counts)):
        raise ValueError("Unique counts in [1,100000] are required")
    if any(os.environ.get(key) for key in ("GCOV_PREFIX", "GCOV_PREFIX_STRIP")):
        raise ValueError("GCOV_PREFIX overrides are incompatible with isolated profile files")
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which(args.cxx)
    gcov = shutil.which(args.gcov)
    if not compiler or not gcov:
        raise ValueError("A matching GCC/gcov pair is required")
    wio, runtime = args.wio.resolve(), args.runtime.resolve()
    paths = [Path(__file__), Path(m.__file__), wio, runtime, Path(compiler), Path(gcov),
             HERE / "costs.wio", HERE / "ownership.h"]
    paths += [p for tree in (ROOT / "runtime/include", ROOT / "sdk/include", ROOT / "std")
              for p in tree.rglob("*") if p.is_file()]
    before = m.snapshot(paths)
    version = checked([compiler, "-dumpfullversion", "-dumpversion"], directory)["Output"].strip()
    gcov_version = checked([gcov, "--version"], directory)["Output"].splitlines()[0]
    if not re.search(r"\b" + re.escape(version) + r"\b", gcov_version):
        raise ValueError("GCC and gcov versions must match exactly")
    for name in ("costs.wio", "ownership.h"):
        shutil.copyfile(HERE / name, directory / name)
    env = dict(os.environ, WIO_ROOT=str(ROOT))
    subprocess.run([str(wio), str(directory / "costs.wio"), "--emit-cpp", "--include-dir", str(directory)],
                   env=env, check=True, timeout=120)
    generated = directory / "costs.wio.cpp"
    # Generated #line directives can make gcov function ranges non-monotonic.
    # Strip only those directives in a separate diagnostic copy, never in output
    # used by the normal compiler or any timing experiment.
    diagnostic = directory / "diagnostic.cpp"
    diagnostic.write_text(re.sub(r'^\s*#line[^\n]*', '', generated.read_text(encoding="utf-8"),
                                 flags=re.MULTILINE), encoding="utf-8")
    flags = ["-std=c++20", "-O2", "-fno-inline", "--coverage", "-fprofile-update=atomic", "-DWIO_COST_COVERAGE"]
    program = directory / ("ownership.exe" if os.name == "nt" else "ownership")
    checked([compiler, *flags, str(diagnostic), f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}",
             str(runtime), *(["-lws2_32", "-lshell32"] if os.name == "nt" else ["-pthread"]),
             "-o", str(program)], directory)
    plain = directory / ("plain.exe" if os.name == "nt" else "plain")
    checked([compiler, "-std=c++20", "-O2", str(generated), f"-I{ROOT / 'runtime/include'}",
             f"-I{ROOT / 'sdk/include'}", str(runtime),
             *(["-lws2_32", "-lshell32"] if os.name == "nt" else ["-pthread"]),
             "-o", str(plain)], directory)
    notes = list(directory.glob("*.gcno"))
    if len(notes) != 1:
        raise ValueError("Expected one coverage notes file")
    artifact_paths = [directory / "costs.wio", directory / "ownership.h", generated, diagnostic, program, plain, notes[0]]
    artifacts = m.snapshot(artifact_paths)
    result = dict(Schema=1, Stage="N01", DiagnosticOnly=True, TimingQualified=False, Flags=flags,
                  CompilerVersion=version, GcovVersion=gcov_version,
                  SourcesBefore=before, ArtifactsBefore=artifacts, Samples=[], Failure=None, Correct=False,
                  PlainFlags=["-std=c++20", "-O2"],
                  Policy="RC/Ref transfer events only; no copied-payload bytes, no timing claim")
    try:
        for count in args.counts:
            for kind in ("owned", "view", "return", "weak", "control"):
                # Fresh process, reset window, and fresh gcda per case: never
                # aggregate profiles across counts or include startup/exit.
                for data in directory.glob("*.gcda"):
                    data.unlink()
                row = checked([program, kind, str(count)], directory)
                plain_row = checked([plain, kind, str(count)], directory)
                if row["Output"] != f"checksum={expected_checksum(kind, count)}":
                    raise ValueError("Wio checksum mismatch")
                if plain_row["Output"] != row["Output"]:
                    raise ValueError("Diagnostic/normal O2 checksum mismatch")
                coverage = checked([gcov, "--json-format", str(notes[0])], directory)
                reports = list(directory.glob("*.gcov.json.gz"))
                if len(reports) != 1:
                    raise ValueError("Expected one gcov JSON report")
                with gzip.open(reports[0], "rt", encoding="utf-8") as stream:
                    document = json.load(stream)
                if document["gcc_version"] != version:
                    raise ValueError("Coverage notes/compiler version mismatch")
                rows = functions(document, ROOT / "runtime/include/ref.h")
                events = summarize(rows)
                moves = transfers(rows)
                sample = dict(Kind=kind, Count=count, Checksum=expected_checksum(kind, count),
                              Events=events, Transfers=moves, Functions=rows,
                              Process=row, PlainProcess=plain_row, Gcov=coverage)
                result["Samples"].append(sample)
                shutil.copyfile(reports[0], directory / f"{kind}-{count}.coverage.json.gz")
                if kind == "control":
                    known = dict(IncStrong=3, DecStrong=4, IncWeak=1, DecWeak=2, TryIncStrong=2, DestroyInternal=1)
                    if events != {key: n * count for key, n in known.items()}:
                        raise ValueError("Coverage RC calibration failed")
                    if moves != {key: count for key in moves}:
                        raise ValueError("Coverage copy/move calibration failed")
                if kind != "control" and events["DestroyInternal"] != count:
                    raise ValueError("Expected exactly one physical destruction per iteration")
                if kind in ("owned", "view", "return") and events["IncStrong"] != events["DecStrong"]:
                    raise ValueError("Unbalanced strong references in the completed Wio kernel")
                print(kind, count, events, moves, flush=True)
        result["Correct"] = True
    except Exception as exc:
        result["Failure"] = str(exc)
    finally:
        result["SourcesAfter"] = m.snapshot(paths)
        result["ArtifactsAfter"] = m.snapshot(artifact_paths)
        result["SourcesUnchanged"] = before == result["SourcesAfter"]
        result["ArtifactsUnchanged"] = artifacts == result["ArtifactsAfter"]
        result["Correct"] &= result["SourcesUnchanged"] and result["ArtifactsUnchanged"]
        if not result["SourcesUnchanged"] or not result["ArtifactsUnchanged"]:
            result["Failure"] = result["Failure"] or "Sources or immutable artifacts changed during collection"
        m.write_json(directory / "results.json", result)
    return result


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--cxx", default="g++")
    p.add_argument("--gcov", default="gcov")
    p.add_argument("--wio", type=Path, default=ROOT / "build/app/Debug/wio.exe")
    p.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
    p.add_argument("--counts", nargs="+", type=int, default=[1, 17, 2000])
    return p


if __name__ == "__main__":
    options = parser().parse_args()
    try:
        outcome = run(options)
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(str(exc), file=sys.stderr)
        if options.output.is_dir():
            m.write_json(options.output / "failure.json", dict(Correct=False, Failure=str(exc)))
        raise SystemExit(2)
    if not outcome["Correct"]:
        print(outcome["Failure"], file=sys.stderr)
    raise SystemExit(0 if outcome["Correct"] else 1)
