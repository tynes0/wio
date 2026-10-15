"""Build Wio/C++ diagnostic workloads, then use the Python measurement driver."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

import qualify
import measurement as m

ROOT = qualify.ROOT
HERE = Path(__file__).resolve().parent


def default_file(candidates):
    for path in candidates:
        if path.is_file():
            return path.resolve()
    return candidates[0]


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--suite", choices=("core", "extended"), default="core")
    p.add_argument("--count", type=int, default=2000)
    p.add_argument("--repetitions", type=int, default=15)
    p.add_argument("--timeout", type=float, default=120)
    p.add_argument("--cpu", type=int, default=-1)
    p.add_argument("--allow-unpinned", action="store_true")
    p.add_argument("--disable-allocation-counters", action="store_true")
    p.add_argument("--cxx", default="g++" if os.name == "nt" else "c++")
    p.add_argument("--wio", type=Path, default=default_file([ROOT / "build/app/Debug/wio.exe", ROOT / "build/app/wio", ROOT / "build/app/Release/wio.exe"]))
    p.add_argument("--runtime", type=Path, default=ROOT / "build/runtime/backend/libwio_runtime.a")
    return p


def build(args):
    if not 1 <= args.count <= 100000 or args.repetitions < 3:
        raise ValueError("Require count in [1,100000] and >=3 repetitions")
    directory = args.output.resolve()
    if directory.exists():
        raise ValueError("Choose a fresh build directory")
    directory.mkdir(parents=True)
    compiler = shutil.which(args.cxx)
    if not compiler:
        raise ValueError(f"Native compiler not found: {args.cxx}")
    wio, runtime = args.wio.resolve(), args.runtime.resolve()
    fixture = HERE / "extended" if args.suite == "extended" else HERE
    files = [fixture / "costs.wio", fixture / "reference.cpp", HERE / "probe.h"]
    if args.suite == "extended":
        files.append(fixture / "workloads.h")
    for file in files:
        shutil.copyfile(file, directory / file.name)
    paths = [Path(__file__), Path(qualify.__file__), Path(m.__file__), wio, runtime, Path(compiler)]
    paths += list(directory.iterdir())
    for tree in (ROOT / "std", ROOT / "runtime/include", ROOT / "sdk/include"):
        paths.extend(p for p in tree.rglob("*") if p.is_file())
    before = m.snapshot(paths)
    env = dict(os.environ, WIO_ROOT=str(ROOT))
    subprocess.run([str(wio), str(directory / "costs.wio"), "--emit-cpp", "--include-dir", str(directory)], env=env, check=True)
    generated = directory / "costs.wio.cpp"
    paths.append(generated)
    before += m.snapshot([generated])
    flags = ["-std=c++20", "-O2", "-ffp-contract=off"]
    flags += ["-static", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    if args.disable_allocation_counters:
        flags.append("-DWIO_COST_NO_COUNTERS")
    programs = {v: v + (".exe" if os.name == "nt" else "") for v in ("cpp", "wio")}
    for variant, source in (("cpp", directory / "reference.cpp"), ("wio", generated)):
        command = [compiler, *flags, str(source), f"-I{directory}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}"]
        if variant == "wio" or args.suite == "extended":
            command += [str(runtime)]
            if os.name == "nt":
                command += ["-lws2_32", "-lshell32"]
        subprocess.run([*command, "-o", str(directory / programs[variant])], check=True)
    after = m.snapshot(paths)
    if sorted(before, key=lambda v: v["Path"]) != after:
        raise RuntimeError("Sources/toolchain changed during build")
    artifacts = m.snapshot(p for p in directory.iterdir() if p.is_file())
    for item in artifacts:
        item["Path"] = Path(item["Path"]).name
    m.write_json(directory / "build.json", dict(Schema=3, Harness="python", Benchmark="native-costs",
                 Suite=args.suite, Count=args.count, CountersEnabled=not args.disable_allocation_counters,
                 Programs=programs, Flags=flags, SourcesBefore=before, SourcesAfter=after, SourcesUnchanged=True,
                 Artifacts=artifacts))
    options = qualify.parser().parse_args(["--artifacts", str(directory), "--output", str(directory / "qualification"),
                                           "--repetitions", str(args.repetitions), "--timeout", str(args.timeout),
                                           "--cpu", str(args.cpu)] + (["--allow-unpinned"] if args.allow_unpinned else []))
    return qualify.qualify(options)


def main():
    try:
        result = build(parser().parse_args())
        print(f'OutputsCorrect={result["OutputsCorrect"]}; TimingQualified={result["TimingQualified"]}')
        return 0 if result["TimingQualified"] else 1
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
