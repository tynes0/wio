# Backend Optimization Passes

Accepted order: 2026-10-15. First finish the three functional closure tasks:
VM project CLI library/host integration, native distribution policy, and platform/
behavioral qualification. SDK kit parity remains a separate v1 blocker.

Status 2026-10-15: all three functional tasks are complete within the qualified
artifact matrix. The final four new gates passed on clean Windows Debug, Linux
Release and macOS Release builds (12/12 executions). See
[qualification evidence](WIO_BACKEND_CLOSURE_QUALIFICATION.md). The next work is
pass 1 below; none of the optimization targets is claimed achieved by this gate.

Pass 1 has started: `722e836` promotes proven closed scalar cells, uses plain
primitive SSA and direct synchronous CFG edges. The paired Mandelbrot compute
measurement is 33.44x faster than original Wio, with 1.027x C++ median execution
time for that kernel only. String/format/container/managed/async and whole-app
budgets remain open. See [scalar evidence](WIO_NATIVE_SCALAR_OPTIMIZATION.md).

The subsequent heavy optimization work is split into exactly three passes.
These are ordered work packages, not renumbered historical backend sprints.

## 1. Generated C++, runtime and coordinated std consolidation

Accepted extension on 2026-10-15: do the std source rules, Result-first API
standardization and implementation optimization module by module within this
pass, rather than optimizing APIs that will immediately be replaced later.
Capture behavior/performance first; keep readability, API and hot-path changes
in reviewable commits and qualify native/VM semantics together. This does not
move full SDK kit parity or language-wide legacy-attribute removal into pass 1.

Start with builders/string/text, byte/buffer output and containers, then expand
through the remaining std inventory. `builders` and its `binary` consumer have
the first scoped Result/style migration and native/VM append-capacity fix. See
[the module report](WIO_STD_BUILDERS_CONSOLIDATION.md). Remaining std modules and
managed/string argument overhead are still open. The scoped 120-column guard
does not freeze a language-wide formatter policy.

The next scoped slice, `79568bd`, promotes proven nonescaping synchronous string
cells and elides single-use adjacent append snapshots without changing the ABI.
Paired append time falls from 5.095 to 3.221 ms (36.78% less), and allocation
calls from 90,020 to 30,020; it is still 5.84x C++, not budget closure. See
[managed string evidence](WIO_MANAGED_STRING_OPTIMIZATION.md). Escaping captures,
async frames, general aggregates and remaining owned argument costs are not
silently promoted; KI-006 separately tracks an expired-borrow diagnostic gap.

`9554b63` adds classic-locale bounded integer formatting and one-buffer,
length-aware interpolation (including embedded NUL preservation). The fresh
full-render median is 128.437 ms versus 250.618 ms for the recompiled last saved
full snapshot, `ddfa2b2`; that combined gain spans both the storage and formatting
slices. Release VM completes the same image in a 20.5356141 s observation,
establishing an explicit pass-2 baseline. New per-update measurements and fixed
dated language references are in
[the interpolation report](WIO_INTEGER_INTERPOLATION_OPTIMIZATION.md).

`d6cdbd3` generalizes direct value-to-interpolation append and local observing
string-copy proofs across multi-block functions. The separate long-string
workload improves by 19.30% with allocation calls 60,020 to 30,020. Mandelbrot
does not establish a meaningful gain in this slice: both the initial adverse
1.64% delta and the subsequent balanced 15-pair 0.21% favorable delta are retained.
Native current/previous full medians in the latter are 131.769/132.043 ms;
three Release VM observations give 20.570923 s full output. See
[general string fast-path evidence](WIO_NATIVE_STRING_FAST_PATHS.md), including
fresh per-update native/VM and fixed dated C++/C#/Python rows. Remaining call,
aggregate, output and broader budgets are open; no VM optimization gain is claimed.

Reduce native runtime overhead toward 5-10% relative to equivalent hand-written
optimized C++ (roughly 1.05-1.10x execution time), where language semantics and the
workload permit it. This is an acceptance target to measure, not a claim that all
programs can meet it or permission to weaken ownership, checks or error behavior.

