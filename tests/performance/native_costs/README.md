# Native cost map (N01)

Manual Windows / PowerShell 7 diagnostic. Requires the built Debug Wio compiler,
backend runtime and MinGW GCC. It changes no production implementation. Sources
are copied to a fresh ignored build directory before C++ emission; user playground
files and outputs are never used. Do not benchmark while another build/test runs.

The default `-Suite core` remains the original nine cases. `-Suite extended`
selects six additional workloads; see [their boundaries](extended/README.md).
`-DisableAllocationCounters` compiles out replacement allocation operators for
counter-free timing. The JSON marks allocation metrics unmeasured, not zero;
the default instrumentation mode and original core workload sources are unchanged.

```powershell
pwsh -NoProfile -File tests/performance/native_costs/run.ps1 `
    -OutputDirectory F:/Projects/wio/build/native-cost-map-new-run `
    -Repetitions 15 -Count 2000
```

Nine independently selected workloads: ordinary string `+=`, StringBuilder,
integer interpolation + builder, ByteWriter, dynamic array, component return,
object construction/alias/method, native scalar call, Result success/unwrap.
The count is runtime input, bounded to 1..100000. Large concat counts can be
quadratic in the current implementation; do not assume every count is cheap.

Both executables use GCC C++20 `-O2 -ffp-contract=off -static`; no fast-math.
Each case has an excluded warm-up pair, followed by alternating-order measured
pairs. C++ warm-up supplies the checksum oracle; every C++/Wio execution must
match. Bytes use order-sensitive FNV-1a; integer sequences use a rolling checksum
rather than a sum that would ignore reordering. This verifies parity, not an
independent algorithm proof. `results.json`
preserves samples, timeouts/failures and hashes; `summary.json` contains successful
medians/ranges. A failure stops the run with nonzero status and partial evidence.
Output directories cannot already exist. Build failures stop before measurement.

The internal timer includes dispatch, setup, construction, copies, hashing/sums
and local destruction inside Run, but excludes process startup and printing.
Process wall time is recorded separately. This is a composite cost map, not
per-instruction latency. Native Hash conversion is borrowed; native argument
transport elsewhere remains part of the Wio path. Both object versions use
Wio's real intrusive Ref and explicit aliasing. The C++ builder is direct
std::string append with a matching snapshot; Result uses std::variant success,
a tagged-value **lower bound, not Wio's current object/identity/ABI model**.
Compiler optimization of reference loops is allowed and must not be attributed
entirely to one runtime operation.

Ordinary throwing new/new[] are replaced only in these standalone diagnostic
executables. Counters use relaxed atomics and record successful calls and the
sum of requested sizes while the timer is active. Instrumentation itself costs
time. It excludes malloc, aligned/nothrow-specific paths not routed through these
operators, deallocation/live bytes, copied bytes, RC events and RSS. Do not use
it for a VM-host allocation claim or link it into production. Future attribution
requires separate profiler/opt-in builds; the current runtime is untouched.

The new `wio_native_cost_contract` CTest qualifies counter reset/disable/byte
accounting, invalid arguments, fingerprints and all nine cases at counts 1/17
under both `-O0` and `-O2`. It has no timing threshold and is not the manual
benchmark. Only this new test is needed for the instrumentation slice.

Full plan and remaining coverage: [N01-N14](../../../docs/WIO_NATIVE_RUNTIME_PLAN.md).
