# Native Runtime Optimization: N01-N14

Accepted 2026-10-15. These are fixed substeps of optimization **pass 1**, not a
renumbering of historical backend sprints, v1 blockers, or the three main passes.
After native generated code/runtime/coordinated std comes pass 2 (non-JIT VM),
then pass 3 (compiler and build throughput). SDK remains a separate v1 blocker.

## Fixed execution order

| ID | Scope | Completion evidence |
| --- | --- | --- |
| N01 | Measurement and cost map | Qualified workloads, raw samples/checksums, clear coverage gaps; distinguish timing, allocation, copies, RC and memory metrics |
| N02 | String/text/concat (implemented 2026-10-16) | Last-use buffers, transactional append and pure-tree flattening; bounded correctness gate complete; noisy timing acceptance remains N14 |
| N03 | Formatting/builders | Direct numeric append, float/locale costs, capacity reuse; preserve formatting and snapshot contracts |
| N04 | Byte/buffer/I/O | Avoid encoding/string/byte round trips, batch writes, reuse buffers; same-format comparisons stay separate from format changes |
| N05 | Value/place/component representation | Proven stack storage, direct fields, fewer temporaries and unnecessary optional/shared/type-erased cells; exact lifetimes and cleanup |
| N06 | Containers | Growth/reserve/bulk operations, iteration/index copies, map/set key/hash/rehash costs, span/view and sorting callbacks |
| N07 | Objects/intrusive RC | Remove redundant retain/release and handle transfers with lifetime/thread proofs; measure weak refs and destruction too |
| N08 | Calls/native ABI | Value argument transport, direct calls/devirtualization, closures and adapters/marshalling; retain boundary validation and exception behavior |
| N09 | Numeric loops/checks | Invariants/conversions/branches, proven redundant bounds/null checks, optimizer-friendly loops; preserve overflow and FP contracts |
| N10 | Result/Option/nullable/any | Success-path allocation/copies, propagation/boxing and cold errors; Result-first std migration without silently changing identity/ABI |
| N11 | Async/threading | Frames/captures, ready-task paths, executor hops, scheduler contention, cancellation/cleanup and batching |
| N12 | Startup/reflection/runtime support | Cold start, registration/global initialization, metadata lookup, unused features and cold panic/log paths |
| N13 | Remaining std closure | JSON/config/parsing, hash/random/regex/sort, Unicode/time/fs/process and remaining inventory, with representative cost checks |
| N14 | Native acceptance | Real applications plus microcases; correctness/platform/regression gates; original/previous/current evidence; measure 1.05-1.10x C++ where comparable semantics permit |

The order is fixed; dependency-related work can be shared without renumbering.
Do not close a whole item because a single narrow case improved. No universal
5-10% overhead guarantee and no permission to weaken safety, arithmetic or ABI.

## Std consolidation policy

The std pass includes **removing unnecessary APIs/modules and merging overlapping
specialized surfaces**, not only formatting existing code. For example, consider
moving the sorting algorithms from `std::sort` into `std::algorithms`, rather
than keeping a separate sorting-only module. This is an accepted candidate,
not a completed move or a mandate to merge unrelated responsibilities.

For each consolidation, inventory declarations, imports, call sites, tests,
native bindings and docs first. Select one canonical implementation/API and
state the migration/removal policy explicitly. Do not preserve duplicate
implementations indefinitely or delete behavior just because names resemble
one another. Apply Result-first API cleanup and the user's std formatting,
visibility, `[]` attribute and block rules in the same module slice. Coordinate
native/VM semantics; SDK/docs/IDE compatibility work remains visible.

No std module, production runtime, compiler or generated-code policy changes in
the N01 instrumentation slice. Existing dirty std/playground files are preserved.

## N01 inventory and honest coverage

