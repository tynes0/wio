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

## 1. Generated C++ and runtime

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

For each pass, report the original baseline, previous revision and current
revision, workload/checksum, measured change, remaining hotspots and acceptance
status. Do not conflate a narrow benchmark win with complete backend closure.

These three passes follow functional backend closure; the final pre-v1
performance checkpoint still requalifies their budgets after later SDK/std/IDE
changes. It is not a fourth heavy optimization scope and does not move these
three passes behind the SDK kit work. JIT remains post-v1.
