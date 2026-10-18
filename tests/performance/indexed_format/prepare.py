"""Frozen indexed-format workloads; Wio controls timing and Python checks bytes."""
import importlib.util
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def expected(mode, count):
    checksum = 0
    for index in range(count):
        text = f"{index:08d}/{index:08X}" if mode == 0 else f"{index / 8:.3f}/hi      "
        hashed = 14695981039346656037
        for byte in text.encode():
            hashed = ((hashed ^ byte) * 1099511628211) & ((1 << 64) - 1)
        checksum = (checksum + hashed) & ((1 << 64) - 1)
    return str(checksum)


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "IndexedFormat.csproj"),
        project="IndexedFormat.csproj", assembly="IndexedFormat", stage="n03", title="N03 indexed format",
        cases=("indexed-integer", "indexed-float"), expected=expected,
        diagnostic_flags=["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else [],
        contracts="Indexed D8/X8 and fixed F3/alignment; 10001 iterations; full-byte FNV; C++ specialized to_chars reference; C# invariant string.Format plus allocating UTF8 hash; two warmups",
        artifacts=("compiler/src/wio_compiler/wir/formatting_call_lowerer.cpp", "runtime/include/std_console.h",
                   "tests/performance/indexed_format/prepare.py", "tests/performance/concat_reuse/prepare.py")))
