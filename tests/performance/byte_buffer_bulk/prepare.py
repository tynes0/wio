"""Freeze ByteBuffer mixed writes using the explicit borrowed block API."""
import importlib.util
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def expected(mode, count):
    checksum = 14695981039346656037
    for index in range(1536):
        checksum = ((checksum ^ ((index * 17 + 3) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return str(checksum ^ (count * 2560))


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE, default_bridge_optimization="O2"), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "ByteBufferBulk.csproj"),
        project="ByteBufferBulk.csproj", assembly="ByteBufferBulk", stage="n04-buffer-bulk",
        title="N04 ByteBuffer mixed writes", cases=("mixed-owned-1k",), expected=expected,
        diagnostic_name="allocation-probe",
        diagnostic_flags=["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else [],
        contracts="API comparison: legacy owned/per-byte-hook Write versus explicit borrowed WriteBlock; each iteration clear/fill 1024, seek 512 and overwrite/extend, rewind and overwrite; reserved 2048-byte destination outside kernel; Work input snapshot owned in all languages (C# explicit clone), block arguments borrowed; two internal warmups; full output check and ToArray outside kernel/allocation window; C++ struct versus Wio intrusive object versus C# managed object, not identical allocation/dispatch machinery; no automatic speedup of existing Write callers",
        artifacts=("runtime/include/std_utility.h", "std/buffer.wio",
                   "tests/performance/byte_buffer_bulk/prepare.py")))
