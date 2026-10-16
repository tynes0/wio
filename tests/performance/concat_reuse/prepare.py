"""Prepare frozen N02 artifacts; the Wio controller owns all timing samples."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def expected(mode, count):
    a = b"copy\0payload:0123456789abcdef0123456789abcdef"
    b = b"second:0123456789abcdef0123456789abcdef"
    c = b"third:0123456789abcdef0123456789abcdef"
    ta = "A🌍é日本語:payload:0123456789abcdef".encode()
    tb = "B🌙é日本語:payload:0123456789abcdef".encode()
    tc = "C⭐é日本語:payload:0123456789abcdef".encode()
    data = a * count if mode == 0 else ta * count if mode == 1 else (a + b + c) * 2 if mode == 2 else (ta + tb + tc) * 2
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return str(result if mode < 2 or count % 2 else 0)


def prepare(args, fixture=None):
    # Other optimization fixtures share the serial build/manifest pipeline, not
    # the workloads or checksum oracle. The original N02 command stays identical.
    fixture = fixture or dict(files=("main.wio", "probe.h", "main.cpp", "Program.cs", "Concat.csproj"),
                              project="Concat.csproj", assembly="Concat", stage="n02", title="N02 concat",
                              cases=("assignment-string", "assignment-text", "chain-string", "chain-text"),
                              expected=expected,
                              contracts="UTF8 hash/NUL; C# UTF16 strings + UTF8 hash allocation; no reserve; two warmups")
    folder = args.output.resolve()
    if not args.reuse_build:
        folder.mkdir(parents=True, exist_ok=False)
        for name in fixture["files"]:
            shutil.copyfile(args.sources.resolve() / name, folder / name)
    cxx = Path(shutil.which(args.cxx) or args.cxx).resolve()
    dotnet = Path(shutil.which("dotnet")).resolve()
    wio, runtime, vm = args.wio.resolve(), args.runtime.resolve(), args.vm.resolve()
    suffix = ".exe" if os.name == "nt" else ""
    links = ["-lws2_32", "-lshell32", "-lpsapi", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    flags = ["-std=c++20", "-O2", "-ffp-contract=off"]
    native, cpp = folder / ("native" + suffix), folder / ("cpp" + suffix)
    diagnostic = folder / ("memory" + suffix)
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    commands = []

    def run(command):
        if args.reuse_build:
            return
        command = [str(x) for x in command]
        row = dict(command=command)
        commands.append(row)
        result = subprocess.run(command, cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=600)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "build.json").write_text(json.dumps(commands, indent=2) + "\n", encoding="utf-8")
        print(result.stdout, end="", flush=True)
        print(result.stderr, end="", flush=True)
        if result.returncode:
            raise RuntimeError("Build failed: retained build.json")

    run([wio, folder / "main.wio", "--emit-cpp"])
    generated = folder / "frozen.cpp"
    if not args.reuse_build:
        shutil.copyfile(folder / "main.wio.cpp", generated)
    for target, extra in ((native, []), (diagnostic, ["-DWIO_BENCHMARK_ALLOCATIONS"])):
        run([cxx, *flags, *extra, generated, f"-I{folder}", f"-I{ROOT / 'runtime/include'}",
             f"-I{ROOT / 'sdk/include'}", runtime, *links, "-o", target])
    run([cxx, *flags, f"-I{folder}", folder / "main.cpp", "-o", cpp])
    run([dotnet, "build", folder / fixture["project"], "-c", "Release", "-o", folder / "csharp", "--nologo"])
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
         "--output", bridge, "--bytecode-sidecar", bytecode])
    cases = []
    memory = []
    for mode, name in enumerate(fixture["cases"]):
        common = dict(workload=fixture["stage"] + "-" + name + "-v1", metric="kernel", operations=args.count,
                      expectedChecksum=fixture["expected"](mode, args.count), cwd=str(folder))
        for implementation, program, argv in (
            ("wio-native", native, [mode, args.count]),
            ("wio-vm", vm, [bytecode, "--native-library", bridge, "--", mode, args.count]),
            ("cpp", cpp, [mode, args.count]),
            ("csharp", dotnet, [folder / ("csharp/" + fixture["assembly"] + ".dll"), mode, args.count])):
            cases.append(dict(name=name + "-" + implementation, implementation=implementation,
                              program=str(program), args=[str(x) for x in argv], **common))
        record = subprocess.run([str(diagnostic), str(mode), str(args.count), "memory"], cwd=folder,
                                capture_output=True, text=True, timeout=60)
        row = dict(mode=name, exitCode=record.returncode, stdout=record.stdout, stderr=record.stderr)
        memory.append(row)
        (folder / "memory.json").write_text(json.dumps(memory, indent=2) + "\n")
        if record.returncode:
            raise RuntimeError("Memory diagnostic failed; raw result retained")
        lines = [line for line in record.stdout.splitlines() if line]
        if len(lines) != 2 or json.loads(lines[0])["checksum"] != common["expectedChecksum"]:
            raise RuntimeError("Allocation checksum mismatch")
    artifacts = [p for p in folder.iterdir() if p.is_file() and p.suffix != ".json"]
    artifacts += [wio, runtime, vm, cxx, dotnet, args.runner.resolve(), folder / ("csharp/" + fixture["assembly"] + ".dll"),
                  folder / ("csharp/" + fixture["assembly"] + ".runtimeconfig.json"), ROOT / "std/benchmark.wio",
                  ROOT / "runtime/include/std_benchmark.h", ROOT / "runtime/include/text.h",
                  ROOT / "runtime/include/wio_benchmark_allocations.h", ROOT / "runtime/include/benchmark_memory.h",
                  ROOT / "std/json.wio", ROOT / "std/serialization.wio", ROOT / "runtime/include/std_serialization.h"]
    artifacts += [ROOT / "compiler/src/wio_compiler/codegen/wir_cpp_storage_plan.cpp",
                  ROOT / "compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp",
                  ROOT / "compiler/include/wio/codegen/wir_cpp_storage_plan.h"]
    if (ROOT / "runtime/include/string_concat.h").is_file():
        artifacts.append(ROOT / "runtime/include/string_concat.h")
    for extra in fixture.get("artifacts", ()):
        if (ROOT / extra).is_file():
            artifacts.append(ROOT / extra)
    manifest = dict(schemaVersion=1, name=fixture["title"] + " " + args.label, warmups=1,
                    repetitions=args.repetitions, finalRepetitions=3, timeoutMs=60000, outputLimit=1048576, cpu=args.cpu,
                    environment=dict(WIO_ROOT=str(ROOT), DOTNET_NOLOGO="1"),
                    maximumRange=.15, maximumDrift=.10, maximumControlDifference=.10,
                    metadata=dict(label=args.label, nativeFlags=flags, vm="Release host; unchanged bytecode semantics",
                                  contracts=fixture["contracts"],
                                  revision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()),
                    versions=[dict(program=str(program), args=argv, cwd=str(folder))
                              for program, argv in ((wio, ["--version"]), (cxx, ["--version"]), (dotnet, ["--info"]))],
                    builds=[], artifacts=[str(p) for p in artifacts], cases=cases)
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(folder / "manifest.json")


def arguments(default_sources=HERE):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--sources", type=Path, default=default_sources, help="Frozen workload source directory for an exact-input rebuild")
    p.add_argument("--reuse-build", action="store_true", help="Explicitly finish an existing frozen build; no compilation")
    p.add_argument("--wio", type=Path, default=ROOT / "build/app/Debug/wio.exe")
    p.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
    p.add_argument("--vm", type=Path, default=ROOT / "build/app/Release/wio-vm.exe")
    p.add_argument("--runner", type=Path, default=ROOT / "build/tools/wio-bench/Debug/wio-bench.exe")
    p.add_argument("--cxx", default="g++" if os.name == "nt" else "c++")
    p.add_argument("--count", type=int, default=2001)
    p.add_argument("--repetitions", type=int, default=5)
    p.add_argument("--cpu", type=int, default=2)
    parsed = p.parse_args()
    if not 1 <= parsed.count <= 100000 or parsed.repetitions < 3:
        p.error("Invalid count/repetitions")
    return parsed


if __name__ == "__main__":
    prepare(arguments())
