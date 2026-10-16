"""N03 numeric builder append: identical Wio source before and after std optimization."""
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("benchmark_fixture", HERE.parent / "concat_reuse/prepare.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def hash_bytes(value):
    checksum = 14695981039346656037
    for byte in value.encode():
        checksum = ((checksum ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return checksum


def expected(mode, count):
    if mode == 0:
        return str(hash_bytes("".join(f"id={index},value={format((index % 64) * .125 - 4, '.6g')},ok="
                                     + ("true\n" if index % 2 == 0 else "false\n") for index in range(count))))
    if mode == 1:
        return str(sum(hash_bytes(f"{-9223372036854775807 + index}:{18446744073709551615 - index}:{index % 127}")
                       for index in range(count)) & ((1 << 64) - 1))
    return str(hash_bytes("".join(format((index % 64) * .125 - 4, ".6g") + "," for index in range(count))))


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "NumericBuilder.csproj"),
        project="NumericBuilder.csproj", assembly="NumericBuilder", stage="n03", title="N03 numeric builder",
        cases=("growing-rows", "clear-integers", "reserved-floats"), expected=expected,
        contracts="Exact integers; float G6/classic/nearest; direct append; capacity64 or count*16; owning snapshots; full UTF8 FNV",
        artifacts=("std/builders.wio", "runtime/include/std_builders.h", "runtime/include/value_format.h",
                   "runtime/include/integer_format.h", "runtime/include/floating_format.h",
                   "runtime/include/intrinsics.h", "vm/src/string_intrinsics.cpp", "vm/src/machine.cpp",
                   "compiler/src/wio_compiler/wir/numeric_append_optimizer.cpp",
                   "compiler/src/wio_compiler/wir/canonical_optimizer.cpp",
                   "tests/performance/numeric_builder/prepare.py", "tests/performance/concat_reuse/prepare.py")))