| Workload/cost | Existing or new fixture | Current measurement boundary |
| --- | --- | --- |
| Numeric compute and full PPM | `mandelbrot_compute/compute.wio`, frozen full-render snapshot, `progress.ps1` | Native/Release VM, checksums; compute vs full wall time; existing snapshot provenance |
| Long-string interpolation | `string_interpolation/append.wio` | Paired generated snapshots, ordinary new/new[] count and internal time |
| Append and builder snapshot | `std_builders/append.wio` | Native paired code, allocations, fixed fingerprint |
| Ordinary `+=`, builder and integer output | New `native_costs/costs.wio` | Instrumented kernel duration, ordinary new/new[] calls and requested bytes |
| ByteWriter and dynamic array | Same new fixture | Growth/write/snapshot/read-sum composite costs, not isolated I/O |
| Component return and object alias/method | Same new fixture; `native_costs/ownership` attribution | Composite costs; separate GCC/gcov diagnostic counts for object argument/view/return and Ref transfers, not production-binary RC timing |
| Native calls and Result success | Same new fixture | Call/unwrap loop costs; tagged-value C++ Result is a lower bound, not matching Wio object ABI |
| Unicode, dictionary, Result errors, weak refs, closures | `native_costs/extended` | Text slice/concat/UTF-8, integer Dict insert/lookup/miss, recoverable error payload, runtime weak helper and scalar capture; not full Unicode/set/exception/escaped-closure coverage |
| Async ready task | Same extended suite | Same AsyncTask runtime in both variants, immediate completion and BlockOn; not suspended/worker/timer/cancellation-race costs |
| Scheduling, cancellation, threading, maps/sets/Unicode expansion | Pending | Existing correctness tests are not performance baselines; add focused workloads before changing those paths |
| Cold startup, reflection, real file/network I/O | Pending | Whole-process wall time alone does not isolate these costs |
| Ref copy/move and retain/release events | `native_costs/ownership` | Calibrated GCC/gcov diagnostic, no runtime modifications; ordinary O2 output parity. Production-optimized RC attribution remains pending |
| String/text/component bulk-copy requests | `native_costs/copies` | Calibrated memcpy/memmove linker wrapping, owning/view/return and concat cases, normal O2 output parity; not all payload/scalar/inline copies or shipping-O2 counts |
| Live/peak memory and aligned traffic | `native_costs/memory` | Separate diagnostic ordinary/aligned new/delete window accounting and OS resident-memory checkpoints; not copied bytes or production RSS; malloc/custom allocator traffic remains open |

N01 has a working first cost-map slice, **not complete coverage of all native
runtime subsystems**. Missing metrics are not zeros. Later slices must extend
coverage before changing the relevant subsystem. N02's first local-append slice
started on 2026-10-16; the residual N01 inventory stays open.

The initial nine-case, 15-pair allocation baseline and its limitations are in
[the measured cost map](WIO_NATIVE_COST_MAP.md). This is starting evidence,
not a declaration that N01's pending instrumentation categories are finished.

The next slice adds six workloads and a separate counter-free timing mode:
[extended cost-map evidence](WIO_NATIVE_COST_EXTENDED_MAP.md). Core source cases
are unchanged; the measured inventory now has 15 cases without renumbering N01-N14.

## Instrumentation and reproducibility

See [the manual runner](../tests/performance/native_costs/README.md). Timing and
allocation instrumentation share a build; atomic counters add overhead, so these
are diagnostic timings, not an uninstrumented production-speed leaderboard.
The optional `-DisableAllocationCounters` mode provides separate counter-free
timings with standard allocation operators. It marks allocation metrics null,
not zero; instrumented and counter-free sample sets must not be conflated.
Requested bytes are cumulative successful ordinary new/new[] request sizes,
not RSS, resident/peak/live memory, allocated capacity or copy traffic.

Record source/toolchain/runtime/binary hashes, current revision and dirty state,
optimization/FP flags, excluded warm-up, sample counts and ranges. Alternate
paired order. Verify every output and keep failed/timeout samples. Preserve
Mandelbrot's same-output and fixed dated cross-language reference policy.

