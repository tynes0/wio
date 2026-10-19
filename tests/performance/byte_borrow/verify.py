"""Closed byte-array native borrows: emitted ABI, semantics, fallback and heap window."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    for name in ("main.wio", "probe.h"):
        shutil.copyfile(HERE / name, folder / name)
    rows = []

    def run(command, expected=0, contains=None, known_gap=None):
        row = dict(command=[str(x) for x in command], expectedExitCode=expected)
        if known_gap:
            row["expectedKnownGap"] = known_gap
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode != expected or (contains and contains not in result.stdout + result.stderr):
            raise RuntimeError(str(row))

    wio = args.wio.resolve()
    run([wio, folder / "main.wio", "--emit-cpp", "--include-dir", folder])
    generated = (folder / "main.wio.cpp").read_text(encoding="utf-8")
    if "InlinePlace<std::vector<std::uint8_t>>" not in generated or "_borrow(const std::vector<std::uint8_t>&" not in generated:
        raise RuntimeError("Closed byte cell/private borrow entry was not emitted")
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, "-ffp-contract=off", folder / "main.wio.cpp",
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}",
             args.runtime.resolve(), *links, "-o", program])
        run([program], contains="byte-borrow-ok")
        run([program, "indirect"], contains="byte-borrow-indirect-ok")
        run([program, "fail"], expected=1, contains="byte-borrow-failure")
    # A Unix executable named "memory" in an -I directory shadows <memory>.
    diagnostic = folder / ("allocation-probe.exe" if os.name == "nt" else "allocation-probe")
    static = ["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else []
    run([args.cxx, "-std=c++20", "-O2", "-ffp-contract=off", "-DWIO_BENCHMARK_ALLOCATIONS", *static,
         folder / "main.wio.cpp", f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}",
         args.runtime.resolve(), *links, "-o", diagnostic])
    run([diagnostic, "memory"], contains="byte-borrow-memory-ok")
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    for optimization in ("-O0", "-O2"):
        run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
             "--output", bridge, "--bytecode-sidecar", bytecode, "--backend-arg=" + optimization,
             "--backend-arg=-ffp-contract=off"])
        run([args.vm.resolve(), bytecode, "--native-library", bridge], contains="byte-borrow-ok")
        run([args.vm.resolve(), bytecode, "--native-library", bridge, "--", "indirect"], expected=1,
            contains="WVM1003", known_gap="KI-009 indirect native function value; not fixed by this optimization")
        run([args.vm.resolve(), bytecode, "--native-library", bridge, "--", "fail"], expected=1,
            contains="byte-borrow-failure")
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print(folder / "results.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--vm", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
