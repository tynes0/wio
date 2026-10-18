# N03 float / builder / container formatting

The 2026-10-18 follow-up outlines floating stream/EH construction and uses
bounded non-growing-target stream views for integer/float fallback. The new
`wio_n03_stream_fallback` unit checks reserved/growing targets, allocation
attribution, long/NUL/partial facets and host float/rounding behavior. The Wio
gate adds exponent/Text loops and a reserved custom-facet native probe, run at
O0/O2 and via the VM bridge. See
[the follow-up evidence](../../../docs/WIO_FLOAT_STREAM_OPTIMIZATION.md).

Use the Wio-owned controller; Python prepares serial builds and the independent
Python `.6g` + full UTF-8 FNV oracle, not timing samples:

```sh
python tests/performance/formatting/prepare.py --output build/n03-before --label before --count 10001
build/tools/wio-bench/Debug/wio-bench run build/n03-before/manifest.json --output build/n03-before-results
# Apply the scoped change and rebuild Wio + the Release VM host.
python tests/performance/formatting/prepare.py --output build/n03-after --label after --count 10001 --sources build/n03-before
build/tools/wio-bench/Debug/wio-bench run build/n03-after/manifest.json --output build/n03-after-results
```

Use `.exe` and the corresponding source-build paths on Windows. Explicit compiler,
runtime archive, runner, VM host, C++ compiler and CPU options come from the shared
fixture harness (`--help`). The default VM path is the Release host. Preserve both
the old host and its observed hash before replacing it. The shared build pipeline
remains backwards compatible with the original N02 command/defaults.

Three workloads: independent scalar f64 strings; mixed float/integer/bool rows
appended to StringBuilder with capacity 64 and an owning final snapshot; and
eight-element f64 array interpolation. Values use defaultfloat precision 6 in the
classic locale. C++ uses standard streams, with one stream per row/array (not one
stream per array element). C# is Release/net9.0, tiering disabled, invariant G6
with lowercase exponent; its UTF-16 strings and UTF-8 hash-array allocations are
real included costs, not identical allocator/encoding contracts. No reserve hint
is added to only one lane; Wio/C++/C# builders start at 64.

All programs perform two in-process warmups and consume full-result FNV checksums.
The native hash uses a compiler memory fence, Wio passes runtime counts through
`std::benchmark::Input`, C# uses no-inline kernels/hash and a volatile result sink.
Kernel time includes formatting/hash/local cleanup, not startup/parsing/printing.
The Wio runner adds rotating/reversing five-sample comparison rounds, native A/A
controls, one warmup per lane, then a distinct three-sample native/VM-only final
phase with fresh warmups/controls. Keep failed/noisy runs; never rerun to pass.

`memory.json` is separate opt-in global-new/delete accounting, not plain timing,
RSS, copied bytes, all malloc/custom allocations or VM heap attribution. The Wio
runner also saves whole-root-process peak RSS and CPU separately. All windows
must end at zero tracked live bytes/blocks; inputs, toolchain/native/bridge/wiob
images and declared source hashes appear in the report.

Only new tests are run: `wio_n03_value_format`, `wio_n03_vm_format` and
`wio_n03_format_semantics`. Native reference tests compare 30,000 float + 30,000
double bit patterns to the host stream, plus supported rounding modes, custom
locale facets, signed zero, subnormals/extremes and nonfinite values. Python's
Wio gate also compiles that oracle with the actual generated-code C++ compiler.
Wio cases cover native O0/O2 + VM, nested containers, bytes/bools/chars/NUL,
Unicode strings, dictionary syntax, generic ToString and builder snapshots.

`custom.wio` is native-only positive coverage for stateful user ToString calls,
including object arrays. It is a deliberate **negative VM case** expecting
WVM1062: VM callback/continuation support for user formatting is still open.
Do not present that bounded rejection as native/VM feature parity.

The original VM also rejected builtin float arrays. Its failed baseline records
are kept, not replaced with a zero duration. After the typed formatting fix the
same frozen builtin-container benchmark must pass. JSON numeric serialization
keeps its independent max_digits10/round-trip policy; this formatting pass does
not change JSON numbers to six-digit display strings.
