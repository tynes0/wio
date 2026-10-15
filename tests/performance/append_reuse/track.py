"""Rebuild native Mandelbrot with current Wio; reuse explicit frozen VM inputs."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent))
import measurement as m

p = argparse.ArgumentParser()
p.add_argument("--output", type=Path, required=True)
args = p.parse_args()
directory = args.output.resolve()
directory.mkdir(parents=True, exist_ok=False)
wio = ROOT / "build/app/Debug/wio.exe"
runtime = ROOT / "build/runtime/backend/libwio_runtime.a"
original = ROOT / "build/mandelbrot-progress-n01-extended-2026-10-15"
records = []
for kind in ("compute", "full"):
    folder = directory / kind
    folder.mkdir()
    source = original / kind
    for file in source.glob("*.wio"):
        shutil.copyfile(file, folder / file.name)
    if kind == "compute":
        shutil.copyfile(HERE.parent / "mandelbrot_compute/benchmark_clock.h", folder / "benchmark_clock.h")
    for name in ("main.wiob", "bridge.dll"):
        shutil.copyfile(source / name, folder / name)
    if kind == "full":
        shutil.copyfile(source / "native.exe", folder / "previous.exe")
    command = [str(wio), str(folder / "main.wio"), "--emit-cpp", "--include-dir", str(folder)]
    subprocess.run(command, env=dict(os.environ, WIO_ROOT=str(ROOT)), check=True, timeout=120)
    flags = ["-std=c++20", "-O2", "-ffp-contract=off", "-static", "-Wa,-mbig-obj"]
    row = m.run_process(["g++", *flags, folder / "main.wio.cpp", f"-I{ROOT / 'runtime/include'}",
                         f"-I{ROOT / 'sdk/include'}", runtime, "-lws2_32", "-lshell32", "-o", folder / "native.exe"],
                        folder, 300)
    if row["Status"] != "passed":
        raise RuntimeError(str(row))
    records.append(dict(Kind=kind, Compiler=m.snapshot([wio]), Flags=flags, Build=row,
                        Artifacts=m.snapshot(f for f in folder.iterdir() if f.is_file())))
m.write_json(directory / "build.json", dict(NativeRebuilt=True, VmRebuilt=False,
             VmSource=str(original), Records=records))
