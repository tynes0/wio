"""Bootstrap the Wio-written tool and generate a four-implementation example.

This script does NOT collect benchmark samples: wio-bench owns that workflow.
All builds are serial. No downloads, shell commands, system-policy changes or PS1.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def fingerprint(path):
    path = Path(path).resolve()
    return dict(path=str(path), bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def prepare(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    cxx = Path(shutil.which(args.cxx) or args.cxx).resolve()
    wio, runtime = args.wio.resolve(), args.runtime.resolve()
    records = []
    env = dict(os.environ, WIO_ROOT=str(ROOT), DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_NOLOGO="1")

    def run(command, cwd):
        row = dict(command=[str(a) for a in command], cwd=str(cwd))
        records.append(row)
        try:
            result = subprocess.run(row["command"], cwd=cwd, env=env, text=True, encoding="utf-8",
                                    errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
            row.update(exitCode=result.returncode, output=result.stdout)
            print(result.stdout, end="", flush=True)
            if result.returncode:
                raise RuntimeError("Bootstrap failed; see bootstrap.json")
        finally:
            (output / "bootstrap.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")

    suffix = ".exe" if os.name == "nt" else ""
    flags = ["-std=c++20", args.tool_opt, "-ffp-contract=off"]
    links = ["-lws2_32", "-lshell32", "-lpsapi", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    if args.runner:
        runner = args.runner.resolve()
    else:
        folder = output / "tool"
        folder.mkdir()
        shutil.copyfile(HERE / "main.wio", folder / "main.wio")
        run([wio, folder / "main.wio", "--emit-cpp"], folder)
        runner = folder / ("wio-bench" + suffix)
        run([cxx, *flags, *(["-static"] if os.name == "nt" else []), folder / "main.wio.cpp", f"-I{ROOT / 'runtime/include'}",
             f"-I{ROOT / 'sdk/include'}", runtime, *links, "-o", runner], folder)
    (output / "tool.json").write_text(json.dumps(dict(runner=fingerprint(runner), compiler=fingerprint(wio),
        runtime=fingerprint(runtime), cxx=fingerprint(cxx), toolFlags=flags, vmHost=str(args.vm_host)), indent=2) + "\n")
    if not args.example:
        print(runner)
        return runner, None
    if not args.vm_host or not args.vm_host.is_file():
        raise ValueError("--example requires --vm-host (prefer a Release wio-vm runner)")
    dotnet = shutil.which(args.dotnet)
    if not dotnet:
        raise ValueError(".NET 9 SDK is required; no comparison will be silently skipped")
    folder = output / "compute"
    folder.mkdir()
    for name in ("main.wio", "main.cpp", "Program.cs", "Compute.csproj"):
        shutil.copyfile(HERE / "examples/compute" / name, folder / name)
    (folder / "csharp").mkdir()
    native = folder / ("wio-native" + suffix)
    cpp = folder / ("cpp" + suffix)
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    vm = args.vm_host.resolve()

    def command(program, arguments, **extra):
        return dict(program=str(program), args=[str(a) for a in arguments], cwd=str(folder), **extra)

    builds = [command(wio, [folder / "main.wio", "--emit-cpp"], timeoutMs=300000),
              command(cxx, ["-std=c++20", "-O2", "-ffp-contract=off", folder / "main.wio.cpp",
                      f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", runtime, *links, "-o", native], timeoutMs=600000),
              command(cxx, ["-std=c++20", "-O2", "-ffp-contract=off", folder / "main.cpp", "-o", cpp], timeoutMs=300000),
              command(dotnet, ["build", folder / "Compute.csproj", "-c", "Release", "-o", folder / "csharp", "--nologo"], timeoutMs=300000),
              command(wio, [folder / "main.wio", "--target", "shared", "--native-bridge-only", "--output", bridge,
                            "--bytecode-sidecar", bytecode], timeoutMs=300000)]
    state, checksum = 12345, 0
    for _ in range(args.count):
        state = state * 48271 % 2147483647
        checksum += state
    shared = dict(workload="integer-recurrence-v1", operations=args.count, expectedChecksum=str(checksum), metric="kernel")
    cases = [command(native, [args.count], name="wio-native", implementation="wio-native", **shared),
             command(vm, [bytecode, "--native-library", bridge, "--", args.count], name="wio-vm", implementation="wio-vm", **shared),
             command(cpp, [args.count], name="cpp", implementation="cpp", **shared),
             command(dotnet, [folder / "csharp/Compute.dll", args.count], name="csharp", implementation="csharp", **shared)]
    artifacts = [wio, cxx, runtime, vm, Path(dotnet), runner, native, cpp, bridge, bytecode,
                 folder / "csharp/Compute.dll", folder / "csharp/Compute.runtimeconfig.json",
                 folder / "main.wio", folder / "main.cpp", folder / "Program.cs", folder / "Compute.csproj",
                 ROOT / "std/benchmark.wio", ROOT / "runtime/include/std_benchmark.h",
                 ROOT / "runtime/include/benchmark_memory.h", HERE / "main.wio"]
    artifacts += [ROOT / "std/json.wio", ROOT / "std/serialization.wio", ROOT / "runtime/include/std_serialization.h"]
    if args.with_python:
        python_image = Path(sys.executable)
        if os.name == "nt":
            # Microsoft Store sys.executable can be a zero-byte App Execution
            # Alias: executable, but not hashable as a PE image. Freeze the
            # actual loaded interpreter instead, never silently skip hashing.
            buffer = ctypes.create_unicode_buffer(32768)
            length = ctypes.windll.kernel32.GetModuleFileNameW(None, buffer, len(buffer))
            if not length or length >= len(buffer):
                raise RuntimeError("Cannot resolve the actual Python interpreter image")
            python_image = Path(buffer.value)
        shutil.copyfile(HERE / "examples/compute/main.py", folder / "main.py")
        cases.append(command(python_image, [folder / "main.py", args.count], name="python", implementation="python", **shared))
        artifacts += [python_image, folder / "main.py"]
    manifest = dict(schemaVersion=1, name="Wio benchmark example: integer recurrence", warmups=1,
                    environment=dict(WIO_ROOT=str(ROOT), DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_NOLOGO="1"),
                    repetitions=args.repetitions, finalRepetitions=3, timeoutMs=30000, outputLimit=1048576, cpu=args.cpu,
                    maximumRange=0.15, maximumDrift=0.10, maximumControlDifference=0.10,
                    metadata=dict(nativeFlags="C++20 O2 ffp-contract=off", csharp="net9.0 Release; tiering disabled; two in-process warmups",
                                  oracle="Independent Python integer arithmetic; same runtime argument in all four programs",
                                  vmHostConfiguration=args.vm_config),
                    versions=[command(wio, ["--version"]), command(cxx, ["--version"]), command(dotnet, ["--info"])],
                    builds=builds, artifacts=[str(p) for p in artifacts], cases=cases)
    if args.with_python:
        manifest["versions"].append(command(python_image, ["--version"]))
    manifest_path = output / "compute.bench.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Runner: {runner}\nManifest: {manifest_path}\nRun: {runner} run {manifest_path} --output NEW_DIRECTORY")
    return runner, manifest_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--wio", type=Path, default=ROOT / "build/app/Debug/wio.exe")
    parser.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
    parser.add_argument("--cxx", default="g++" if os.name == "nt" else "c++")
    parser.add_argument("--dotnet", default="dotnet")
    parser.add_argument("--runner", type=Path, help="Reuse an explicitly supplied tool executable")
    parser.add_argument("--tool-opt", choices=("-O0", "-O1", "-O2"), default="-O1")
    parser.add_argument("--example", action="store_true")
    parser.add_argument("--with-python", action="store_true", help="Add an optional fifth implementation to the example")
    parser.add_argument("--vm-host", type=Path)
    parser.add_argument("--vm-config", default="unspecified")
    parser.add_argument("--count", type=int, default=20000)
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--cpu", type=int, default=-1)
    args = parser.parse_args()
    if not 1 <= args.count <= 1000000 or not 3 <= args.repetitions <= 10000:
        parser.error("Invalid count/repetitions")
    prepare(args)
