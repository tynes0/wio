"""Prepare same-byte cached-file reads; Wio owns all benchmark sampling."""
import importlib.util
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def expected(mode, count):
    checksum = 14695981039346656037
    for index in range(65536):
        checksum = ((checksum ^ ((index * 17 + 3) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return str((checksum * count) & ((1 << 64) - 1))


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE, default_bridge_optimization="O2"), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "ByteIo.csproj"),
        project="ByteIo.csproj", assembly="ByteIo", stage="n04", title="N04 owned byte I/O",
        cases=("exact-64k", "some-64k", "exact-reused-64k", "some-reused-64k"), expected=expected,
        diagnostic_flags=["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else [],
        contracts="Seek/read/full-byte FNV; owned cases allocate each iteration; reused cases use a preallocated caller buffer in all languages, setup outside kernel/allocation window; OS-cache hot after two warmups; file setup/open/close excluded; Wio OS handles, C++ ifstream, C# managed buffering disabled FileStream; not cold disk throughput or identical syscall/error implementations",
        artifacts=("runtime/include/std_io.h", "runtime/src/std_io.cpp", "std/io.wio", "runtime/CMakeLists.txt",
                   "runtime/cmake/WioBackendRuntimeProfile.cmake",
                   "tests/performance/byte_io/prepare.py", "tests/performance/concat_reuse/prepare.py")))
