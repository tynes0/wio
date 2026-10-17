"""N03 shared scalar ToString lowering; Wio controls every sample."""
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def expected(mode, count):
    checksum = 0
    for index in range(count):
        value = f"<item:{index + 1}>" if mode == 0 else f"{index}:item:{index * 2 + 1}|item:{index * 2 + 2}:true"
        hashed = 14695981039346656037
        for byte in value.encode():
            hashed = ((hashed ^ byte) * 1099511628211) & ((1 << 64) - 1)
        checksum = (checksum + hashed) & ((1 << 64) - 1)
    return str(checksum)


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "FormattingCalls.csproj"),
        project="FormattingCalls.csproj", assembly="FormattingCalls", stage="n03", title="N03 scalar ToString",
        cases=("single-hook", "ordered-hooks"), expected=expected,
        contracts="Stateful scalar object ToString; G6/decimal; left-to-right hooks after operand evaluation; UTF8 FNV",
        artifacts=("compiler/src/wio_compiler/wir/formatting_call_lowerer.cpp",
                   "compiler/src/wio_compiler/wir/lowering_pipeline.cpp",
                   "runtime/include/value_format.h", "runtime/include/integer_format.h",
                   "tests/performance/formatting_calls/prepare.py", "tests/performance/concat_reuse/prepare.py")))
