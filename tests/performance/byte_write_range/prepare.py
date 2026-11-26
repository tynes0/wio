"""Freeze copy versus borrowed-range writes; Wio owns the sampling session."""
import importlib.util
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def expected(mode, count):
    checksum = 14695981039346656037
    for index in range(7, 65526):
        checksum = ((checksum ^ ((index * 17 + 3) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return str(checksum ^ (count * 65519))


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE, default_bridge_optimization="O2"), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "ByteWriteRange.csproj"),
        project="ByteWriteRange.csproj", assembly="ByteWriteRange", stage="n04-write-range",
        title="N04 bounded byte writes", cases=("slice-copy-64k", "borrow-range-64k"), expected=expected,
        diagnostic_name="allocation-probe",
        diagnostic_flags=["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else [],
        contracts="Seek/overwrite 65519 bytes from offset 7 in a 65536-byte source; copy lane explicitly materializes an owned slice, range lane borrows existing storage; setup/open/close/full-byte output verification excluded from kernel and allocation window; two internal warmups; OS-cache writes, no durability flush; Wio OS handles, C++ ofstream, C# unbuffered FileStream are not identical syscall implementations; API strategy comparison, not unchanged-source compiler speedup",
        artifacts=("runtime/include/std_io.h", "runtime/src/std_io.cpp", "std/io.wio",
                   "tests/performance/byte_write_range/prepare.py")))