An N01 baseline does not prove an optimization or a regression against another
session. Numeric-only reference loops may be optimized more aggressively than
generated Wio; the result is an end-to-end composite cost, not the latency of an
individual component operation, Ref increment or instruction.

The Mandelbrot controller now rejects unstable runs and supports exact-binary
remeasurement, pre-launch affinity, VM warm-ups, A/A controls and endpoint hash/
power checks. See [measurement correction and evidence](WIO_MEASUREMENT_QUALIFICATION.md).
This repairs acceptance, not the busy machine. The Python Mandelbrot follow-up
meets coarse gates; tighter accuracy and noisy microcase baselines remain pending.
The native-cost runner now uses Python affinity, process-capture,
hash and stability helpers, with independent timing/output acceptance and frozen
binary reuse. See [the native-cost controller pass](WIO_NATIVE_COST_CONTROLLER.md).
Do not retroactively qualify its older diagnostic timing results.

The extended session also shows substantial timing drift with unchanged compiler,
runtime and VM-host hashes, including a slower prior-generated-C++ control.
Keep all samples and qualify the environment before numeric performance-budget
acceptance. Stable documented conditions and paired controls are an N01
measurement requirement; do not silently alter the user's system power policy.

## Progress reporting

Every update names its current N01-N14 item. Closing one item and starting another
in the same turn must state both the closure and transition explicitly. A narrow
subsystem slice is not a whole-item closure. The earlier controller passes were
N01-only; N02 is closed within its recorded bounds and the current production
change is N03 formatting/builders. Remaining N01 attribution and coverage gaps above
remain open.

The next ownership-attribution slice adds Wio object argument/view/return cases
and native weak/control calibration, with Python build/collection. See
[ownership attribution](WIO_OWNERSHIP_ATTRIBUTION.md). It counts header-only Ref
paths in a non-inlined coverage compilation, not exact shipping-binary events,
copy bytes or operation latency. N01 remains open; no N02 transition here.

The subsequent [memory attribution slice](WIO_MEMORY_ATTRIBUTION.md) adds live
and peak requested-byte/block counters, calibrated aligned/failure/free handling,
Wio array/string/object cleanup, native weak lifetime, and platform-specific
resident-memory checkpoints. Its replacement allocator is diagnostic-only;
neither its timings nor its RSS represent the uninstrumented production binary.
Payload-copy bytes, broader coverage and timing qualification remain open.

The [bulk-copy attribution slice](WIO_COPY_ATTRIBUTION.md) adds calibrated
observed memcpy/memmove byte lengths for string/text/component transport and
repeated concat. These are separate from allocation counts, Ref handle events,
and process memory. They expose growing-prefix costs for N02 investigation,
without claiming to count inline/scalar copies or changing production code.

New behavior tests should preferably be Wio; Python is preferred for portable
build/process/measurement orchestration. Avoid new substantial PS1 test logic.
Legacy PS wrappers may remain for compatibility, not as the primary new test path.

## N02: first production slice, 2026-10-16

[Local string/text append reuse](WIO_LOCAL_APPEND_REUSE.md) replaces a proven
adjacent local load/add/replace sequence with in-place append. It preserves RHS
snapshots and excludes fields, globals, proxies, intervening effects and async
frames. This is not closure of N02 or a rewrite of the VM.

Each subsequent slice follows **measure -> optimize -> measure**. Report plain
runtime separately from diagnostic allocation/live/peak memory, including retained
capacity tradeoffs, and compare original/previous/current when evidence permits.
Keep failed/noisy samples; do not claim a speedup from an unqualified session.
Balance memory and speed rather than optimizing only the displayed timing.

## Wio-owned comparison policy, 2026-10-16

