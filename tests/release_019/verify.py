"""Interim-release correctness gate: operand snapshots, native callables, globals."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CASES = ("semantics", "globals", "io_globals", "callback_globals", "container_aliases", "opaque_nullable")


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    rows = []

    def run(command, expected=0, contains=None):
        row = dict(command=[str(item) for item in command], expectedExitCode=expected)
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder,
                                env=dict(os.environ, WIO_ROOT=str(ROOT)), capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode != expected or (contains and contains not in result.stdout + result.stderr):
            raise RuntimeError(str(row))
        return result

    wio = args.wio.resolve()
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    shared = ".dll" if os.name == "nt" else ".dylib" if sys.platform == "darwin" else ".so"
    for name in ("release_probe.h", "semantics.wio", "globals.wio", "callback_globals.wio", "container_aliases.wio",
                 "opaque_nullable.wio"):
        shutil.copyfile(HERE / name, folder / name)
    shutil.copyfile(HERE.parent / "performance/byte_io/global_mode_repro.wio", folder / "io_globals.wio")
    for name in args.case or CASES:
        source = folder / (name + ".wio")
        run([wio, source, "--emit-cpp", "--include-dir", folder])
        for optimization in ("-O0", "-O2"):
            program = folder / (name + (".exe" if os.name == "nt" else ""))
            run([args.cxx, "-std=c++20", optimization, str(source) + ".cpp",
                 f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", f"-I{folder}",
                 args.runtime.resolve(), *links, "-o", program])
            run([program])
            if name == "semantics":
                run([program, "fail"], expected=1, contains="release-probe failure")
            if name == "opaque_nullable":
                run([program, "callback"])
            # Full producers run their independent C++ static initializers
            # during DLL loading. Starting/joining a thread there deadlocks
            # under Windows loader lock (KI-011). This separate fixture tests
            # VM initializer callback reentry, not unsupported DLL startup.
            modes = ("pruned",) if name == "callback_globals" else ("full", "pruned")
            for mode in modes:
                # Keep the module basename identical across producer modes.
                output = folder / (name + optimization + mode)
                output.mkdir()
                bridge = output / ("bridge" + shared)
                bytecode = output / "main.wiob"
                run([wio, source, "--target", "shared", "--output", bridge,
                     "--bytecode-sidecar", bytecode, "--include-dir", folder,
                     "--backend-arg=" + optimization,
                     *(["--native-bridge-only"] if mode == "pruned" else [])])
                run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
                if name == "semantics":
                    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge,
                         "--", "fail"], expected=1, contains="release-probe failure")
                if name == "opaque_nullable":
                    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge,
                         "--", "callback"], expected=1, contains="WVM1225")
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("0.19 correctness: " + ", ".join(args.case or CASES) +
          "; native O0/O2 and applicable full/pruned VM bridges passed; "
          "threaded global callback uses pruned VM producer only (KI-011)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", action="append", choices=CASES,
                        help="Run only the named case; the registered CTest gate runs every case")
    verify(parser.parse_args())
