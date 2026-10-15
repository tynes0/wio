"""N01 observed memcpy/memmove traffic in Wio diagnostic builds, not total copies."""
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

CASES = ("control", "string_owned", "string_view", "string_return", "text_owned", "text_view",
         "text_return", "component_owned", "component_view", "component_return", "concat",
         "text_concat", "scalar_component")
STRING = b"copy\0payload:0123456789abcdef0123456789abcdef"
TEXT = "A🌍é日本語:payload:0123456789abcdef".encode("utf-8")
METRICS = ("MemcpyCalls", "MemcpyBytes", "MemmoveCalls", "MemmoveBytes")


def hash_bytes(data):
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return result


def checksum(kind, count):
    if kind == "control":
        source, data = bytes(range(32)), bytearray(32)
        for _ in range(count):
            data[:7] = source[:7]
            data[2:13] = bytes(data[:11])
            data[:5] = bytes(data[3:8])
        return str(hash_bytes(data))
    if kind in ("concat", "text_concat"):
        return str(hash_bytes((STRING if kind == "concat" else TEXT) * count))
    value = hash_bytes(TEXT if kind.startswith("text_") else STRING)
    if kind.startswith("component_"):
        value = (value + 7) & ((1 << 64) - 1)
    result = 0
    for index in range(count):
        term = index * 2 + 1 if kind == "scalar_component" else value
        result = (result * 33 + term) & ((1 << 64) - 1)
    return str(result)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON field")
        result[key] = value
    return result


def parse(output, kind, count, enabled):
    def invalid(value):
        raise ValueError(f"Non-finite JSON: {value}")
    row = json.loads(output, object_pairs_hook=unique_object, parse_constant=invalid)
    if not isinstance(row, dict) or set(row) != set(METRICS + ("Checksum", "CountersEnabled")):
        raise ValueError("Invalid copy-counter schema")
    if type(row["CountersEnabled"]) is not bool or row["CountersEnabled"] != enabled:
        raise ValueError("Counter mode mismatch")
    if row["Checksum"] != checksum(kind, count):
        raise ValueError("Independent Wio checksum mismatch")
    for metric in METRICS:
        value = row[metric]
        if enabled:
            if type(value) is not int or not 0 <= value <= (1 << 64) - 1:
                raise ValueError(f"Invalid unsigned copy counter: {metric}")
        elif value is not None:
            raise ValueError("Counter-free metrics must be null, not zero")
    if enabled and kind == "control":
        expected = dict(MemcpyCalls=count * 2, MemcpyBytes=count * 7,
                        MemmoveCalls=count * 2, MemmoveBytes=count * 16)
        if any(row[key] != value for key, value in expected.items()):
            raise ValueError(f"Copy/move calibration failed: {row}; expected {expected}")
    return row


def checked(command, directory, timeout=120):
    row = m.run_process(command, directory, timeout)
    if row["Status"] != "passed":
        raise RuntimeError(f"{command}: {row}")
    return row


def run(args):
    if (not args.counts or len(set(args.counts)) != len(args.counts)
            or any(not 1 <= n <= 100000 for n in args.counts)):
        raise ValueError("Require unique counts in [1,100000]")
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which(args.cxx)
    if not compiler:
        raise ValueError("Native compiler not found")
    version = checked([compiler, "--version"], directory)["Output"]
    if "Free Software Foundation" not in version:
        raise ValueError("This diagnostic requires GCC and GNU-compatible --wrap linking")
    wio, runtime = args.wio.resolve(), args.runtime.resolve()
    paths = [Path(__file__), Path(m.__file__), Path(compiler), wio, runtime, HERE / "costs.wio", HERE / "probe.h"]
    paths += [p for tree in (ROOT / "runtime/include", ROOT / "sdk/include", ROOT / "std")
              for p in tree.rglob("*") if p.is_file()]
    before = m.snapshot(paths)
    for name in ("costs.wio", "probe.h"):
        shutil.copyfile(HERE / name, directory / name)
    env = dict(os.environ, WIO_ROOT=str(ROOT))
    subprocess.run([str(wio), str(directory / "costs.wio"), "--emit-cpp", "--include-dir", str(directory)],
                   env=env, check=True, timeout=120)
    generated = directory / "costs.wio.cpp"
    common = ["-std=c++20", "-O2", "-static-libstdc++", "-static-libgcc"]
    diagnostic_flags = ["-fno-inline", "-fno-builtin", "-fno-ipa-cp", "-DWIO_COPY_DIAGNOSTIC",
                        "-Wl,--wrap=memcpy", "-Wl,--wrap=memmove"]
    programs, builds = {}, {}
    for variant in ("diagnostic", "plain"):
        program = directory / (variant + (".exe" if os.name == "nt" else ""))
        flags = common + (diagnostic_flags if variant == "diagnostic" else [])
        builds[variant] = checked([
            compiler, *flags, str(generated), f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}",
            str(runtime), *(["-lws2_32", "-lshell32"] if os.name == "nt" else ["-pthread"]),
            "-o", str(program)], directory)
        programs[variant] = program
    artifacts = [directory / "costs.wio", directory / "probe.h", generated, *programs.values()]
    artifact_before = m.snapshot(artifacts)
    result = dict(Schema=1, Stage="N01", DiagnosticOnly=True, TimingQualified=False,
                  CompilerVersion=version, CommonFlags=common, DiagnosticFlags=diagnostic_flags, Builds=builds,
                  SourcesBefore=before, ArtifactsBefore=artifact_before, Samples=[], Correct=False, Failure=None,
                  Policy="Observed wrapped C library copy traffic, not all payload bytes or shipping-O2 counts",
                  Revision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  Worktree=subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True))
    try:
        for count in args.counts:
            for kind in CASES:
                for variant, program in programs.items():
                    process = checked([program, kind, str(count)], directory)
                    sample = dict(Kind=kind, Count=count, Variant=variant, Process=process, Metrics=None)
                    result["Samples"].append(sample)
                    sample["Metrics"] = parse(process["Output"], kind, count, variant == "diagnostic")
                    if variant == "diagnostic":
                        metrics = sample["Metrics"]
                        print(f'{kind}/{count}: memcpy={metrics["MemcpyBytes"]}, memmove={metrics["MemmoveBytes"]}', flush=True)
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
    p.add_argument("--cxx", default="g++")
    p.add_argument("--wio", type=Path, default=ROOT / "build/app/Debug/wio.exe")
    p.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
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