The user requires related **Wio + C++ + C#** workloads before a change that can
affect measurements. Wio has both native and VM variants. Capture a before state,
apply the scoped change, rebuild and capture after. At the end of each session,
run Wio native and VM once more as a separate repeated final phase, not a
replacement for comparison samples. Include independent checksums/operation
counts, exact build/runtime/source provenance, warmups, raw failures, A/A and
range/drift qualification. Report memory separately from time and retain noise.

`std::benchmark` and the Wio-written `wio-bench` now implement this workflow; see
[the library/controller manual](../tools/wio-bench/README.md). Python remains a
bootstrap and independent validation oracle, not the main sample controller.
Allocation diagnostics require a separate opt-in executable and are not ordinary
timing samples. Existing historical tables stay dated; do not retroactively
reinterpret their measurement scopes. This tooling does not close N02 or start
the VM/compiler optimization passes.

## N02 closure, 2026-10-16

[The concat closure pass](WIO_CONCAT_OPTIMIZATION.md) completes the production
N02 scope: ordinary self-assignment, transactional multi-append, pure concat-tree
flattening, last-use owned-buffer transport and Text rvalue overloads. Identical
direct field/global cells qualify; unknown ref/proxy/coroutine paths intentionally
retain their checked fallback. No unproven alias/borrow lifetime is weakened.

Only three new tests ran locally: 3/3 passed, native O0/O2 and VM plus ownership/
type/effect/CFG and allocation-failure checks. Before/intermediate/final Wio-owned
four-implementation sessions have 504/504 correct outputs and zero final tracked
live bytes/blocks. Growing string/text allocations are 2005/4013 -> 16/24; chain
allocations are 12009/22038 -> 4005/4029. All raw timing sessions remain globally
unqualified; keep that N01/N14 acceptance gap visible, not a speedup promise.

The pre-existing mutating-RHS borrowed-LHS native/VM discrepancy is now explicitly
tracked as KI-007; N02 does not silently choose new language semantics to hide it.
N01 residual coverage and the v1 parity fix remain open. Next production work is
**N03 formatting/builders**; no N03 transition was implemented in this patch.

## N03: float and builtin-container formatting, 2026-10-17

[The first formatting slice](WIO_FORMATTING_OPTIMIZATION.md) replaces normal
finite float streams with stack-buffer `to_chars(general, 6)` and appends builtin
containers directly into the interpolation buffer. Locale, rounding, nonfinite
and user-hook fallback contracts remain explicit. Negative-zero literal transport
and VM builtin array/dictionary/char/float display parity were also corrected;
user-defined ToString in VM remains open as KI-008, not a passing feature claim.

Frozen Wio native/VM + C++ + C# workloads cover scalar floats, mixed builder rows
and float vectors, with independent output checks, separate allocation diagnostics
and a distinct final native/VM measurement phase. Failed baseline VM-vector
processes remain in evidence; no speedup is computed from unsupported execution.
N03 stays open for direct numeric builder append, richer formatting, the legacy
stream API and shared user-hook callable lowering. N04 has not started. N01/N14
measurement qualification and KI-007 remain separate open work.

## N03: direct primitive builder append, 2026-10-17

[The numeric builder slice](WIO_NUMERIC_BUILDER_OPTIMIZATION.md) adds constrained
primitive/bool Append and AppendLine overloads and fuses sole-use adjacent empty-
affix primitive formatting/append in canonical Lowered WIR. C++ and VM append
directly to their string storage, without per-number native ABI marshalling.
Exact integer types, locale/rounding, snapshots and checked receiver behavior are
retained. Frames, nonprimitive hooks, intervening effects and shared formatting
remain conservative; only matching unobservable string cleanup is removed.

The same-source before/after suite uses direct-to_chars C++ and stack-span C#
references, separate memory diagnostics and a final repeated native/VM phase.
The before state explicitly includes the new API's interpolation implementation;
it is not an unmodified release. The previous macOS const vector<bool> proxy
compile failure is corrected here, not erased from its CI record. N03 stays open
for richer formatting, legacy Format and KI-008 user-hook lowering. N04 has not
started; timing budgets and the fixed remaining passes are not renumbered.

