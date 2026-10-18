# N03 actual generated stream formatting

These Wio sources interpolate a native POD component with a C++ `operator<<`,
not a test wrapper around the legacy `runtime::Format` template. The 192-byte
case is bare interpolation; the 8192-byte case has counted prefix-NUL and suffix
literals. Native `operator<<` executes once per value. Immutable input payloads
are cached in all supported implementations before timing via two in-process
warmups. Every result is fully FNV-hashed with an independent Python oracle.

The native stream operator is currently **unsupported in the VM** (`WVM1062`).
All four-implementation sessions deliberately retain these failed VM launches.
The controller's overall `outputsCorrect` and `timingQualified` are false; there
is no usable VM duration/speedup. Do not quote an empty-summary zero as a VM time.
The separate correctness gate verifies supported VM builtin/object ToString
formatting and the explicit native-POD rejection. This patch does not add a VM
native formatter thunk or close KI-008.

```sh
python tests/performance/generated_stream/prepare.py --output build/generated-before --label before --count 2001
build/tools/wio-bench/Debug/wio-bench run build/generated-before/manifest.json --output build/generated-before-results
# Apply the codegen change and rebuild wio_app.
python tests/performance/generated_stream/prepare.py --output build/generated-after --sources build/generated-before --label after --count 2001
build/tools/wio-bench/Debug/wio-bench run build/generated-after/manifest.json --output build/generated-after-results
```

Use `.exe` and corresponding paths on Windows; see `--help` for tools, CPU and
paths. Wio `wio-bench` controls comparison samples, native A/A and a separate
final native/VM phase. Python prepares builds and checksums, not timing samples.
No retries, trimming or system power/priority/security changes are part of this
workflow. C++ is a frozen one-stream owning-copy reference, not optimized C++
with rvalue buffer extraction. C# uses UTF-16 builders and an allocating UTF-8
hash. These are equivalent byte outputs, not identical storage contracts.

Memory counters are a separate replacement-new/delete executable, static C++
runtime on Windows. They are not RSS, all malloc traffic or ordinary timing.
`helpers.cpp` includes the actual generated C++ (renaming only its host main)
and compares its helpers against copies of the original owning-copy paths in
the same image. It checks NUL, seek/high-water, partial badbit and thrown native
operators, allocation/live/peak bytes and capacity tradeoffs.
The selected bare helper requests a compact copy when capacity exceeds twice
the content size; a 217-byte native output with a 1 MiB reserve is an explicit
regression case. This gives up some short-lived allocation savings to avoid
retaining arbitrary native buffer excess. The unbounded intermediate build and
all its samples remain separate evidence, not a favorable replacement run.

`wio_n03_generated_stream` runs native O0/O2, the actual-helper diagnostic, native
exception/order/snapshot cases, supported VM builtins and the native POD negative
case. Expanded `wio_n03_scalar_format_plan` IR cases ensure private rendered
integer/bool fragments cannot enter the new bare/tail owning helpers. Existing
fragment views, Text interpolation, multi-operand ordering and builtin locale
policy retain their existing paths.
