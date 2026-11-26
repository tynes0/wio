"""Legacy stream formatting ownership; native helper also runs through the VM bridge."""
import importlib.util
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def expected(mode, count):
    payload = "".join("\0" if index % 31 == 0 else chr(ord("a") + index % 26)
                      for index in range(192 if mode == 0 else 8192))
    result = 0
    for index in range(count):
        value = f"payload={payload};index={index};ok=" + ("true" if index % 2 == 0 else "false")
        hashed = 14695981039346656037
        for byte in value.encode():
            hashed = ((hashed ^ byte) * 1099511628211) & ((1 << 64) - 1)
        result = (result + hashed) & ((1 << 64) - 1)
    return str(result)


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "StreamOwnership.csproj"),
        project="StreamOwnership.csproj", assembly="StreamOwnership", stage="n03", title="N03 stream ownership",
        cases=("stream-192", "stream-8192"), expected=expected,
        diagnostic_flags=["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else [],
        contracts="Legacy runtime::Format via native wrapper; counted ASCII/NUL payload; exact decimal/bool; UTF8 FNV; owning result; C++ owning-copy stream; C# UTF16 builder",
        artifacts=("runtime/include/format.h", "tests/performance/stream_ownership/prepare.py",
                   "tests/performance/concat_reuse/prepare.py")))