## N03: libc++ floating-contract correction, 2026-10-17

[The portability follow-up](WIO_N03_PORTABILITY.md) fixes a macOS CI output
regression by conservatively retaining host stream formatting on libc++.
The MSVC/libstdc++ fast path remains; libc++ performance is explicitly pending,
not assumed equal. The finite-value counterexample has permanent oracle
coverage. Six new N03 tests pass locally; failed macOS evidence is retained,
and cross-platform CI must be checked independently. N03 is still in progress.

## N03: shared scalar ToString call lowering, 2026-10-17

[The scalar hook slice](WIO_FORMATTING_CALL_LOWERING.md) materializes synchronous
public zero-argument object/interface ToString in Typed WIR after specialization.
Expression evaluation, ordered formatting, null/call/merge and cleanup are shared
by C++ and VM. Concrete generic callers, views and temporary objects have native
O0/O2 + VM coverage. Implicit interface public visibility is also retained in WIR.
Object-container traversal, async frames and native/open-generic hook declarations
remain KI-008 work; N03 and N14 are not closed. The preceding libc++ correction's
CI passed Windows Debug, Ubuntu Release and macOS Release.

## N03: call-scoped native string transport, 2026-10-17

[The transport pass](WIO_NATIVE_STRING_BORROW_OPTIMIZATION.md) removes shared
heap cells and general Place construction from proven synchronous native string
borrow leaves. Ordinary ABI/function identities and native exception boundaries
remain. This shares proof work with the planned storage passes, not closure or
renumbering of N05. The original shared hook lowering is unchanged: a streaming
experiment was rejected after measurement exposed added VM cost.

Three new relevant gates pass locally, including native O0/O2, direct VM calls,
mutable growth, snapshots and exceptions. Indirect native function values exposed
the unchanged WVM1003 frame-entry boundary (KI-009); do not infer full native/VM
callable parity. N03 still includes richer formatting, legacy Format and remaining
KI-008 hooks. N04 has not started, and N01/N14 timing acceptance remains separate.

## N03: bounded fields and pre-sized rendered interpolation, 2026-10-17

[Two measured slices](WIO_SCALAR_FIELD_OPTIMIZATION.md) replace closed primitive
Wio field transport with a direct pointer inside proven call-free block windows,
then preflight and reserve final interpolation containing already-rendered string
parts. Root checks, ABI and WIR/VM shape remain unchanged; unknown/proxy/escaping/
native/async cases retain the established representation. This is shared proof
work with N05, not a renumbering or closure of that pass.

Three focused gates pass, with O0/O2 + VM object/component-extension, mutable
field-ref, numeric fields, repeated/NUL/Text strings and size-overflow checks.
Before/field-only/final sessions retain 252/252 correct outputs, but timing is
globally unqualified. Native medians fall versus the fresh baseline and long-case
allocations fall from 15004 to 10003; single-case final observations rise between
the two slices and remain visible. N03/C++ budgets and KI-008/KI-009 stay open;
N04 has not begun. Next cost attribution is receiver/merge transport and integer
locale/fallback formatting, without weakening contracts.

## N03: bounded private receiver transport, 2026-10-17

[The receiver slice](WIO_RECEIVER_OPTIMIZATION.md) removes general self Place
transport from small proven synchronous scalar-field object methods. The
ordinary ABI entry remains, override selection is unchanged, and null/expired
callback borrow checks remain explicit. Native/proxy/behavioral/async/generic
and escaping/reentrant receiver uses keep the established entry. This is shared
representation/call proof work with N05/N08, not their closure or renumbering.