Measure compute-only kernels, string/byte formatting, containers, allocation,
intrusive retain/release, async and complete applications separately. Match
algorithm, inputs, checksums, compiler/version, optimization flags, floating-point
policy and runtime configuration. Keep I/O and startup costs visible, not hidden
inside a single favorable kernel result.

Priorities: plain nonescaping scalar storage, direct CFG/SSA emission, removal
of unnecessary optional/function/shared storage, copy and retain/release elision,
capability-driven support code, bounds/conversion simplification and measured
std/runtime hotspots. Include the Mandelbrot compute/format/full workloads.

Record generated/binary size, allocation traffic and native compilation cost
alongside execution time, but keep broad compiler throughput work in pass 3.

## 2. VM runtime

Obtain the best practical execution performance from the current non-JIT VM.
Profile dispatch, frames/registers, value representation, scalar arithmetic,
container/string/text operations, ownership, calls/native bridges, coroutine
resume and scheduler work. Optimize from measured hot paths with reproducible
before/after runs; do not present a JIT-language speed target as already feasible.

Preserve bytecode verification, safety budgets, source mapping, panic/cleanup,
thread contracts and native ownership. Separate loader/startup time from steady
execution, and publish baseline-relative gains with the same inputs/checksums.

## 3. Compilation and preparation time

Profile and optimize lexer, parser, semantic analyzer, Typed WIR, Lowered WIR,
C++ emission, bytecode emission and VM decode/verify/index preparation. VM steady
execution belongs to pass 2; native application execution belongs to pass 1.

Include generated C++ host compilation/linking and module packaging in end-to-end
build measurements. Investigate repeated traversal/type work, allocations, module
reuse, PCH/precompiled support, canonical sharding, parallel builds and validated
content-addressed reuse rather than assuming a stage is already cheap.

Measure per-stage time and peak memory plus cold/warm/incremental build latency.
Pin source/toolchain/runtime versions and configurations, report sample counts
and variability, and retain semantic/ownership/differential correctness gates.

## Reporting and final acceptance

Updated reporting requirement (2026-10-15): every optimization update must end
with fresh Wio native and Wio VM measurements for the same Mandelbrot case,
alongside fixed C++/C#/Python reference rows. Label those reference revisions,
sample dates/configurations and workload/metric explicitly; they are not fresh
paired measurements or a claim that host timing can never vary. Rebaseline the
references if algorithm, input, checksum, toolchain or measurement policy changes.
Use Release VM execution, retain raw samples, and report a timeout as a lower
bound rather than inventing a completed duration. The diagnostic VM host has an
explicit finite larger instruction budget; production safety defaults do not
change. Fresh Wio-only measurements follow each implementation slice, not only
the end of a numbered pass. Pure explanations do not create new timing evidence.

After all three passes, rerun the same Mandelbrot workload in all five rows:
C++, Wio native, Wio VM, C# and Python (the two Wio backends remain separate).
Report compute-only, formatting/buffer construction and complete PPM output
separately. Match image dimensions, iteration limit, numeric policy and output
checksums; record compiler/runtime versions, configurations, cold/warm startup,
sample counts/ranges and compare original/previous/final measurements without
turning historical timeouts into exact speedups. Use normal optimized C# and
plain Python baselines; disclose any native/vectorized-library variant separately.
Wio native and VM remain separate rows, with no implicit JIT claim. This final
measurement is part of the existing acceptance checkpoint, not a fourth pass.

For each pass, report the original baseline, previous revision and current
revision, workload/checksum, measured change, remaining hotspots and acceptance
status. Do not conflate a narrow benchmark win with complete backend closure.

These three passes follow functional backend closure; the final pre-v1
performance checkpoint still requalifies their budgets after later SDK/std/IDE
changes. It is not a fourth heavy optimization scope and does not move these
three passes behind the SDK kit work. JIT remains post-v1.
