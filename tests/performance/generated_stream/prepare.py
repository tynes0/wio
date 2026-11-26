"""Actual generated native POD stream formatting; VM rejection is retained."""
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
        if mode == 1:
            value = "prefix\0" + value + "suffix"
        hashed = 14695981039346656037
        for byte in value.encode():
            hashed = ((hashed ^ byte) * 1099511628211) & ((1 << 64) - 1)
        result = (result + hashed) & ((1 << 64) - 1)
    return str(result)


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "GeneratedStream.csproj"),
        project="GeneratedStream.csproj", assembly="GeneratedStream", stage="n03", title="N03 generated stream",
        cases=("pod-bare-192", "pod-affix-8192"), expected=expected,
        diagnostic_flags=["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else [],
        contracts="Native POD operator<< interpolation via registered native formatter; cached immutable ASCII/NUL input after two warmups; exact index/bool; full UTF8 FNV; historical VM WVM1062 has no timing; C++ owning-copy stream; C# UTF16 builder",
        artifacts=("compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp", "runtime/include/value_format.h",
                   "runtime/include/floating_format.h", "runtime/include/integer_format.h",
                   "compiler/src/wio_compiler/wir/formatting_call_lowerer.cpp",
                   "compiler/src/wio_compiler/bytecode/verifier.cpp", "runtime/include/std_console.h",
                   "tests/performance/generated_stream/prepare.py", "tests/performance/concat_reuse/prepare.py")))