A second separately measured slice outlines integer stream formatting while
retaining the per-call live locale guard. Final 3/3 focused gates pass; 252/252
session outputs are correct. Native comparison observations fall to
0.8473/2.6165 ms, but final-repeat single native rises to 1.1231 ms. Heap traffic
is unchanged, generated source grows, and the executable is 6186 bytes smaller.
All timing remains globally unqualified; no universal speedup/budget closure.

N03 remains open for richer formatting, legacy Format and KI-008 hooks. KI-007,
KI-009 and N01/N14 timing acceptance remain open; N04 has not started. The
preceding scalar-field commit's CI passed Windows Debug, Ubuntu Release and
macOS Release; the present commit requires its own platform qualification.

## N03: single-value string edge forwarding, 2026-10-17

[The transfer slice](WIO_STRING_TRANSFER_OPTIMIZATION.md) removes the source
string slot and parallel-edge temporary when a fresh owned string producer is
immediately followed by its only single-value transfer. The destination slot
and cleanup remain. Extra uses, references/metadata/cleanup, intervening effects,
parallel assignments, non-string values and async/coroutine frames retain their
established path. This is C++ storage emission only, not new WIR/VM instructions
or ABI. Shared N05 proof work does not close or renumber that pass.

The new/expanded gates pass 2/2 with O0/O2 + VM native return/error, snapshot,
NUL/Text and ordered-hook coverage. Both sessions retain 168/168 correct outputs.
Native comparison medians are 0.9110/2.4496 -> 0.7787/2.1290 ms; separate final
0.8710/2.3850 -> 0.8260/2.1240. Work's measured GCC O2 prologue reservation falls
1032 -> 696 bytes; heap traffic is unchanged and the executable is 3072 bytes
smaller. Global timing remains unqualified and the C++ budget stays open.

N03, KI-007/KI-008/KI-009 and N01/N14 performance qualification remain open;
N04 has not started. The preceding receiver/cold-integer commit's three-platform
CI passed independently of this change.

## N03: direct construction into proven-empty cells, 2026-10-17

[The initialization slice](WIO_STRING_INIT_OPTIMIZATION.md) removes the fresh
source slot and subsequent moves for a sole initializer into a closed local
string cell. A bounded may-initialized CFG proof joins all predecessors and
backedges and checks emptiness before the producer, not after the later reset.
Owned string-only cleanup stays in place; other effects/resources, aliases,
extra/metadata uses, Text and async frames retain the existing path. An exact
string factory uses prvalue construction and remains empty on exceptions.

Four relevant gates pass, including native O0/O2 + VM loops, snapshots, native
returns/initializer failures and runtime factory-state tests. Both sessions
retain 168/168 correct outputs. Native comparison observations fall
0.7707/2.1480 -> 0.6586/2.1031 ms; separate final 0.7730/2.1295 ->
0.6762/2.0893. Work loses two optional-string slots, actual O2 stack reservation
falls 696 -> 584 bytes, and the executable is 512 bytes smaller. Heap traffic
is unchanged and process RSS slightly rises. Timing remains globally
unqualified; no accepted percentage speedup or C++ budget closure is claimed.

N03 remains open for primitive-part materialization, locale/richer/legacy
formatting and KI-008 hooks. KI-007/KI-009 and N01/N14 qualification remain;
N04 has not started. This shares N05 proof work without renumbering/closing it.
The previous string-transfer commit's Windows/Ubuntu/macOS CI passed; this
change requires its own platform qualification.

## N03: private already-rendered scalar fragments, 2026-10-18

[The scalar-fragment slice](WIO_SCALAR_FRAGMENT_OPTIMIZATION.md) replaces private
sole-consumer integer strings with inline rendered bytes and bool strings with
literal views. Conversion stays at the original instruction, even across
intervening hooks/CFG blocks. Live locale guards remain; custom facets retain
stream formatting and owning fallback. Extra/escaping/metadata/edge uses and
unsafe cleanup retain strings. This changes neither public ABI nor WIR/VM.

