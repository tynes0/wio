# Backend Optimization Passes

Current scheduling decision: **2026-11-27**. Complete the approved `0.20.x`
usability/correctness, `0.21.x` language/std, `0.22.x` IDE/tooling and `0.23.x`
SDK/product closure before remaining heavy optimization: pass 1 in `0.24.x`,
pass 2 in `0.25.x`, pass 3 in `0.26.x`. See the
[canonical release train](WIO_V1_RELEASE_PLAN.md#approved-remaining-order-2026-11-27).
This supersedes the 2026-10-15 immediate-after-functional-closure schedule, not
the three pass definitions, N01-N14, historical evidence or SDK's separate gate.

Status 2026-10-15: all three functional tasks are complete within the qualified
artifact matrix. The final four new gates passed on clean Windows Debug, Linux
Release and macOS Release builds (12/12 executions). See
[qualification evidence](WIO_BACKEND_CLOSURE_QUALIFICATION.md). These completed
functional tasks do not establish optimization-budget acceptance.

Pass 1 has started: `722e836` promotes proven closed scalar cells, uses plain
primitive SSA and direct synchronous CFG edges. The paired Mandelbrot compute
measurement is 33.44x faster than original Wio, with 1.027x C++ median execution
time for that kernel only. String/format/container/managed/async and whole-app
budgets remain open. See [scalar evidence](WIO_NATIVE_SCALAR_OPTIMIZATION.md).

The subsequent heavy optimization work is split into exactly three passes.
These are ordered work packages, not renumbered historical backend sprints.

## 1. Generated C++, runtime and coordinated std consolidation

The fixed native substep list is **N01-N14** in
[the native runtime plan](WIO_NATIVE_RUNTIME_PLAN.md). N01 begins with an
allocation/requested-byte and checksum-qualified cost map; missing subsystem
metrics remain explicit. These IDs do not change the three main passes.
Std consolidation also removes unnecessary/duplicate surfaces and evaluates
merging focused modules (for example `sort` into `algorithms`), with import/API
migration and native/VM behavior qualified before removal.

The earlier 2026-10-15 plan combined std source/API consolidation with pass 1.
Under the approved 2026-11-27 train, remaining std style/API/attribute migration
closes in `0.21.x` as single-purpose patches; pass 1 in `0.24.x` optimizes those
stabilized implementations. Preserve already completed combined slices and
their evidence. Keep baseline/regression checks and fix severe scaling or
performance defects before the heavy pass when found. Full SDK kit parity
remains separate and closes in `0.23.x` rather than being absorbed into pass 1.

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

### Required VM execution-policy closure (accepted 2026-11-26)

Scheduling update 2026-11-27: implement the runtime/CLI policy in `0.20.x`;
pass 2 in `0.25.x` optimizes/requalifies that stabilized execution model.

This subsection is part of pass 2 above, not a new pass. Before 0.20.1 WVM1008
terminated normal execution after the default 10,000,000-instruction cumulative budget
is consumed; ordinary application frames do not replenish that execution-state
counter. Long-lived applications must have an explicit suitable policy instead
of eventually failing merely because they continue running. Separate trusted
long-running execution from intentionally bounded embedding/test execution,
expose/document CLI, manifest and host controls, and define counter scope,
suspension and disabled-limit semantics. Preserve finite-budget WVM1008 tests,
cancellation/debugger abort, other resource guards and correct cleanup. Include
a multi-frame application that exceeds the former total limit. Increasing the
constant alone is not closure. The correctness policy is implemented in 0.20.1:
normal applications select unlimited execution, embedding keeps finite defaults,
and explicit budgets survive packaging/deployment with no-build verification.
See [scope and semantics](WIO_0_20_1_RELEASE_NOTES.md). The later VM pass still
owns performance qualification; this policy fix does not claim a VM speedup.

## 3. Compilation and preparation time

### Bytecode-first builds and native adapter reuse (accepted 2026-11-26)

Scheduling update 2026-11-27: required bytecode-first architecture and elimination
of unnecessary project bridge work belong to `0.20.x`; pass 3 in `0.26.x` tunes
and qualifies compiler/build throughput rather than delaying that usability fix.

This remains an open v1 blocker within pass 3. Since 0.20.2, standard-covered
executable projects emit bytecode and reuse the precompiled std adapter, avoiding
per-project C++ generation/compilation. Project-specific native producers and
shared/static wrappers still use their existing path. Separating native adapter
ABI/dependency caches from ordinary Wio body changes is the next narrow patch;
do not infer that this full blocker has closed. See
[implemented scope](WIO_0_20_2_RELEASE_NOTES.md).
Required outcomes:

- pure-Wio executable builds use bytecode and prebuilt VM/runtime support,
  without invoking a C++ compiler/linker for a per-project bridge;
- native libraries such as raylib use only the required compiled adapters and
  dependencies, with reusable versioned std/runtime support;
- Wio-only edits that preserve the native adapter/dependency closure regenerate
  bytecode without regenerating/recompiling the bridge;
- actual native-header/source/transitive-input, ABI/layout/generic-instantiation,
  code-generator/runtime/toolchain/target/flag changes invalidate the cache;
- publication, native ownership/identity checks, callbacks and artifact deployment
  remain correct; required shared/static embedding wrappers are reported
  separately rather than hidden in a claim of C++-free builds.

Measure compiler/linker invocations as well as cold/warm/incremental latency.
Require zero bridge compilation for pure executable builds and binding-preserving
script edits, plus positive invalidation tests for changed native requirements.
Reusing stale code or merely skipping every build is not an optimization.

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
explicit finite larger instruction budget; that benchmark override does not
change current production defaults or close the execution-policy blocker above.
Fresh Wio-only measurements follow each implementation slice, not only
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

Under the approved 2026-11-27 train, remaining heavy passes follow the
`0.20.x`-`0.23.x` contract/tooling/SDK closure in native -> VM -> compile/build
order (`0.24.x` -> `0.25.x` -> `0.26.x`). Final integrated acceptance rechecks
their budgets before beta/RC; it is not a fourth heavy pass. Earlier measured
work and N01-N14 identities remain intact. JIT remains post-v1.
