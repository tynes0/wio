# N01 ownership attribution

This is a diagnostic profiler, **not a speed benchmark**. The language workload
is Wio; Python builds, checks and collects evidence. No production runtime or
compiler sources are modified. Requires matching GCC/gcov with JSON support;
Clang/MSVC coverage is not silently treated as equivalent.

```sh
python tests/performance/native_costs/ownership/run.py --output build/ownership-fresh \
  --wio build/app/wio --runtime build/runtime/backend/libwio_runtime.a
```

On a Windows multi-config build use `--wio build/app/Debug/wio.exe` (the default).
Select `--cxx` and `--gcov` explicitly if multiple toolchains are installed.
The output directory must be new. Profiles are isolated per subprocess and
case; `GCOV_PREFIX` overrides are rejected. Raw profiles and failed evidence
remain under that directory; no retry-until-pass or trimming.

## What runs

- Wio object construction plus owning argument, borrowed view, or owning return.
- A Wio call into the existing native `Ref`/`WeakRef` lifetime surface: successful
  live lock, failed expired lock, delayed physical destruction after final weak
  release. This does not introduce source-language weak-reference syntax.
- A separate native calibration called by Wio, with explicit strong copy/move
  construction and assignment. Each iteration must have exactly 3 IncStrong,
  4 DecStrong, 1 IncWeak, 2 DecWeak, 2 TryIncStrong entries and 1 destruction;
  plus one each of copy/move construction/assignment. A successful weak lock
  increments strong inside TryIncStrong, not through IncStrong.

All five cases run at counts 1, 17, 2000. Python independently computes the
wrapping-u64 checksum. Every case also runs using ordinary `-O2`, with no
coverage, reset/dump calls, or disabled inlining, and must match the checksum.
That is output parity, not proof that both binaries execute equal RC counts.

## Measurement boundary and limitations

The attribution binary uses `-O2 -fno-inline --coverage -fprofile-update=atomic`.
Disabling inlining keeps individual runtime functions visible in gcov's JSON.
The window resets immediately before `Run` and dumps after its returned value
and scoped locals have cleaned up; argument parsing, startup and final process
teardown are excluded. Only generated `#line` directives are removed in a
separate diagnostic C++ copy to avoid non-monotonic gcov function ranges.
Original generated C++ and all input/runtime files remain unchanged and hashed.

Counts identify paths in **this diagnostic compilation**, not the exact retained
operations or overhead of the fully optimized shipping binary. Function counts
include all instantiated Ref types; they are not merged/max gcov line counts.
Raw per-template counts are saved. Copy/move *handle events* are not copied
payload bytes. Weak TryIncStrong counts are attempts, not CAS retries or successes.
Single-thread calibration does not measure contention or races. Linked static
runtime objects are uninstrumented; this fixture's measured ownership operations
are in the generated translation unit/header-only runtime. Calibration rejects
missing/ambiguous counters instead of inventing zeros.

Neither profiler/ordinary process duration is timing-qualified. No claims about
String/Text/component payload copies, live/peak memory, allocator traffic,
threaded ownership or general application RC cost follow from this slice.

The `wio_native_ownership_attribution` CTest runs five Python checks including
the real Wio program (30 checked workload executions). It is registered only for
GCC with gcov present; Windows/Linux GCC CI runs it separately. macOS's Clang
controller remains useful, but this GCC-specific profiler is not claimed there.
