# N01 extended cost map

This is a separate suite; the original nine core workload sources are unchanged.
Run serially, with a fresh output directory:

```powershell
pwsh -NoProfile -File tests/performance/native_costs/run.ps1 `
    -Suite extended -OutputDirectory F:/Projects/wio/build/native-cost-extended-new-run `
    -Repetitions 15 -Count 2000
```

Repeat with another fresh directory and `-DisableAllocationCounters` to measure
the same kernels without allocation-counter overhead. This compiles out the
replacement new/delete operators and uses the standard allocator. Raw numeric
output retains zero placeholders, but JSON allocation metrics are **null**, the
summary says `not-measured`, and metadata records `CountersEnabled=false`.
Never interpret those placeholders as evidence of zero allocation.

The runner copies these files and the common `probe.h` into scratch before
emission. Both variants link the same backend runtime. Flags, warm-up, alternate
pairing, ordinary-new counters, source/hash evidence and failure policy follow
the [core runner](../README.md). Runner hash and suite selection are recorded.
These are instrumented composite timings, not per-operation latency or a clean
universal language comparison. Internal timing includes cleanup before Run
returns, but not process startup, reporting or global runtime destruction.

## Workload boundaries

- `unicode`: repeated code-point slice of globe/e/combining-accent, concat,
  UTF-8 snapshot and FNV-1a. Both sides use Wio's actual validated UTF-8 Text
  runtime, not a UTF-32/wchar substitution or normalization/grapheme benchmark.
- `dictionary`: integer-key insert/update, ordered lookups, missing-key fallback
  and rolling checksum, without reserve. C++ uses unordered_map; this is not
  ordered Tree/set, string-key hashing or rehash-only attribution.
- `result_error`: recoverable Result failure, IsError, copied error payload and
  long-message hashing. No thrown exception or panic is benchmarked. C++'s
  tagged variant is an optimized lower bound, not Wio's object/identity ABI.
- `weak`: the same noinline native helper uses the real Ref/WeakRef runtime,
  successful lock, logical expiry, failed lock and final physical destruction.
  Wio adds its native call boundary; this does not add a Wio weak-reference
  declaration or measure every compiler-generated retain/release event.
- `closure`: scalar capture, type-erased callable construction, one invocation
  and cleanup per iteration. C++ uses std::function; optimizer elision is allowed.
  No escaped closure, large capture, foreign callback or thread transfer.
- `async_ready`: an immediately completing coroutine and BlockOn in each
  iteration. C++ uses the same actual Wio AsyncTask runtime with a simple
  co_return; Wio's generated cancellation/transport overhead remains part of
  its path. No timer, worker executor hop, suspended resume or cancellation race.

`wio_native_cost_extended_contract` checks all six cases at counts 1 and 17 in
O0/O2 with counters, plus O2 without counters, against fixed independently
calculated fingerprints as well as paired
outputs. Golden arithmetic uses unsigned modulo 2^64. Bytecode/VM allocation
claims are outside this suite; the existing separate Mandelbrot runner tracks VM.

At count 2000, golden fingerprints are Unicode `11864642420176368741`, dictionary
`16071255685101081899`, Result error `7352865453926614568`, and weak/closure/ready
async `11505999919603546472`. These are useful independent checks, not time budgets.