Three new/expanded gates pass with native O0/O2 locale-switch/counting-facet
checks, runtime boundary/random/long-NUL formatting and VM mixed-hook NUL/Text
fallback checks. Native bridge and VM host locale globals may belong to distinct
C++ runtimes, so native facet mutation is not asserted as a cross-runtime ABI.
Before/after retain 168/168 correct outputs. Native ordered observations fall
2.1909 -> 1.8798 ms (final 2.9425 -> 1.9695). Work stack reservation falls
584 -> 520 bytes; heap traffic is unchanged, production size -4804 bytes and
the separate diagnostic executable +892 bytes. All timing remains globally
unqualified. C++ is still ahead and its invariant to_chars control does not
implement Wio's whole live-locale contract; no budget closure is claimed.

N03 remains open for locale/richer/legacy formatting and KI-008 hooks. N04 has
not started; N05 sharing does not close/renumber it. KI-007/KI-009 and N01/N14
acceptance remain. The previous initialization commit's platform CI passed;
this change requires its own Windows/Ubuntu/macOS qualification.

## N03: floating hot frame and bounded stream-buffer views, 2026-10-18

[The floating/stream slice](WIO_FLOAT_STREAM_OPTIMIZATION.md) outlines stream
construction/EH state from the normal finite/classic path and shrinks its
precision-6 scratch. The libc++ stream-only policy remains, without a cold
annotation on its usual path. Integer/float fallback share a counted buffer
view only when the target cannot grow; the old owning snapshot remains at
allocation boundaries, preserving native reentrancy safety. Locale, fenv,
nonfinite spelling, partial output, ABI and WIR/VM shape stay unchanged.

Two final gates pass in 85.41s with native O0/O2 + VM and host-stream oracles.
The initial Windows Debug count assumption is retained, not a waived code bug.
GCC long-facet reserved diagnostics fall 256 -> 128 allocations and
99200 -> 65664 requested bytes per 128 calls; growing targets keep the copy
control's allocation shape. Production classic heap does not change. The f64
helper's actual O2 reservation falls 504 -> 112 bytes; Work remains 856 bytes.
Production size falls 1594 bytes. Both sessions retain 252/252 correct outputs.

Comparison native medians fall 0.7818/4.2988/6.3691 -> 0.7533/4.2397/6.1426 ms,
but final mixed builder rises 4.2792 -> 4.4872. Global timing remains
unqualified; the C++ stream reference is not a to_chars lower bound. No general
speedup, RSS win or N14 budget closure is claimed. N03 remains open for
richer/legacy formatting and KI-008 hooks; N04 has not started. The previous
scalar-fragment commit's three-platform CI passed; this change needs its own.

## N03: legacy stream ownership, 2026-10-18

[The legacy extraction slice](WIO_STREAM_OWNERSHIP_OPTIMIZATION.md) takes the
owning result from a finished `runtime::Format` stream instead of copying its
payload. This is the native compatibility helper, not normal interpolation or
indexed console formatting. Wio benchmarks reach it through a test-only native
wrapper; the VM bridge also runs the native helper, not a new VM-core formatter.

Two new gates pass in 13.65s, native O0/O2 + VM bridge and host stream behavior.
The 2001-call allocation workloads fall 4004 -> 2003 and 14009 -> 12008;
requested bytes fall 1466872 -> 1026762 and 81012621 -> 64564511. Final tracked
live bytes remain zero. Ownership transfer retains spare capacity: a 216-byte
GCC result goes from capacity 216 to 512. Native binary size grows 1296 bytes.
No universal retained-memory/size win is claimed.

All 168/168 benchmark outputs are correct. Small native observations fall,
large comparison native rises slightly and both comparison VM observations
rise; global timing remains unqualified. Preserve the mixed results and initial
setup failures. N03 remains open, N04 has not started. The prior floating/stream
commit's three-platform CI passed; this slice needs its own qualification.
