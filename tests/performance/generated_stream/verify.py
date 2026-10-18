"""Actual generated-helper correctness and allocation proofs, including native-POD VM dispatch."""
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
    for source, destination in (("semantics.wio", "main.wio"), ("probe.h", "probe.h"), ("helpers.cpp", "helpers.cpp")):
        shutil.copyfile(HERE / source, folder / destination)
    rows = []

    def run(command, expected=0, contains=None):
        row = dict(command=[str(item) for item in command], expectedExitCode=expected)
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode != expected or (contains and contains not in result.stdout + result.stderr):
            raise RuntimeError(str(row))

    wio = args.wio.resolve()
    run([wio, folder / "main.wio", "--emit-cpp", "--include-dir", folder])
    generated = folder / "main.wio.cpp"
    source = generated.read_text(encoding="utf-8")
    if ("return std::move(stream).str();" not in source or "wio::wir_backend::format_single(" not in source
            or "wio::wir_backend::append_formatted_literal_tail(" not in source):
        raise RuntimeError("New generated stream/single-operand path not emitted")
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, generated, f"-I{folder}", f"-I{ROOT / 'runtime/include'}",
             f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(), *links, "-o", program])
        run([program])
        run([program, "builtin"])
        run([program, "throw"], expected=1, contains="generated stream hook failure")
    helper = folder / ("helpers.exe" if os.name == "nt" else "helpers")
    static = ["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else []
    run([args.cxx, "-std=c++20", "-O2", *static, folder / "helpers.cpp", f"-I{folder}",
         f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(), *links, "-o", helper])
    run([helper])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
         "--output", bridge, "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge, "--", "builtin"])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge, "--", "throw"],
        expected=1, contains="generated stream hook failure")
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("Generated stream: native O0/O2, actual helper allocations/snapshot/seek/partial/throw; VM builtins and POD native formatter dispatch pass")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
