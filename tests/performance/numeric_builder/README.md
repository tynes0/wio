# N03 direct numeric StringBuilder append

The workload is Wio. The Wio-written `wio-bench` owns every timing sample;
Python prepares serial builds and independent decimal/FNV fingerprints.

```sh
python tests/performance/numeric_builder/prepare.py --output build/builder-before --label before --count 10001
build/tools/wio-bench/Debug/wio-bench run build/builder-before/manifest.json --output build/builder-before-results
# Change the builder implementation, not the workload.
python tests/performance/numeric_builder/prepare.py --output build/builder-after --label after --count 10001 --sources build/builder-before
build/tools/wio-bench/Debug/wio-bench run build/builder-after/manifest.json --output build/builder-after-results
```

Use platform/configuration-specific executable paths and `.exe` on Windows.
The shared harness exposes explicit compiler/runtime/VM/controller paths.

This patch adds constrained `StringBuilder.Append<T>` and `AppendLine<T>` for
the listed primitive numeric types and bool, preserving their exact types.
Because the API did not exist at the baseline revision, the recorded before
state first adds the **Wio interpolation implementation** of numeric Append.
That source is frozen as `builders-baseline.wio`; it is not represented as an
unmodified released compiler/std. The after state keeps that source body and
fuses its sole-use primitive interpolation/append in canonical Lowered WIR.
No per-append native ABI string marshalling is introduced in VM. Workload
sources/inputs are identical. This is not a new numeric public builtin member.

Three cases:

- Growing rows: id u64, finite f64, lowercase bool, separators and newline;
  initial capacity 64, final owning snapshot plus UTF-8 FNV.
- Clear/reuse: exact near-i64-min and near-u64-max decimal values and i8, initial
  capacity 64, clear each iteration, owning snapshot and full FNV each time.
- Reserved floats: f32 in an exactly representable bounded sequence, comma
  separators, initial capacity count*16, final owning snapshot plus FNV.

C++ uses independent stack-buffer `to_chars` direct append, **not streams**.
C# uses stack-span `TryFormat` and direct StringBuilder span append, invariant
G6 with lowercase exponent for floats. Its float inputs are exactly representable
as double too; integers stay exact integer values. UTF-16 builder storage and
UTF-8 hashing arrays remain real C# costs, not identical allocator layouts.
All lanes start with the same capacity hint and consume runtime input/results.
Two in-process warmups precede the timed kernel. Creation, formatting, snapshots,
hashing and local cleanup are included; parsing/startup/output are excluded.

The controller rotates/reverses comparison rounds with A/A native controls and
then runs a distinct final native/VM phase. All samples, failures and warmups
remain in JSON. No retry-to-pass, sample trimming or system power/priority changes.
Memory diagnostics are separate global-new/delete requested/live/peak counters,
not RSS, all malloc traffic, VM heap accounting or production timing. Whole-root-
process CPU/peak RSS and C# current-thread GC allocation bytes are separate scopes.

New-only tests: `wio_n03_numeric_builder_plan`, `wio_n03_numeric_builder_unit` and
`wio_n03_numeric_builder_semantics`. They check exact limits/type selection,
signed zero/G6, owning snapshots, reserve/clear, NUL, AppendLine, allocation-free
reserved primitive append, locale/rounding policy, native O0/O2 + VM, semantic
rejection of an explicit string numeric specialization, and libc++ vector<bool>
proxy normalization. Arbitrary user ToString/containers are not added to this
primitive overload; the separate KI-008 VM user-hook gap remains open.
