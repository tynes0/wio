"""N03 float/builder/container formatting; Wio owns timing collection."""
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
        return str(sum(hash_bytes(format((index % 64) * .125 - 4, ".6g")) for index in range(count)) & ((1 << 64) - 1))
    if mode == 1:
        value = "".join(f"reading={format((index % 64) * .125 - 4, '.6g')};sample={index};ok=true\n"
                        for index in range(count))
        return str(hash_bytes(value))
    values = [-4., -.125, 0., .125, 1.5, 7.875, 32., 12500000.]
    value = "[" + ", ".join(format(item, ".6g") for item in values) + "]"
    return str((hash_bytes(value) * count) & ((1 << 64) - 1))


if __name__ == "__main__":
    harness.prepare(harness.arguments(HERE), dict(
        files=("main.wio", "probe.h", "main.cpp", "Program.cs", "Format.csproj"),
        project="Format.csproj", assembly="Format", stage="n03", title="N03 formatting",
        cases=("float-scalar", "mixed-builder", "float-vector"), expected=expected,
        contracts="Defaultfloat precision6/classic/nearest; full UTF8 FNV; C# G6 lowercase exponent; two warmups; builder snapshot",
        artifacts=("runtime/include/floating_format.h", "runtime/include/value_format.h",
                   "runtime/include/integer_format.h", "std/builders.wio", "runtime/include/std_builders.h",
                   "vm/src/value_format_internal.cpp", "vm/src/value_format_internal.h", "vm/src/machine.cpp",
                   "tests/performance/formatting/prepare.py", "tests/performance/concat_reuse/prepare.py")))
