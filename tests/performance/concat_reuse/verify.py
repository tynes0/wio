"""New N02 concat behavior: actual Wio source, native O0/O2 and VM."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    for name in ("semantics.wio",):
        shutil.copyfile(HERE / name, folder / "main.wio")
    rows = []

    def run(command):
        result = subprocess.run([str(a) for a in command], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        rows.append(dict(command=[str(a) for a in command], exitCode=result.returncode,
                         stdout=result.stdout, stderr=result.stderr))
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(str(rows[-1]))

    wio = args.wio.resolve()
    run([wio, folder / "main.wio", "--emit-cpp"])
    generated = folder / "main.wio.cpp"
    source = generated.read_text(encoding="utf-8")
    for marker in ("Wio local append reuse", "Wio transactional concat reuse", "wio::runtime::Concat(", "std::move("):
        if marker not in source:
            raise RuntimeError("Expected codegen path not exercised: " + marker)
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, "-ffp-contract=off", generated,
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--output", bridge,
         "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    # A mutating RHS must not accidentally acquire a new in-place fast path.
    # The pre-existing native/VM borrowed-LHS divergence is recorded explicitly;
    # this native-only compatibility test is not presented as parity evidence.
    shutil.copyfile(HERE / "barrier.wio", folder / "barrier.wio")
    run([wio, folder / "barrier.wio", "--emit-cpp"])
    barrier = folder / "barrier.wio.cpp"
    if "Wio local append reuse" in barrier.read_text(encoding="utf-8"):
        raise RuntimeError("Mutating RHS call incorrectly fused")
    for optimization in ("-O0", "-O2"):
        program = folder / ("barrier.exe" if os.name == "nt" else "barrier")
        run([args.cxx, "-std=c++20", optimization, barrier, f"-I{ROOT / 'runtime/include'}",
             f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(), *links, "-o", program])
        run([program])
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("N02 concat: native O0/O2, transactional/owned paths and VM passed")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--wio", type=Path, required=True)
    p.add_argument("--runtime", type=Path, required=True)
    p.add_argument("--cxx", required=True)
    p.add_argument("--output", type=Path, required=True)
    verify(p.parse_args())
