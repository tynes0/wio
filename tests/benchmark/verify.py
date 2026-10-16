"""Only new benchmark-library/tool tests; Wio workloads, independent Python oracle.

Serial builds and bounded processes. Correctness is a test gate, noisy timings
are saved and must not turn into a retry-until-pass performance gate.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/performance"))
import measurement as m


def main(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    evidence = dict(Stage="N02-measurement-tool", Correct=False, Processes=[], Checks=[], Failure=None)
    env = dict(os.environ, WIO_ROOT=str(ROOT), DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_NOLOGO="1")
    os.environ.update({k: env[k] for k in ("WIO_ROOT", "DOTNET_CLI_TELEMETRY_OPTOUT", "DOTNET_NOLOGO")})
    cxx = shutil.which(args.cxx) or args.cxx
    suffix = ".exe" if os.name == "nt" else ""
    links = ["-lws2_32", "-lshell32", "-lpsapi", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]

    def save():
        m.write_json(folder / "results.json", evidence)

    def checked(command, accepted=(0,), timeout=600):
        row = m.run_process(command, folder, timeout)
        row["Command"] = [str(item) for item in command]
        evidence["Processes"].append(row)
        save()
        if row["ExitCode"] not in accepted or row["Status"] != ("passed" if row["ExitCode"] == 0 else "failed"):
            raise AssertionError(str(row))
        return row

    def compile_cpp(source, name, optimization="-O2", defines=()):
        executable = folder / (name + suffix)
        checked([cxx, "-std=c++20", optimization, *defines, source, f"-I{ROOT / 'runtime/include'}",
                 f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(), *links, "-o", executable])
        return executable

    try:
        before = m.snapshot([ROOT / "std/benchmark.wio", ROOT / "runtime/include/std_benchmark.h",
                             ROOT / "tools/wio-bench/main.wio", args.wio, args.runtime])
        source = folder / "library.wio"
        shutil.copyfile(ROOT / "tests/benchmark/library.wio", source)
        checked([args.wio.resolve(), source, "--emit-cpp"])
        generated = folder / "library.native.cpp"
        shutil.copyfile(folder / "library.wio.cpp", generated)
        for optimization in ("-O0", "-O2"):
            executable = compile_cpp(generated, "library" + optimization, optimization)
            checked([executable])
        bridge = folder / ("library.dll" if os.name == "nt" else "library.dylib" if sys.platform == "darwin" else "library.so")
        bytecode = folder / "library.wiob"
        checked([args.wio.resolve(), source, "--target", "shared", "--native-bridge-only", "--output", bridge,
                 "--bytecode-sidecar", bytecode])
        checked([args.wio.resolve(), "--vm-run-bytecode", bytecode, "--native-library", bridge])
        evidence["Checks"].append("Wio stats/checksum/native POD memory: native O0/O2 + VM")
        allocations = compile_cpp(ROOT / "tests/benchmark/allocations.cpp", "allocations")
        checked([allocations])
        # Same Wio behavior fixture in a separate instrumented executable.
        instrumented = compile_cpp(generated, "instrumented", defines=["-DWIO_BENCHMARK_ALLOCATIONS"])
        checked([instrumented])
        evidence["Checks"].append("Allocation ordinary/aligned/zero/failure/pre-window/live/reset calibration")
        child = compile_cpp(ROOT / "tests/benchmark/child.cpp", "child")

        def capture(mode, *extra, timeout=2000, limit=4096, program=None):
            row = checked([child, "execute", program or child, timeout, limit, mode, *extra])
            return json.loads(row["Output"])

        quoted = 'spaces "quotes" slash\\ and ;$() are literal'
        assert capture("echo", quoted)["stdout"] == quoted
        assert capture("bytes")["stdout"].encode("latin1") == b"a\0b\xff"
        assert capture("sleep", timeout=25)["status"] == "timeout"
        assert capture("flood", limit=1000)["status"] == "output-limit"
        failed = capture("fail")
        assert failed["status"] == "nonzero-exit" and failed["exitCode"] == 7 and "expected failure" in failed["stderr"]
        assert capture("missing", program=folder / "missing-executable")["status"] in ("launch-failed", "nonzero-exit")
        memory = capture("protocol")
        assert memory["cpuNanoseconds"] is None or memory["cpuNanoseconds"] >= 0
        assert memory["peakResidentBytes"] is None or memory["peakResidentBytes"] > 0
        checked([child, "cleanup"])
        evidence["Checks"].append("Shell-free argv/NUL capture, timeout, output cap, nonzero/launch failures, CPU/RSS and descendant cleanup before outer-worker exit")

        bootstrap = ROOT / "tools/wio-bench/bootstrap.py"
        prepared = folder / "prepared"
        command = [sys.executable, bootstrap, "--output", prepared, "--wio", args.wio.resolve(),
                   "--runtime", args.runtime.resolve(), "--cxx", cxx, "--tool-opt=-O0", "--example",
                   "--vm-host", args.vm_host.resolve(), "--vm-config", args.vm_config, "--count", "2000", "--repetitions", "3"]
        if args.runner:
            command += ["--runner", args.runner.resolve()]
        checked(command)
        runner = args.runner.resolve() if args.runner else prepared / "tool" / ("wio-bench" + suffix)
        checked([runner, "--help"])
        checked([runner, "--version"])
        cases = [dict(name=kind, implementation=kind, workload="protocol-test", operations=10, expectedChecksum="42",
                      program=str(child), args=["protocol"], cwd=str(folder), metric="kernel")
                 for kind in ("wio-native", "wio-vm", "cpp", "csharp")]
        manifest = dict(schemaVersion=1, name="Protocol contract, NOT cross-language performance", repetitions=3,
                        finalRepetitions=3, warmups=1, artifacts=[str(child)], cases=cases)

        def write(name, value):
            target = folder / (name + ".json")
            m.write_json(target, value)
            return target

        valid = write("valid", manifest)
        checked([runner, "validate", valid])
        for key, value in (("warmups", -1), ("repetitions", "three"), ("cpu", -9), ("maximumRange", 0), ("outputLimit", 0)):
            bad = copy.deepcopy(manifest)
            bad[key] = value
            checked([runner, "validate", write("bad-" + key, bad)], (1,))
        bad = copy.deepcopy(manifest)
        bad["cases"].pop()
        checked([runner, "validate", write("missing-csharp", bad)], (1,))
        bad = copy.deepcopy(manifest)
        bad["cases"][1]["expectedChecksum"] = "different"
        checked([runner, "validate", write("different-workload", bad)], (1,))
        result_directory = folder / "protocol"
        checked([runner, "run", valid, "--output", result_directory], (0, 2))
        data = json.loads((result_directory / "results.json").read_text())
        assert data["complete"] and data["outputsCorrect"] and data["artifactsUnchanged"]
        assert len(data["samples"]) == 32 and len(data["summary"]) == 6
        for summary in data["summary"]:
            assert summary["statistics"]["median"] == 1000000 and summary["statistics"]["standardDeviation"] == 0
        for artifact in data["artifactsBefore"]:
            assert artifact["sha256"] == hashlib.sha256(Path(artifact["path"]).read_bytes()).hexdigest()
        assert {s["implementation"] for s in data["samples"] if s["phase"] == "final"} == {"wio-native", "wio-vm"}
        existing = (result_directory / "results.json").read_bytes()
        checked([runner, "run", valid, "--output", result_directory], (1,))
        assert (result_directory / "results.json").read_bytes() == existing
        checked([runner, "report", result_directory / "results.json"])
        checked([runner, "compare", result_directory / "results.json", result_directory / "results.json"], (0, 2))
        bad = copy.deepcopy(manifest)
        bad["cases"][0]["args"] = ["wrong"]
        negative_directory = folder / "bad-output"
        checked([runner, "run", write("bad-checksum", bad), "--output", negative_directory], (1,))
        rejected = json.loads((negative_directory / "results.json").read_text())
        assert rejected["complete"] and not rejected["outputsCorrect"] and not rejected["timingQualified"]
        assert len(rejected["samples"]) == 32 and any(not s["correct"] for s in rejected["samples"])
        evidence["Checks"].append("Wio runner strict manifests, 32 sample/final/control records, independent oracle, checkpoints, no overwrite and failure retention")

        previous_root = os.environ["WIO_ROOT"]
        try:
            os.environ["WIO_ROOT"] = str(folder / "deliberately-wrong-installed-root")
            checked([runner, "run", prepared / "compute.bench.json", "--output", folder / "cross-language"], (0, 2), timeout=900)
        finally:
            os.environ["WIO_ROOT"] = previous_root
        actual = json.loads((folder / "cross-language/results.json").read_text())
        assert actual["complete"] and actual["outputsCorrect"] and len(actual["samples"]) == 32
        state, expected = 12345, 0
        for _ in range(2000):
            state = state * 48271 % 2147483647
            expected += state
        assert all(s["payload"]["checksum"] == str(expected) and s["payload"]["operations"] == 2000 for s in actual["samples"])
        evidence["Checks"].append("Actual Wio native/VM + C++ O2 + C# Release, independent Python checksum, final native/VM rerun")
        evidence["CrossLanguageReport"] = str(folder / "cross-language/results.json")
        evidence["ArtifactsUnchanged"] = before == m.snapshot([Path(row["Path"]) for row in before])
        assert evidence["ArtifactsUnchanged"]
        evidence["Correct"] = True
    except Exception as exc:
        evidence["Failure"] = str(exc)
        raise
    finally:
        save()
        print(json.dumps({key: evidence[key] for key in ("Correct", "Checks", "Failure")}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--vm-host", type=Path, required=True)
    parser.add_argument("--vm-config", default="unspecified")
    parser.add_argument("--runner", type=Path)
    main(parser.parse_args())
