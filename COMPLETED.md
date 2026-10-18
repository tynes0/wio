# Wio Completed Work

This file gathers the completed work that used to be scattered across the old
`TODOLIST.md`.

Notes:

- These items are no longer brand-new work.
- Partially finished but real, landed work is still listed here.
- Anything that still needs hardening continues to be tracked as `[~]` in
  `TODOLIST.md`.

## N04 runtime/native-bridge profile slice — 2026-10-19

- [x] Explicit host-independent backend runtime profiles default to Release/O2;
  Debug O0/symbols remains selectable. Share C++/C99/PIC policy, preserve checks/
  exceptions/FP semantics, track profile changes and install build-profile.txt.
- [x] Byte-I/O benchmark selects an O2 native bridge, records build provenance
  and durations, and rejects reuse that would mislabel known runtime/bridge flags.
- [x] Two local scoped gates pass in 107.91s, including 21 I/O processes and
  O0/O2 native bridges. Runtime-only install/hash/profile checks pass. Archive
  shrinks 7344752 -> 1721974 bytes; no RSS or accepted speedup claim follows.
- [ ] Source CI and final measurements remain separately qualified; N04/N14,
  KI-010 and the remaining buffer/batching/std work stay open.
  Report: `docs/WIO_RUNTIME_PROFILE_OPTIMIZATION.md`.

## N04 first slice: direct owned byte reads — 2026-10-19

- [x] Read Wio unsigned-byte arrays directly into their final native payload,
  eliminating the intermediate std::byte vector and per-byte push conversion.
  Preserve exact/partial/EOF/error/cursor behavior and owning snapshots; compact
  excessive short-read capacity and discard EOF speculative storage.
- [x] New scoped gate passes 1/1 in 115.61s (17 processes): native O0/O2,
  allocation/control tests and VM/native bridge. Isolated full-read allocations
  fall 2 -> 1, requested/peak bytes 131072 -> 65536. No local old full suite.
- [x] Both same-source Wio-controlled sessions have 84/84 correct samples.
  Actual Wio heap allocations fall 771 -> 514 and requested bytes 33697840 ->
  16855088 per 257 reads, ending live-zero. Preserve noisy/unqualified timings,
  increased binary sizes and the unoptimized local runtime/bridge build caveat.
- [x] Source `3c9e268` passes scoped Windows Debug, Ubuntu Release and macOS 15
  Release CI, run 39169819998, including the new N04 gate.
- [ ] N04 remains open. KI-010 computed VM globals are a separately recorded defect,
  not fixed by the identical local-mode workaround in both benchmark fixtures.
  Report: `docs/WIO_BYTE_IO_OPTIMIZATION.md`.

## N03 formatting/builders implementation closure — 2026-10-18

- [x] Complete shared hook rendering for object/nested containers, concrete
  generic owners and synchronous hooks in async callers. Native ToString uses
  explicit owned-receiver native adapters with tested native-to-Wio reentry.
  Native POD scalar/view/array display now has registered formatter thunks.
- [x] Fix emitted interpolation ref/view signatures and separate POD owned-copy
  from destruction across all three verifiers. Reject forged nontrivial/layout/
  object/RC metadata. Skip open generic owners in native thunk/registry emission.
- [x] Forward bare completed results, move private container output buffers and
  preflight single rendered string affixes. Audit indexed formats and remove
  only unused numeric scratch zeroing; ordinary StackText stays initialized.
- [x] Six scoped gates pass in 323.90s; final verifier/POD recheck passes 2/2
  in 23.47s. Initial failures and intermediate candidates remain as evidence.
- [x] Final indexed and native-POD sessions each have 84/84 correct samples.
  POD VM formatting now works; native heap windows remain equal/live-zero.
  Preserve mixed/noisy timings and binary-size costs without speedup claims.
- [x] Source `29fabf1` passes the scoped Windows Debug, Ubuntu Release and
  macOS 15 Release workflow: CI 39149670456.
- [ ] Timing qualification remains open, not a universal speedup or whole-backend
  parity claim. N04 is next; KI-007/KI-009 and N14 remain open.
  Report: `docs/WIO_N03_CLOSURE.md`.

## N03 generated native stream copies and bounded capacity — 2026-10-18

- [x] Optimize actual emitted C++ native stream formatting: move out the finished
  stream, avoid redundant bare assembly with an excessive-capacity guard, and
  reserve once for a rendered single operand plus independent literal suffix.
  Keep private integer/bool fragments, Text and multi-operand ordering intact.
- [x] New/expanded gates pass 2/2 in 31.06s: native O0/O2, actual emitted-helper
  allocation/snapshot/seek/partial/throw tests, 1 MiB reserve compaction, IR
  fragment guards and supported VM builtins. Native POD operator<< rejection
  WVM1062 remains explicit KI-008 work, not VM parity.
- [x] Selected heap counts per 2001 calls fall 8004 -> 6003 / 20010 -> 16008;
  tracked live bytes end at zero. Preserve the unbounded intermediate, whose
  lower bare allocations retained more capacity. Selected bare sample capacity
  stays 217, affix falls 442 -> 225. Isolated affix peak rises 8 bytes and native
  executable grows 3003 bytes; no universal memory/size win is claimed.
- [ ] N03 remains open. Each retained session has 64 correct supported samples
  and 20 VM rejections; global timing is unqualified. No accepted speedup or
  C++ budget closure. Evidence: `docs/WIO_GENERATED_STREAM_OPTIMIZATION.md`.

## N03 legacy stream-result ownership — 2026-10-18

- [x] Extract the owning string from a finished native compatibility `Format`
  stream, avoiding its final payload copy. Retain placeholder/hook order,
  counted bytes, locale/fenv and partial/throw behavior. This is not normal Wio
  interpolation, indexed console formatting or VM-core optimization.
- [x] Two new gates pass in 13.65s with native O0/O2 and VM bridge; standalone
  GCC allocation proof also passes. Initial include/diagnostic-runtime setup
  failures are retained, not used as timing baselines.
- [x] Same-source Wio-owned sessions have 168/168 correct outputs. Allocation
  workloads fall 4004 -> 2003 / 14009 -> 12008 per 2001 calls, ending live-zero.
  Retained capacity can rise (GCC 216 -> 512 for a 216-byte result); the native
  executable grows 1296 bytes. Timing observations are mixed/unqualified, not
  an accepted native/VM speedup or universal memory win. N03 remains open.
  Evidence: `docs/WIO_STREAM_OWNERSHIP_OPTIMIZATION.md`.

## N03 bounded stream views and floating hot-frame reduction — 2026-10-18

- [x] Outline floating stream/EH work and reduce precision-6 scratch 128 -> 32
  bytes, retaining conversion-failure fallback, live locale/rounding and libc++
  stream policy. Share a stream-buffer append helper with integer formatting;
  use a view only for non-growing targets and retain snapshots at allocation
  boundaries. No ABI/WIR/bytecode shape changes.
- [x] Final new/expanded gates pass 2/2 in 85.41s with native O0/O2 + VM,
  boundaries/random bits, rounding modes, custom long/NUL/partial-output facets,
  reserved/growing targets and owning snapshots. The initial MSVC Debug exact-
  allocation-count assumption failed and remains visible; portable bounds pass.
- [x] GCC reserved long-facet diagnostics: 256 -> 128 allocations, requested
  99200 -> 65664 bytes per 128 integer/float calls. Growing target stays equal
  to the copy control. Production classic heap is unchanged. Actual f64 helper
  stack reservation falls 504 -> 112 bytes; Work stays 856. Production binary
  shrinks 1594 bytes. Both sessions retain 252/252 correct outputs.
- [x] All three native comparison observations fall, but final mixed builder
  rises 4.2792 -> 4.4872 ms. Global timing remains unqualified; no accepted
  speedup percentage, VM gain, whole-process memory win or C++ budget closure.
  N03 remains open. Evidence: `docs/WIO_FLOAT_STREAM_OPTIMIZATION.md`.

## N03 private rendered scalar fragments — 2026-10-18

- [x] Keep integer formatting at its original instruction but retain private
  rendered bytes instead of a sole-consumer string. Bool uses stable literal
  views. Custom locales use the same stream formatter and owning fallback.
  Escape/extra/metadata/edge uses, unsafe cleanup and excluded types keep strings;
  ordinary ABI and WIR/VM instructions remain unchanged.
- [x] New/expanded gates pass 3/3 in 145.87s, with runtime boundary/random/
  long-NUL-locale checks, native O0/O2 hook-driven locale/counting facets and
  unchanged mixed-hook NUL/Text/8-bit fallback in VM. Strict GCC syntax and
  standalone runtime unit pass. Initial fixture accessor typo is retained.
- [x] Before/after retain 168/168 correct outputs. Native ordered comparison
  observations: 2.1909 -> 1.8798 ms; separate final 2.9425 -> 1.9695. Work loses
  two optional-string slots, O2 stack reservation falls 584 -> 520 bytes, and
  production executable size falls 4804 bytes. Heap is unchanged; diagnostic
  executable grows 892 bytes. All timing remains globally unqualified: no
  accepted speedup percentage, VM gain or C++ budget closure. N03 remains open.
  Evidence: `docs/WIO_SCALAR_FRAGMENT_OPTIMIZATION.md`.

## N03 direct construction into proven-empty string cells — 2026-10-17

- [x] Construct fresh, sole-use owned string results directly in closed local
  cells. Preserve original RHS/exception/hook order and destination cleanup.
  A bounded may-initialized CFG proof checks the producer, joins loop backedges
  and rejects any possibly-live incoming path; excluded cases retain staging.
- [x] New proof/emitter/runtime gates and the expanded Wio native O0/O2 + VM
  suite pass 4/4 in 88.89s. Cover snapshot/borrow, NUL/long/Text fallback,
  continue/early return, native return/initializer errors and factory failure
  state/retry. Shared fresh-producer classification and strict GCC syntax pass.
- [x] Before/after retain 168/168 correct outputs. Native comparison medians
  0.7707/2.1480 -> 0.6586/2.1031 ms; separate final 0.7730/2.1295 ->
  0.6762/2.0893. Work optional-string slots fall 7 -> 5, O2 stack reservation
  696 -> 584 bytes, executable size -512 bytes. Heap is unchanged and RSS
  slightly rises. Global timing remains unqualified; no percentage/budget
  closure claim. N03, KI-007/KI-008/KI-009 and N01/N14 acceptance remain open.
  Evidence: `docs/WIO_STRING_INIT_OPTIMIZATION.md`.

## N03 sole-use string edge forwarding — 2026-10-17

- [x] Forward fresh owned string producers into one immediately following block
  parameter slot. Remove source/edge staging; preserve destination ownership,
  cleanup, RHS completion/exception order and ordinary ABI. Unsafe cases retain
  simultaneous transfers, including parallel edges and async frames.
- [x] New proof/emitter and expanded Wio native O0/O2 + VM gates pass 2/2 (86.41s),
  with NUL/long strings, snapshots, hooks, native returns/errors and Text fallback.
  GCC syntax checking passes; initial fixture API typo is retained as an excerpt.
- [x] Before/after sessions retain 168/168 correct outputs. Native comparison
  medians: 0.9110/2.4496 -> 0.7787/2.1290 ms; separate final: 0.8710/2.3850 ->
  0.8260/2.1240. Work's actual O2 stack reservation falls 1032 -> 696 bytes;
  executable size falls 3072 bytes. Heap allocations remain unchanged.
- [ ] N03 and C++ performance budget remain open. Global timing qualification
  is false; no accepted percentage speedup or VM gain is claimed. See
  [the report](docs/WIO_STRING_TRANSFER_OPTIMIZATION.md).

## N03 bounded receivers and integer hot/cold paths — 2026-10-17

- [x] Emit implementation-only one-pointer self entries for proven small
  synchronous scalar-field methods; retain ordinary ABI, dispatch, ownership,
  null and callback-borrow checks. Unsafe/reentrant/complex cases retain Place.
- [x] Outline integer stream fallback without caching or weakening live locale
  semantics. Add deterministic boundary/prefix/NUL/locale-toggle checks.
- [x] Final 3/3 relevant gates pass (91.76s), including native O0/O2 + VM and C++
  callback/null/expiry probes. GCC strict header execution and receiver syntax
  checks pass. Retain and fix the initial receiver-prelude ordering failure.
- [x] Before/intermediate/final retain 252/252 correct outputs. Native comparison
  medians: 1.0847/2.7687 -> 0.9826/2.7876 -> 0.8473/2.6165 ms. Allocation traffic
  is unchanged; final executable is 6186 bytes smaller, generated source grows.
- [ ] N03 remains open. Final-repeat single native rises to 1.1231 ms; all
  sessions are globally timing-unqualified. The C++ gap and performance budget
  are not closed. See [the full report](docs/WIO_RECEIVER_OPTIMIZATION.md).

## N03 bounded scalar fields and rendered-string reservation — 2026-10-17

- [x] Emit direct pointers for proven closed primitive Wio field intervals;
  preserve receiver checks and keep escaping/proxy/native/async paths general.
- [x] Preflight two-or-more already-rendered string parts and literal bytes with
  checked arithmetic, then reserve once before assembly. WIR/VM shape is unchanged.
- [x] New proof/overflow units and expanded Wio O0/O2 + VM gate pass 3/3 (78.79s).
  Strict GCC warning-only correction has identical unit O2 assembly; 2/2 C++
  recheck passes. Initial fixture/strict-warning failures remain in evidence.
- [x] Before/field-only/final sessions retain 252/252 correct outputs. Native
  comparison medians: 1.5623/4.0313 -> 1.0518/2.9288 -> 1.0735/2.7163 ms.
  Ordered-case allocations: 15004 -> 10003; requested bytes: 615156 -> 315097;
  zero final live blocks/bytes. No globally qualified speedup is accepted.
- [ ] N03 and the C++ performance gap remain open. Single-case final native
  observations rise between the two slices; no universal no-regression claim.
  See [the full report](docs/WIO_SCALAR_FIELD_OPTIMIZATION.md).

## N03 call-scoped native string transport — 2026-10-17

- [x] Prove bounded synchronous native UTF-8 borrows, promote closed string
  cells to existing inline storage, and avoid general Place transport through
  private C++ reference adapters. Ordinary ABI/function identities remain intact.
- [x] Three new relevant tests passed (65.44 seconds), covering native O0/O2,
  direct VM calls, mutable growth, snapshots, NUL and exception translation.
- [x] Reject the shared-WIR streaming experiment after measurement exposed more
  VM work; preserve its timings and initial ownership-verifier failure.
- [x] Capture frozen before/final Wio-owned comparison and final native/VM phase:
  84/84 correct outputs per session; allocations 10003/25005 -> 2/15004,
  zero final live bytes/blocks. Lower native medians are observed, not a qualified
  speed budget; full timing qualification remains false.
- [ ] Native indirect function values work, but VM reports WVM1003 (KI-009).
  N03, full formatting parity and runtime timing budgets remain open. See
  [the transport report](docs/WIO_NATIVE_STRING_BORROW_OPTIMIZATION.md).

## N03 shared scalar user ToString — 2026-10-17

- [x] Materialize synchronous scalar object/interface hook calls in shared WIR,
  with ordered evaluation/formatting, exactly-once dispatch and explicit cleanup.
- [x] Cover view/temporary receivers, concrete generic std::ToString callers,
  NUL and implicit interface public visibility; native O0/O2 + VM gate passed.
- [x] Two new tests passed locally (63.76 seconds); preceding portability CI is
  green on all three platforms. See [the report](docs/WIO_FORMATTING_CALL_LOWERING.md).
- [x] Final open-generic-owner guard recheck passed 2/2 (98.96 seconds); the
  measured closed-type generated C++ remains byte-identical. Benchmark observes
  higher native times, unchanged allocations and newly working VM cases; no
  performance budget or zero-regression claim is accepted.
- [ ] Object-container hooks, async frames and native/open-generic hook declarations
  remain KI-008/N03 work. This correctness slice does not establish a speed budget.

## N03 libc++ floating display correction — 2026-10-17

- [x] Preserve the host stream spelling on libc++ after CI exposed a finite
  six-digit-formatting mismatch; retain the qualified MSVC/libstdc++ fast path.
- [x] Pin the counterexample as f32/f64 and retain the 60,000-value oracle;
  scope cross-platform allocation-free builder proof to integer/bool leaves.
- [x] Six new N03 tests pass locally (131.76 seconds); failed macOS CI evidence
  is preserved in [the portability report](docs/WIO_N03_PORTABILITY.md).
- [ ] libc++ fast-path optimization and cross-platform CI qualification are
  separate work; N03, KI-008 and N14 are not declared complete by this fix.

## N03 direct primitive builder append — 2026-10-17

- [x] Constrained numeric/bool StringBuilder Append and AppendLine overloads,
      preserving exact integer types, owning snapshots and capacity reuse.
- [x] Dedicated backend-neutral NumericAppendOptimizer fuses only adjacent,
      empty-affix, sole-use primitive formatting/append and matching string
      cleanup. Async/frames, shared values, effects, borrows and user hooks stay
      conservative. C++ and VM append directly without native string marshalling.
- [x] Corrected libc++ const vector<bool> proxy handling after the preceding CI
      run passed Windows/Ubuntu but failed macOS compilation. New CI acceptance
      is independent; local success is not a macOS qualification claim.
- [x] Three new tests passed locally in 64.15s: selection/rejection proof matrix,
      allocation/locale/rounding helper checks and actual Lowered WIR plus Wio
      native O0/O2 + VM, including all allowed primitive types and rejection.
      No old functional corpus run. Earlier development failures remain recorded.
- [x] Same-source four-implementation before/after and final native/VM sessions:
      252/252 correct outputs. Large-integer allocations 40007 -> 20005, requested
      bytes 1631632 -> 1011570, unchanged peak 311 bytes and zero final live memory.
      Short scalar cases have unchanged allocation counts. Both timing sessions
      remain unqualified; separate final growing-row native timing is higher.
- [ ] N03 still open for richer formatting, legacy Format and KI-008 user-hook
      invocation. Native N14 budgets and N01 qualification are separate, not
      silently satisfied by this slice. See `docs/WIO_NUMERIC_BUILDER_OPTIMIZATION.md`.

## N03 float and builtin-container formatting slice — 2026-10-17

- [x] Finite classic-locale/nearest float display uses a stack buffer and
      `to_chars(general, 6)`, retaining explicit stream fallbacks for locale,
      rounding, nonfinite values and unavailable conversion overloads.
- [x] Builtin scalar/string/text/enum containers append directly into generated
      interpolation storage. User/native hooks and non-classic container locale
      behavior retain their existing fallback; public builder APIs are unchanged.
- [x] Corrected native negative-zero constant emission and bounded typed VM
      float/char/array/dictionary display. No bytecode schema or native ABI change.
- [x] Three new tests passed locally; the latest Wio gate passed in 127.77s,
      including actual generated-code-compiler float oracle, native O0/O2,
      shared builtin VM cases and explicit user-hook rejection. No old corpus run.
- [x] Wio-owned frozen before/after comparison and separate final native/VM phase:
      after 126/126 correct; baseline 116/126 with unsupported VM-vector failures
      retained. Vector allocations 50007 -> 30005 and requested bytes 2460358 ->
      1480260; peak live bytes 278 -> 285, final live blocks/bytes zero. Scalar/
      builder allocation counts are unchanged. Both timing sessions unqualified.
- [ ] N03 is not closed: the subsequent direct primitive append slice is above;
      richer formatting,
      legacy stream formatting and VM user ToString invocation remain open.
      KI-008 records the hook gap; N01/N14 timing qualification and KI-007 remain
      separate. See `docs/WIO_FORMATTING_OPTIMIZATION.md` for scoped evidence.

## N02 string/text/concat closure — 2026-10-16

- [x] Ordinary self-assignment and safe direct-cell append reuse; transactional
      multi-append with original-length self aliases, NUL and failure rollback.
      Identical direct field/global cells qualify, not arbitrary proxy/ref cells.
- [x] Pure concat trees reserve one payload buffer, including parenthesized RHS
      trees. Independently owned sole-use temporaries use rvalue concat outside
      pure trees. Matching builtin types, CFG/borrow observations, effects and
      async boundaries remain conservative; generated variadic groups are bounded.
- [x] Text lvalue/rvalue concat overloads avoid prefix-copy/reallocation, preserve
      validated UTF-8 and explicitly guard moved self alias on MSVC/libstdc++.
- [x] Three new tests only, final 3/3 in 19.01s: actual Wio native O0/O2 + VM,
      proof rejection/selection and runtime allocation-failure/alias coverage.
      Subsequent CI run 38965482262 passed Windows Debug, Ubuntu Release and
      macOS Release; no full old-corpus rerun is inferred from that focused gate.
- [x] Wio-owned before/intermediate/final comparisons: 504/504 correct outputs.
      Native allocations growing string/text 2005/4013 -> 16/24; six-part chains
      12009/22038 -> 4005/4029. All diagnostic windows end with zero live blocks.
- [x] N02 production scope closed; the subsequent N03 slice is recorded above.
      All three N02 timing sessions remain globally unqualified; no speedup
      factor, VM optimization or full native/VM parity claim. N01/N14 evidence
      gaps and pre-existing KI-007 remain open. See
      `docs/WIO_CONCAT_OPTIMIZATION.md` for raw data, memory scope and tradeoffs.

## N02 Wio-owned benchmark library and controller — 2026-10-16

- [x] `std::benchmark`: checksum-verified warmup/repetition runs, calibration,
      opaque scalar inputs/observable consumption, min/max/mean/median,
      p90/p95/p99, sample standard deviation/error, MAD and range/drift checks.
      Wio report/protocol/JSON surfaces; no new language syntax.
- [x] Wio-written `wio-bench`: strict manifests, literal argv/no shell, serial
      builds/version capture, source/runtime/toolchain/binary SHA256 endpoints,
      rotating/reversing comparison rounds, native A/A controls and distinct
      final native/VM rounds. Raw/checkpoint JSON and Markdown, compare/report
      commands, explicit process-local suite environment and no result overwrite.
- [x] Native OS boundary: timeout/output limits, ordinary descendant cleanup,
      child CPU and peak RSS, self-process memory flags. Separate opt-in native
      allocation windows cover ordinary/aligned/zero/nothrow/overflow/frees with
      request/live/peak/failure accounting. Not malloc/custom/managed/VM-heap
      attribution or production timing. Scoped Wio example: 9 requests, 4587
      requested/freed bytes, 3652 peak live bytes, zero final live bytes/blocks.
- [x] CMake build/install/distribution integration; static Windows tool linkage.
      Wio project, portable Python bootstrap, four-language real example plus
      optional Python. Required future before/after + final-Wio workflow recorded.
- [x] Tests caught and corrected disappearing pure-compute timing and JSON
      floating zero -> null/precision loss. Locale-independent round-trip number
      serialization, plus new Wio/native/VM and independent Python/C++ checks.
- [x] Final new-only CTest 1/1 passed in 690.91 seconds, including intentional
      stale-root override, failed outputs, descendant cleanup and immutable
      before/after artifacts. No old corpus run. Three-OS CI configured.
- [x] Real 20,000-step comparison: 48/48 outputs correct, endpoint hashes,
      affinity and power observation unchanged. Overall time qualification rejects
      Python's 24.6% range; no samples removed, no general speedup claim.
- [x] The remaining N02 string/concat work was subsequently completed by the
      closure above; this tooling alone was not that closure. Broader native/VM/
      compiler optimization and N01 coverage remain open.
      See `docs/WIO_BENCHMARKS.md` and `tools/wio-bench/README.md`.

## N02 local string/text append reuse — 2026-10-16

- [x] Actual C++ backend/storage-plan optimization: fuse a proven adjacent
      mutable-local load/add/store-or-replace into in-place append. RHS snapshots,
      evaluation order and cleanup remain; fields/globals/proxies/async excluded.
- [x] New Wio semantics run at native O0/O2 and VM, plus narrow selection/rejection
      proof tests. Only these new tests ran locally; cross-platform CI configured.
- [x] Frozen before/after 2000-repeat diagnostics: string allocations 2002 -> 14,
      peak requested bytes 180059 -> 138344; text 4001 -> 14, 419808 -> 129125.
      Final tracked live bytes/blocks zero. String retained bytes increase ~2.4%
      due to reusable capacity: explicit speed/memory tradeoff, not hidden.
- [x] Counter-free paired timings collected with raw samples and A/A control;
      overall timing rejected for baseline/control range noise. No qualified
      speedup factor. Pilot and rejected no-fusion candidate retained locally.
- [x] Broader concat/last-use patterns and direct-field-safe reuse subsequently
      completed by the closure above. N01 residual profiling stays open.
      See `docs/WIO_LOCAL_APPEND_REUSE.md` for the historical first slice.

## N01 observed bulk-copy attribution — 2026-10-15

- [x] Wio string/text/component owning/view/return, concat and scalar-component
      fixtures; native overlap/zero-length calibration called from Wio. Python
      orchestration, independent fingerprints and source-immutability checks.
      No production compiler/runtime/std edits and no new PowerShell logic.
- [x] GCC/GNU-linker memcpy/memmove diagnostic plus ordinary O2 output parity.
      Only the new CTest ran: 1/1, 52.32 seconds, five checks, 78/78 actual Wio
      executions. Count-2000 concat observes 90135003 / 168086004 bulk-copy request
      bytes for string/text; these are not all payload copies or production counts.
- [x] Fresh unchanged-artifact Mandelbrot 32/32 correct. Native compute 51.7406 ms,
      VM 42.6891409 s; A/A and other noise gates reject the session. No compiler
      speedup/regression claim. Prior memory-attribution CI green on all three OSes.
- [x] Current feature-branch CI runs only the new copy test on GCC-supported
      runners; main/release/PR functional and previous diagnostic gates remain.
- [ ] Still N01, not N02: exact optimized/all-inline payload-copy profiling,
      direct malloc/custom allocators, broader coverage and precise timing remain
      open. See `docs/WIO_COPY_ATTRIBUTION.md` and raw evidence.

## N01 live/peak memory attribution — 2026-10-15

- [x] Wio retained object/string/byte-array workloads and native controls called
      from Wio; Python orchestration and independent checksums. Diagnostic-only
      ordinary/aligned allocation/free live/peak window accounting; process
      memory checkpoints for Windows/Linux/macOS. No production changes or PS1.
- [x] Known-size/control alignment, zero-byte, nothrow/checked-overflow failure,
      pre-window frees and weak-delayed physical release. Only the new CTest ran:
      1/1, 8.75 seconds, five checks and 18/18 real Wio executions. All final
      window-owned live bytes/blocks zero; immutable inputs/artifacts unchanged.
- [x] Fresh unchanged-binary Mandelbrot 32/32 correct: native compute 24.0372 ms,
      VM 20.6477775 s. Overall timing rejected for noise/drift, with all samples
      retained. No speedup/regression claim. Previous Ref-attribution CI green.
- [ ] Still N01, not N02. Payload-copy bytes, direct malloc/custom allocator
      attribution, broader subsystem coverage and precise timing remain open.
      See `docs/WIO_MEMORY_ATTRIBUTION.md` and its raw evidence.

## N01 Ref ownership attribution — 2026-10-15

- [x] Wio owning-argument/view/return workloads; native weak-lifetime and explicit
      copy/move control called from Wio; Python-only orchestration. GCC/gcov
      diagnostic attribution, no production runtime/compiler changes or new PS1.
- [x] Calibrated retain/release/weak/destruction and Ref handle-transfer counts at
      1, 17, 2000; ordinary O2 checksum parity. Only the new CTest ran locally:
      1/1, 17.64 seconds; five checks and 30 actual Wio workload executions.
- [x] Fresh unchanged-binary Mandelbrot: 32/32 correct outputs. Native compute
      23.9045 ms, VM compute 20.5566244 s; full-VM range/drift fail timing gates,
      so overall timing is rejected without trimming/retries. No speedup claim.
- [ ] Still N01, not N02: general copied-payload bytes, live/peak memory, broader
      subsystem coverage and precise baselines remain open. Ref counts are from
      non-inlined diagnostics, not exact shipping-binary event attribution.
      See `docs/WIO_OWNERSHIP_ATTRIBUTION.md` and its raw evidence.

## N01 Wio + Python measurement path — 2026-10-15

- [x] `33f2c1b`: prefer Wio behavior/workload fixtures with Python orchestration.
      Both original Wio suites are unchanged. Native-cost PS entries are now thin
      compatibility forwarding only; removed the just-added synthetic C++/PS test
      (recoverable in Git). Old unrelated scripts were not bulk-converted.
- [x] Python affinity/worker isolation, partial timeout output, descendant cleanup,
      immutable build/runner/input hashes, explicit counter mode and independent
      extended fingerprints, separate output/timing acceptance. Windows/Linux
      support hard affinity; macOS explicitly collects unpinned correctness and
      never fabricates a timing-qualified result.
- [x] Only the new Python CTest was run: 1/1, 55.66 seconds, 26 checks with no
      skips, including both real Wio builds and 128/128 checked executions at count
      17. No old corpus. Final count-2000 runs: 720/720 outputs correct; allocation
      evidence matches previous instrumented baselines. All three microcase timing
      sessions are rejected without trimming/retry-until-pass.
- [x] Frozen-artifact Python Mandelbrot: 32/32 output checks; unchanged hashes and
      endpoint power state; all coarse timing gates pass. Native compute median
      23.6669 ms, Release VM 20.5034887 s. This is not a compiler/runtime optimization
      or 5% accuracy claim; child-wall boundary differs from old external timing.
- [ ] Opening/closing stage remains N01. Copy/move/RC attribution, live/peak memory,
      broader subsystem coverage and precise microcase baselines remain open;
      N02 has not started. See `docs/WIO_NATIVE_COST_CONTROLLER.md` for evidence.

## N01 measurement acceptance correction — 2026-10-15

- [x] `4718f82`: exact-binary Mandelbrot remeasurement, pre-launch inherited
      affinity/restoration, VM/native warm-ups, rotating/reversing order and
      same-executable A/A controls. The build runner delegates to this controller.
- [x] Preserve partial/failed/timeout evidence; check binary/controller/provenance
      hashes and endpoint power scheme; reject incomplete/unstable experiments.
      Coarse noise limits are not statistical confidence or 5% accuracy claims.
- [x] Only the new `wio_measurement_contract` was run locally: 1/1 in 6.02 seconds,
      38 assertions including real child inheritance and failed-process evidence.
      A separate real VM timeout check terminates its child and retains failure.
      The new contract also passes on Windows Debug, Ubuntu Release and macOS
      Release (3/3 CI executions, run 38759859521); no CI speed gate is claimed.
- [x] Retained all 93 manual diagnostic rows, including the expected timeout.
      The final native experiment passes all 40 output checks and immutable hash
      checks but fails noise limits. Active competing application CPU usage was
      observed; no process/power/priority configuration was modified. No production
      optimization or performance regression attribution is claimed.
- [ ] A quiet stable baseline, native-cost runner controller migration and the
      other N01 attribution/coverage gaps remain open. VM development observations
      are quarantined, not final-controller performance acceptance. See
      `docs/WIO_MEASUREMENT_QUALIFICATION.md` for ranges, references and raw evidence.

## N01 extended coverage and counter-free timing — 2026-10-15

- [x] `5f40405`: six new Unicode, Dict, Result-error, native weak-reference,
      scalar-capture and ready-coroutine diagnostics; original nine core
      workload sources unchanged. Source/ABI/cancellation boundaries and
      remaining coverage gaps are explicit in `docs/WIO_NATIVE_COST_EXTENDED_MAP.md`.
- [x] Separate counter-free mode compiles out diagnostic replacement allocation
      operators, uses standard allocation, and marks unmeasured metrics null.
      It is not a production allocator/runtime change or a speedup claim.
- [x] Final local new test: 1/1 in 140.03 seconds, O0/O2 plus counter-free O2,
      independent fingerprints at counts 1/17, counter/input guards. New gate
      passed on all three clean CI platforms (3/3 executions, run 38754331032).
      No old corpus rerun. Final manual samples: 384/384 independently qualified
      outputs across the two modes, with all ranges/outliers retained.
- [x] Fresh native/Release-VM Mandelbrot: 24/24 checks passed. This session has
      substantial timing drift despite identical compiler/runtime/VM-host hashes;
      the prior C++ control is also slower. Preserve all results and report no
      attributed speedup/regression. Measurement calibration remains part of N01.
- [ ] N01 copy/move/RC-event attribution, peak/live memory and suspended/worker/
      cancellation, broader containers, startup/reflection and I/O baselines
      remain open; N02 has not started. Missing metrics are not zero.

## N01 native cost-map foundation — 2026-10-15

- [x] `ae12526`: fixed native substeps N01-N14 and recorded std API/module
      deletion/consolidation policy (candidate: sort into algorithms), with
      dependency/import/API migration before removal. No production changes.
- [x] Added nine native/reference workloads, isolated scratch emission and
      repeatable alternating-order runner with checksums, source/binary hashes,
      dirty-state evidence and explicit failure/timeout handling. Ordinary
      new/new[] call/requested-byte metrics are not copied bytes, RC events or
      peak memory. See `docs/WIO_NATIVE_COST_MAP.md` for baseline and caveats.
- [x] Final local new contract test: 1/1 passed in 32.41 seconds, covering O0/O2,
      nine workload fingerprints and counter/input validation. Manual baseline:
      288/288 checksum-qualified executions (15 measured pairs per case plus
      excluded warm-ups). Only this new test ran; the old corpus was not rerun.
- [ ] N01 remains in progress: async, weak refs, map/set/Unicode/error and startup
      baselines, copy/move/RC attribution and peak/live-memory measurement remain
      explicit gaps. N02 optimization is not implemented in this slice.

## Playground backend and profile selection — 2026-10-15

- [x] `670fd04`: launcher accepts native/VM and Debug/Release flags, forwards
      application arguments without a shell and prints host/application profiles.
      Rider Program arguments and Visual Studio Command Arguments are documented
      in `playground/README.md`; `--dry-run` previews commands without execution.
- [x] Native profiles pass real optimization/debug flags. VM profiles build and
      select the corresponding runtime configuration, rejecting incompatible
      single-config CMake selections. Backend/profile output directories are
      isolated; compiler-argument signatures invalidate stale artifacts and are
      invalidated before compilation so failures cannot preserve success stamps.
- [x] Only the three new local tests were run: 3/3 passed in 28.30 seconds,
      including actual native and VM Debug/Release execution, literal arguments,
      incremental reuse, flag changes and recovery after failed compilation.
      Tests use scratch projects and do not overwrite user playground sources or
      output. Production VM instruction limits remain unchanged.
- [~] Three-platform qualification was dispatched in GitHub Actions run
      `38721548440`; local results do not imply Linux/macOS qualification. This
      tooling change has no new runtime-performance measurements.

## Native optimization pass 1: general string fast paths — 2026-10-15

- [x] `d6cdbd3`: interpolation appends integer/string/text/bool/char directly,
      without the extra stringify-owned string. Preserve locale/byte/enum and
      float/container fallback behavior; retain public value-argument semantics.
- [x] Generalize sole-use adjacent borrowed string snapshots to known observers
      and string binary operations inside multi-block functions. Count uses over
      the whole CFG; preserve copies for unknown/mutating/intervening/capture/
      cleanup/phi/async cases. No std-name or Mandelbrot recognizer was added.
- [x] Only two new tests run locally: plan/locale/ownership proof and actual
      native O0/O2 plus VM+bridge semantics, 2/2 in 11.30 s. Windows O0/O2 search
      loop: two total allocations for initial value args across 1,000 iterations;
      maximal-u64 render/native-check path retains 2,000 for 1,000 operations.
      VM checks semantics, not the DLL allocation counter's view of the host heap.
- [x] Clean real-CLI CI passes two new gates on Windows Debug (19.18 s), Linux
      Release (12.59 s), macOS Release (11.29 s), 6/6 executions. Older corpus was
      not rerun; diagnostic reference validation remains 693 IDs/3 dynamic-only/
      49 std Result construction sites.
- [x] Fresh 15-round paired long-string interpolation: previous 4.016 ms/current
      3.241 ms, 19.30% less measured time; allocation calls 60,020 to 30,020 with
      identical 960,000-byte fingerprint. Still 5.63x the C++ direct-append lower
      bound, not final native budget closure.
- [x] Preserve fresh Mandelbrot results and both native experiments: initial
      five-round current 135.035 ms versus previous 132.854 ms (adverse 1.64%);
      subsequent native-only, alternating-order 15-round recheck current
      131.769 ms versus previous 132.043 ms (0.21% favorable, overlapping ranges).
      No meaningful image-render gain or certain regression is claimed. Native
      compute median 23.850 ms; three Release VM samples give 19.077921 s kernel
      and 20.570923 s full output. All checksums match; fixed dated language
      reference rows are retained, not presented as fresh measurements.
- [~] Remaining owning argument/effect/alias work, direct numeric-to-builder
      output, ordinary += and broader aggregate/std costs remain pass 1 work.
      VM and compiler-throughput heavy work remain passes 2/3. See
      `docs/WIO_NATIVE_STRING_FAST_PATHS.md` and its three new evidence files.

## Native optimization pass 1: integer interpolation slice — 2026-10-15

- [x] `9554b63`: use bounded to_chars integer formatting in the classic locale,
      preserve custom global grouping through the prior stream path, and leave
      bool/char/float/object/container formatting contracts unchanged.
- [x] Accumulate interpolation parts in one owning string with explicit literal
      byte lengths, fixing the existing NUL prefix/suffix truncation. New native
      O0/O2 and VM tests cover extrema, Unicode, nesting and long pieces; the
      standalone formatter compares boundaries/random values and custom locales
      against stream output. Local new tests: 2/2 in 5.31 s; older corpus not run.
- [x] Clean platform CLI CI at `9554b63`: two new tests pass on Windows Debug
      (8.18 s), Linux Release (3.33 s), macOS Release (4.16 s), 6/6 executions.
      Diagnostic reference validation remains 693 IDs/three dynamic-only/49 std
      Result construction sites. Remote performance is not inferred from CI.
- [x] Add the manual per-update native/Release-VM Mandelbrot runner and an
      excluded-from-default-build diagnostic VM host with an explicit finite
      10-billion instruction budget. Production runner defaults are unchanged.
- [x] Fresh compute: native 23.300 ms (five measured rounds), Release VM
      19.0023835 s (one observation). Same three-field numeric fingerprint.
- [x] Fresh full PPM: native 128.437 ms versus recompiled previous full snapshot
      `ddfa2b2` at 250.618 ms, 1.951x faster; Release VM 20.5356141 s. Every full
      image has the same 5,156,528-byte hash. The paired native gain includes both
      string storage and interpolation work since that saved snapshot, not solely
      this commit; there is no immediate-previous-commit full-render comparison.
- [x] Preserve dated C++/C#/Python reference rows and raw compute samples for
      every update's report. Keep compute kernel and full-output wall metrics
      separate and single VM observations explicit. See
      `docs/WIO_INTEGER_INTERPOLATION_OPTIMIZATION.md` and raw evidence.
- [~] Owning argument/alias proofs, direct numeric append/buffers and the rest
      of native/std pass 1 remain open. The large measured VM overhead is a real
      pass-2 baseline, not a Debug artifact, fixed result or new regression claim.

## Native optimization pass 1: closed string storage/copy slice — 2026-10-15

- [x] `79568bd`: prove closed synchronous string cells and select inline
      initialized storage without a heap cell/optional/function wrapper. Keep
      escaping/captured/borrowed cells and async frames on the established path;
      preserve clear/reinit/move-out and checked empty reads.
- [x] Elide only single-use adjacent borrowed string snapshots feeding Append,
      with no WIR identity, ABI or owned-value snapshot contract change.
- [x] Two new local tests pass 2/2 in 29.92 s: storage proof and resource/throwing
      cleanup; actual native O0/O2 and VM+bridge semantics. Native short-string
      echo has zero allocation calls across 1,000 loop iterations in both modes.
      This counter does not establish zero VM allocations.
- [x] Clean CLI/platform CI passes both new tests on Windows Debug (43.10 s),
      Linux Release (29.52 s) and macOS Release (24.26 s): 6/6 executions.
- [x] Fresh paired append evidence, 15 measured rounds per variant: original
      543.757 ms, previous 5.095 ms, current 3.221 ms, C++ 0.552 ms. Current is
      36.78% less time than previous and 168.81x faster than original in this
      session; new/new[] calls fall from previous 90,020 to 30,020 (C++ 17).
- [~] Still 5.84x C++ on this append case; remaining owning argument/copy/format
      work is open. General aggregate/async promotion and complete pass budgets
      are not closed. KI-006 records an escaping-local-borrow diagnostic gap,
      not a fix or a new inline-storage lifetime extension. User changes remain
      untouched. See `docs/WIO_MANAGED_STRING_OPTIMIZATION.md` and raw evidence.
- [x] Record the requested post-three-pass C++/Wio native/Wio VM/C#/Python
      Mandelbrot comparison as a final acceptance gate, with separate compute,
      format/buffer and full PPM measurements; do not call it a fourth pass.

## Coordinated std consolidation: builder/binary slice — 2026-10-15

- [x] Integrate std readability/API/performance work module by module into native
      optimization pass 1, keeping VM heavy optimization and compiler throughput
      as passes 2/3 and full SDK kit parity as a separate v1 gate.
- [x] Apply the accepted source rules to builders/binary in a readability-only
      commit. Follow with Result-first ByteReader read APIs, explicit IO read
      codes 1101/1102/1103, cursor rollback, no Try compatibility API, and the
      binary consumer migration. Explicit user Option remains supported.
- [x] Preserve string append/line/clear capacity; add reserve hints, bulk byte/
      string conversion and read-only native input borrows. VM append/push/clear
      mutates the actual inline owned string without copying its growing prefix.
- [x] Only three new local gates run: native/VM Result/snapshot/UTF-8/NUL/endian/
      varint/bulk/binary behavior, scoped style guard, VM capacity/alias/failure
      behavior. Final local Windows Debug run: 3/3 in 24.02 seconds.
- [x] Clean final CI at `ddfa2b2` builds the actual CLI and passes all three new
      tests on Windows Debug (46.32 s), Linux Release (15.68 s) and macOS Release
      (13.54 s): 9/9 executions, with logs linked in the module report.
- [x] Preserve 15-round paired append evidence: old 389.139 ms, current 4.897 ms,
      C++ 0.584 ms. 79.47x faster but still 8.39x C++; ordinary allocation calls
      fall from 120,004 to 90,020 (C++ 17), exposing remaining managed-arg costs.
- [x] Requalify native full PPM completion: 247.528 ms median wall time versus
      C++ 55.233 ms, seven interleaved measured rounds and identical 5,156,528-byte
      image hash. This is 4.48x C++, not final budget acceptance or an exact gain
      inferred from the historical 300-second timeout.
- [~] Managed argument/temp storage, direct numeric formatting, string +=,
      ByteBuffer/BytePool and the remaining std/intrinsic surface remain open.
      User-owned std/buffer and playground changes were not modified/committed.
      See `docs/WIO_STD_BUILDERS_CONSOLIDATION.md` and its committed raw evidence.

## Native optimization pass 1: scalar storage/CFG slice — 2026-10-15

- [x] Add a conservative function-local storage plan: closed primitive cells use
      inline initialized storage without optional/function/shared allocation;
      escaping/borrowed/captured/managed/coroutine state keeps the prior model.
- [x] Emit plain trivial scalar SSA slots and direct synchronous CFG edges;
      preserve checks/cleanup and simultaneous block-parameter transfer.
- [x] Two new local gates pass 2/2 in 9.48 seconds, including O0/O2 zero-loop-
      allocation probes, initialized/clear reads, native ref/capture fallback,
      early loop exits and verified self-loop phi swaps.
- [x] Preserve 15-round paired native benchmark evidence: original 774.603 ms,
      intermediate 699.159 ms, current 23.163 ms, C++ 22.553 ms. Current is
      33.44x faster than original, 1.027x C++ for compute only. Every checksum
      matches; source/flags/runtime hashes and raw samples are committed.
- [x] Independently instrument generated-entry allocation calls: original
      15,454,738 ordinary new/new[] calls versus 2 currently, with all three
      checksums preserved. Instrumented timing is not mixed into the medians.
- [x] Clean CI at `722e836` builds the real Wio CLI and passes both new tests
      on Windows Debug (12.26 s), Linux Release (8.79 s) and macOS Release
      (8.46 s): 6/6 executions. See `docs/WIO_NATIVE_SCALAR_OPTIMIZATION.md`.
- [~] Full generated/runtime pass remains open: string/buffer formatting,
      aggregate/managed/container/async/application costs and support pruning.
      VM runtime and compiler throughput remain later passes. No full old test
      corpus, universal 5% budget, PPM timeout fix or release is claimed.

## Planning decision: SDK kit boundary — 2026-10-15

- [x] Separate full SDK kit parity from compiler/VM/backend closure. The existing
      v1 SDK gate owns complex/generic/nested host values, host ref/view/callback
      APIs, complex async results, application hosting, detailed reflection and
      reload/capability coverage.
- [x] Retain common low-level ABI/native bridge, runtime ownership/thread/error
      safety, artifact production/CLI integration and cost/platform acceptance
      inside backend closure. Product version alignment remains unchanged.
- [~] This is a planning reclassification, not an implementation or qualification
      claim. Outstanding kit work is still required before v1. Historical slice
      records below retain their original scope; the later boundary governs
      current milestone/reporting decisions.

## VM SDK shared/static module foundation — 2026-10-14

### Follow-up: final functional platform closure — 2026-10-15

- [x] Complete the three agreed pre-optimization tasks: project CLI integration,
      declared native distribution policy and focused platform/behavioral
      qualification. SDK kit parity remains a separate v1 blocker; broad
      architecture/configuration portability remains its own roadmap work.
- [x] Fix NI-011 using isolated ASCII external-tool output paths without
      changing cwd/relative arguments, then native filesystem copy and atomic
      publication. Static archives now publish safely too. Known temporary files
      clean up; unknown user tool side outputs are preserved.
- [x] Fix actual CI portability blockers: GCC dependent-template syntax,
      missing libc++ floating from_chars via a shared bounded C-locale fallback,
      Darwin stdio API availability/timeval width, and Mach-O library identity/
      private-library lookup. Native floating implementations keep the direct
      path; this is correctness/portability work, not a heavy optimization pass.
- [x] Four new gates passed locally on Windows Debug (4/4, 78.31 seconds) and
      on three clean CI builds of b4c552f (12/12 executions): Windows Debug
      65.88 seconds, Linux Release 22.19 seconds, macOS Release 28.60 seconds.
      Covers runtime-only relocated SDK prefixes, Unix DESTDIR, native lifecycle/
      rollback and private DLL/SO/dylib, Unicode native outputs, failed rebuild
      preservation, native/VM float parsing, shared/static CLI and source/SDK-
      independent packaged execution. No old full corpus was rerun.
- [x] Pin successful CI evidence in `docs/WIO_BACKEND_CLOSURE_QUALIFICATION.md`.
      Diagnostic reference validation remains 693 IDs. Functional backend item 8
      is closed within its qualified boundaries. Next: generated C++/runtime
      heavy optimization, then VM runtime, then compiler/preparation/build time.

### Follow-up: declared native distribution policy — 2026-10-15

- [x] Freeze explicit registry/private-library roles across executable/shared/
      static VM artifacts. No import-table discovery or live-generation pruning
      is promised. Old cohorts remain intentionally retained; fresh deployment
      copies only the selected generation.
- [x] Executable packaging now accepts `--runtime-library`; project CLI forwards
      `vm.runtimeLibraries`, tracks changes and protects their source directories.
      V2 cohorts preserve original native import filenames and bind dependency
      role/path/size/hash. V1 producer-only format remains supported.
- [x] Verify every v2 dependency before embedded execution, `--verify` or deploy.
      Reject corruption, conflicting files, unsafe names, noncanonical records,
      reserved output names and escaping/dangling symlinks. Missing files repair
      from current inputs; failed updates preserve the prior image.
- [x] Only two new Windows Debug gates ran: `wio_vm_artifact_cohort` and
      `wio_vm_executable_cohort`, 2/2 in 13.74 seconds. They cover real registry +
      private DLL calls, Unicode deployment/project paths, ASCII build directory,
      fresh renamed deployment, source-independent execution, corruption and
      missing-helper CLI repair, retained generations and v1 compatibility.
      Symlink escape ran successfully in the local unit gate.
- [~] The initial integration run exposed MinGW failure for emoji-containing
      native build-output paths. Tracked explicitly as NI-011 for the next
      platform closure step; using an ASCII build directory is not a fix for it.
      The older test corpus, Linux/macOS and Release configuration were not run.
      Full SDK kit parity remains a separate v1 blocker. See
      `docs/WIO_NATIVE_DISTRIBUTION_POLICY.md`.

### Follow-up: project CLI library and host integration — 2026-10-15

- [x] VM project build/run/package accepts shared/static modules, optional native
      hosts and bare libraries through a data-only CMake plan and installed runtime.
      Native-backend outputs stay separate; compiler/configuration compatibility
      remains explicit rather than silently linking incompatible VM archives.
- [x] Add `--bytecode-sidecar-kind program|library`, preserving the executable
      default while retaining a true library identity for VM wrappers. Invalid
      values and use without a sidecar reject before generation.
- [x] Record/verify module, host and native dependency hashes for no-build run/
      package. Missing bundles repair without relinking an up-to-date image;
      failed record publication preserves the prior metadata.
- [x] Shared packages retain module-relative bundles; static-host packages retain
      final-image bundles; bare archive packages remain explicit SDK link inputs.
      POSIX launchers now forward package-relative shared-library arguments too.
- [x] Two new Windows Debug gates passed 2/2 in 91.65 seconds: shared/static hosts,
      bare libraries, relocated distributions, independent static-archive consumer,
      Unicode paths, NUL-bearing result ownership, no-build without source/bytecode,
      missing/corrupt inputs, path/hash policy and executable sidecar compatibility.
- [x] Record the three subsequent optimization passes: generated C++/runtime
      (measured 5-10% overhead target where feasible), non-JIT VM runtime, then the
      entire compilation/preparation pipeline. This records intent, not speed gains.
- [~] Functional closure steps 2 (distribution policy) and 3 (broader platform
      qualification) remain open. No old/full corpus or Linux/macOS pass is claimed;
      SDK kit parity stays a separate v1 blocker. No release is claimed.

### Follow-up: final-image install and explicit repair — 2026-10-15

- [x] `wio_install_vm_module` installs shared images or a static module's final
      executable/shared HOST with validated native bundles, relative destinations,
      named components and CMake install-manifest data entries.
- [x] Native validation/copy precedes image replacement; reinstall restores missing
      files but rejects conflicting existing bytes. Runtime and optional development
      import-archive components are separate.
- [x] Optional `DEPLOY_TARGET` creates an explicit repair target that restores an
      absent build-tree generation without changing/relinking an up-to-date image.
      Repeated module/host registration keeps one post-link deployment hook.
- [x] New installed-runtime workflow tests invalid arguments, shared/static repair,
      Unicode prefix relocation, repeat install, missing/conflicting files and a
      real private-library rebuild. Retained old and updated hosts execute in
      separate processes after original producer/bytecode inputs go offline.
      The new Windows Debug gate passed 1/1 in 52.14 seconds; older tests were not run.
- [~] Qualification is focused Windows Debug only. `DESTDIR`, macOS bundles and
      framework deployment are not locally qualified; bundles/frameworks reject.
      Automatic discovery, project CLI library targets, generation collection and
      complex/application SDK parity remain open. No release is claimed.

### Follow-up: declared native dependency bundles

- [x] Installed shared/static CMake wrappers accept qualified registry producers
      and explicit private runtime libraries, staging SHA-256 content generations
      without build-machine paths in generated code or distribution manifests.
- [x] Image-relative runtime resolution is independent of CWD. Shared wrappers
      deploy beside the module; static wrappers explicitly deploy beside the final
      host rather than the intermediate archive.
- [x] Per-session native registration precedes token publication and user Load;
      failure rolls back the private VM. Native-dependent legacy default getters
      reject to keep cleanup out of static/DLL teardown.
- [x] Windows loaders use preferred-form absolute UTF-16 paths and suppress native
      load error dialogs while preserving error state. Private sibling DLL loading
      was reproduced and fixed through the new installed-host test.
- [x] Portable filename, duplicate, traversal, symlink-parent, stage-hash and
      existing-generation collision guards preserve older library bytes.
- [x] Three new Windows Debug gates passed 3/3 in 30.64 seconds; the bundle-policy
      gate also passed after adding the nonexistent-child symlink-parent case.
      Installed-only relocated shared/static hosts cover private DLLs, independent
      sessions, rollback, last-session producer shutdown and escaped string owners.
- [~] Transitive discovery, automatic install/repair, project CLI library targets,
      old-generation collection and Linux/macOS qualification remain open.
      This is not full SDK parity, runtime library authentication or a release.

### Follow-up: same-session owning task inputs

- [x] Synchronous/async and concrete generic exports accept qualified owning
      task parameters, sharing task identity without waiting or consuming the
      host's claim. Suspended frames retain inputs after host value release.
- [x] SDK and raw call-table paths reject other sessions/generations, forged
      identities/descriptors and borrowed/consumed task claims before the body.
- [x] Generic canonical ABI validation now matches ordinary exports; legacy
      C ABI rejection remains. Parameter-only task/payload identities are emitted.
- [x] Generic relay, cancellation direction and fault propagation use existing
      VM await semantics, with exactly-once task state and post-close value/code
      ownership. Source wires may be released off-thread after shutdown.
- [x] Two new Windows Debug gates passed 2/2 in 23.17 seconds: 100 import/lifetime
      repetitions and relocated shared/static hosts, including generic relay,
      enum/flagset/void/Unicode inputs, negative raw ABI checks, parameter-only
      reflection and unsupported complex-task module rejection.
- [~] Foreign task creation/transfer and complex/nested payloads remain open;
      focused Windows gates are not complete SDK/platform/corpus qualification.

### Follow-up: owning async session task boundary

- [x] Additive task sidecar v1 reuses the canonical task-source wire without
      changing session v1, native value v2 or bytecode versions.
- [x] Concrete async exports/generic identities return pending tasks with
      scalar/string/text/enum/flagset/void payloads, not placeholder values.
- [x] Explicit affine poll/read/cancel/main pumping, bounded wait and once-only
      completion subscriptions preserve provenance and callback/value leases.
      Timeouts do not cancel; owner callbacks may read settled results.
- [x] Exact canonical AsyncSleep and bounded RunAsync bindings can use VM
      services without a producer; ordinary foreign symbols still need one.
- [x] Async quiescence precedes unload, workers/timers join during session
      close, and escaped wires retain immutable metadata without a live VM.
- [x] Two new Windows Debug gates passed 2/2 in 36.67 seconds: relocated
      shared/static async hosts, negative ABI/provenance/reentry checks, 80
      shutdown race repetitions and nonblocking queries behind a native barrier.
- [~] Qualification uses only the two new Windows Debug gates; complex/async
      payload parity, application hosting, native dependency/project packaging
      and broader platform/corpus acceptance remain open.

### Follow-up: independent affine sessions and lifecycle

- [x] Additive session descriptor v1 opens independent VM instances per SDK
      module open/attachment, including multiple instances of one DLL or static
      wrapper, without changing existing call/value/bytecode versions.
- [x] Synchronous start/update/API-version/unload and scalar/string/text/enum
      state snapshots/restore validate signatures and exact state payload types.
- [x] Idempotent start/stop, failed-load rollback, exactly-once unload, unload
      failure destruction and active-call pinning cover reentrant view closure.
- [x] SDK results/snapshots retain producer code, not closed VM instances.
      Off-thread last views retire to the owner-thread collectSessions safe point.
- [x] Two new Windows Debug gates passed 2/2 in 28.28 seconds, including
      installed relocated shared/static modules, independent globals, snapshots,
      reentrant closure, failure cleanup and deferred owner-thread retirement.
- [~] Async/complex/application exports, native dependency/project library
      packaging and fresh Linux/macOS/full-suite qualification remain open.

- [x] Compiler-independent ModuleAdapter owns decoded bytecode, Machine,
      stable SDK export descriptors, reflection identities and call contexts.
- [x] Concrete synchronous functions/generic specializations support owning
      scalar/string/text/enum/flagset calls using existing native value ABI v2.
      Unsupported signatures and lifecycle/async hosting reject explicitly.
- [x] NativeModule::attach requires a lifetime owner; returned SDK values and
      clones pin the attachment/shared library after the host view closes.
- [x] Relocatable installed WioVmRuntime CMake targets and wio_add_vm_module
      produce real compiler-free shared/static modules with embedded bytecode.
      Static getters have unique names and independent VM instances.
- [x] Two new Windows Debug gates passed 2/2 in 40.89 seconds, including
      installed-only relocated hosting, generic/enum/flagset/void exports,
      Unicode/NUL storage, panic recovery, wrong-thread entry and invalid modules.
- [~] Async/complex/application exports, native dependency deployment, project
      CLI library targets and Linux/macOS acceptance remain tracked in Backend
      item 8. Session/lifecycle closure is recorded in the follow-up above;
      this is not complete SDK parity or a release.

## v0.15 attribute foundation sprint

- [x] Declaration-leading `[Attribute]` syntax supports stacked and grouped
      lists while preserving legacy input and canonical `using` activation.
- [x] Bracket attributes retain target identity on ordinary declarations,
      members, parameters, enum/flagset cases, applications, systems,
      lifecycle handlers, and application/system fields.
- [x] User attributes compose other attributes with compile-time parameter
      substitution, default materialization, recursive expansion, and cycle
      diagnostics.
- [x] Effective-set contracts cover targets, requires, requires-any,
      conflicts, only-with allow lists, exclusive groups, cardinality, and
      before/after ordering cycles.
- [x] `std::attribute` publishes processor phases, meta-attribute policies,
      and declarations for the current compiler attributes.
- [x] Processor declarations are checked against exactly one validation,
      derive, pre, post, finally, or around capability interface.
- [x] Built-ins and user attributes share stable contract identities for
      target, requirement, and conflict matching.
- [x] Bounded validators execute at compile time with constant diagnostics;
      synchronous pre, post, and exactly-once finally hooks execute on Wio
      function bodies with explicit diagnostics for still-closed edge cases.
- [x] Behavioral processors honor topological `Before`/`After` dependencies;
      source order is deterministic for unrelated processors and exit hooks
      unwind in reverse order.
- [x] Unit synchronous around hooks own a guarded zero-or-one `Proceed`;
      duplicate calls and use after the around invocation are rejected without
      exposing mutable AST or token rewriting.
- [x] Typed synchronous around hooks map or replace exact return values;
      post processors observe typed results, finally processors observe normal
      versus exceptional completion, and pre/post/finally hooks execute inside
      async coroutine frames. Async around is explicitly rejected.
- [x] Object-method pre processors may receive the current receiver as `any`;
      boolean pre guards can skip unit-returning methods, while receiver use on
      free functions and result-bearing guards fail during analysis.
- [x] Method reflection exposes behavioral processors in effective execution
      order, including attribute identity, processor type, phase, hook, and
      invocation mode; `Describe<T>()` includes the same pipeline per method.
- [x] Bounded checked derives expose marked processor methods as real members
      on concrete and generic component/object targets. Every concrete generic
      instantiation inherits its generic primary declaration surface.
      Calls receive the target through a hidden `any` receiver or an immutable
      typed contract view from `DeriveProcessor<TTarget>` and use an isolated
      default-constructed processor. Incompatible targets, unsafe method
      shapes, conflicts, and ambiguous derives fail during analysis instead of
      becoming silent metadata.
- [x] Runtime type reflection exposes structured attribute descriptors with
      stable FNV-1a identity, retention, origin, normalized arguments, and
      default provenance while retaining the legacy string view.
- [x] The focused processor foundation matrix passes 29/29 new tests.

## v0.13 language coherence and SDK parity sprint

- [x] The normative Wio 0.13 coherence specification freezes first-class
      Unicode `text`, `const string`/`const text`, textual const generics,
      fixed-array extent inference, compact typed attributes, named attribute
      arguments, generic extensions, deterministic partial specialization,
      and guarded/exhaustive matching behavior.
- [x] Canonical `with`/`using` native metadata is emitted by project and
      binding generators, while legacy `@Attribute` source remains accepted as
      compatibility input.
- [x] Source-tree validation pins its own runtime, std, and SDK roots so an
      older installed Wio cannot silently satisfy or break repository tests.
- [x] The public C++ SDK is version-aligned at 0.13.0 and ships a
      machine-readable feature inventory plus host-semantic values for text,
      Option, Result/UnitResult, tuples, queues, sets, spans, buffers, pools,
      Box, any, and nullable values.
- [x] Module ABI descriptor v7 publishes product version, descriptor size,
      capabilities, canonical stable type IDs, and ordered concrete generic
      arguments. Generated shared modules round-trip Unicode `text` through
      typed and dynamic SDK field access.
- [x] The SDK parity matrix distinguishes real bridges, metadata-only values,
      host values, opaque identities, and deliberately deferred surfaces. The
      packaged distribution includes the SDK headers, release manifest, and
      parity documentation.
- [x] The focused SDK/interop regression matrix passed 16/16, package smoke
      passed, and an independently installed SDK header probe compiled and ran
      successfully with the 0.13 package surface.

## v0.12 async, structured concurrency, and platform-I/O sprint

- [x] Wio 0.12 separates continuation, blocking,
      and I/O capacity; checks executor-crossing captures; makes cancellation
      wake suspension boundaries; and provides ordered explicit shutdown.
- [x] `Task<T>` member ergonomics, typed non-blocking polling, recoverable
      deadlines, heterogeneous owning scopes, language `async scope`/`spawn`,
      explicit detach, and homogeneous selection shipped in Wio 0.12.
- [x] `spawn worker` and `spawn blocking` schedule synchronous expressions on
      the appropriate executor while retaining lexical scope ownership; unsafe
      borrowed captures fail during semantic analysis.
- [x] Applications bind and deterministically drain the owner executor;
      `await main` directly transfers the suspended caller, and headless hosts
      can bind/drain the same queue.
- [x] The Wio 0.12 platform-I/O slice adds a dedicated bounded executor and
      ownership-safe, Result-preserving asynchronous filesystem plus process
      run/capture operations, a cancellable portable first-change watcher, and
      leased DNS/connect plus TCP/UDP data I/O and ownership-safe async accept
      with close-race safety.
- [x] `std::process::Spawn` adds owned child lifetime, separate stdin/stdout/
      stderr pipes, sync/async streaming, running state, wait, termination,
      deterministic close/reap, pre-scheduling operation leases, and live-state
      diagnostics.
- [x] Windows entry-point arguments are normalized from the native UTF-16
      command line to UTF-8, including spaced and non-ASCII spawned arguments.
- [x] `std::async::AsyncChannel<T>` provides bounded or unbounded buffering,
      non-blocking `TrySend`/`TryReceive`, cancellable async backpressure,
      close wake-up, and buffered drain-before-completion semantics.
- [x] Wio enum members remain isolated to their declaring enum through the
      C++ backend, allowing separate enums to reuse natural names such as
      `pending`, `empty`, and `closed` without backend collisions.
- [x] Re-importing an already merged module under another alias now restores
      cached export/realm metadata, making nested and combined std imports
      deterministic regardless of module discovery order.

- [x] `async fn`, async object/interface methods, generic async declarations,
      async `Entry`, `await`, and the `coroutine<T>` type are implemented across
      lexer, parser, semantic analysis, and C++20 code generation.
- [x] Suspension-unsafe borrowed parameters/returns, component and extension
      receivers, lifecycle methods, operators, and native/export ABI surfaces
      fail during semantic analysis.
- [x] The shared task runtime provides eager execution, multiple awaiters,
      exception propagation, strong object-receiver lifetime, a worker pool,
      priority-queue timers, cooperative cancellation, and non-blocking timer
      drain during detached-task shutdown.
- [x] `std::async` provides Sleep/Yield, BlockOn/Run, task state and timed wait,
      cancellation/deadlines, All/Any/Race/Timeout, cancellation sources, and
      generic/void structured task groups.
- [x] Async generic object-method mangling now recursively recognizes generic
      parameters nested inside `coroutine<T>`.
- [x] The first-sprint focused matrix covers execution, async Entry, generics,
      object lifetime, structured groups, timeout/cancellation failure paths,
      and semantic rejection cases.
- [x] The second async freeze sprint repaired final-frame ownership, removed
      detached timer shutdown hangs, added configurable workers, recoverable
      timeout, cancellable sleep, owner-thread dispatch, async interface and
      multiple-await qualification, and high-volume native stress coverage.
- [x] Lambda capture is frozen as value-by-default: primitive/component values
      snapshot, object handles retain shared identity, and explicit `ref/view`
      captures remain borrows.
- [x] Windows/Ubuntu freeze gates, packaged documentation checks, native
      ASan/UBSan stress, and libFuzzer/ASan/UBSan corpus validation are part of
      release CI.
- [x] Escaping object-method lambdas retain `self`; scheduler destruction
      drains frame cleanup; backend output replacement tolerates transient
      Windows locks; import-alias and missing-array-extent diagnostics are
      deterministic.
- [x] The final Windows freeze qualification passed the 157-test traceability
      pack, the 78-test focused freeze matrix, package/file-cache/installed-
      package qualification, five consecutive executable replacement probes,
      and the complete 585-test repository matrix.
- [x] The post-0.11 async/multithreading direction is recorded as a dedicated
      simplicity-first evolution plan: a compact task model, structured work,
      explicit blocking and affinity, compiler-checked cross-thread safety,
      real application qualification, and staged I/O/stream expansion.

## P1 language semantics and standard-library foundation sprint

- [x] `std::UnitResult`, `OkUnit`, and `ErrUnit` are the canonical unit-success
      API and public console/filesystem/I/O/tooling surfaces use it.
- [x] Typed postfix `with`, scoped `using`, user attribute declarations,
      target/argument/repetition validation, conflict groups, and runtime
      type/field reflection landed with legacy spelling compatibility.
- [x] Pipeline calls and Option/Result/array destructuring have defined
      evaluation, binding, guard, reachability, and exhaustiveness behavior.
- [x] `|>` and `<|` were evaluated and reserved as ordinary-call pipelines;
      they preserve call inference/diagnostics and single evaluation.
- [x] The sequential stack-resident application/system lifecycle landed with
      deterministic start/update/reverse-close and orderly `self.Exit`.
- [x] The 0.11 semantic stabilization pass froze scoped-attribute boundaries,
      `with` clause diagnostics, pipeline precedence and single evaluation,
      match binding ownership/reachability, and application-root/lifecycle
      invariants. Its 22 newly added runtime and expected-diagnostic tests pass.
- [x] Unicode UTF-8/codepoint/grapheme foundations, text/byte builders,
      hardened JSON, versioned binary frames, CSV/INI, MIME, geometry/color,
      localization, bigint, and compression utilities landed.
- [x] Allocation-conscious `StringBuilder`, endian/varint `ByteWriter` and
      rollback-safe `ByteReader` provide the canonical builder surface.
- [x] Threads, recursive mutexes, condition variables, atomics, channels,
      blocking channels, TaskGroup, Promise/Future, and cancellation
      foundations now share one runtime surface.
- [x] DNS, URI, TCP, and UDP loopback foundations, expanded OS/process
      facilities, structured log sinks, regex safety limits, time formatting,
      secure random, full SemVer precedence, and UUID validation landed.
- [x] The normative 0.11 language delta and standard-library contract are
      published in `docs/spec/` with explicit boundaries for unfinished work.
- [x] Sprint qualification passed all 36 newly added runtime and expected-
      diagnostic tests, including generated C++ and native loopback/threading
      execution; unrelated legacy tests were intentionally not run.

## P1-B nullability, lifetime, and failure safety sprint

- [x] Explicit nullable types now use `T?`; object/interface, opaque,
      function, `ref`, and `view` values are non-null by default. Grouped type
      syntax distinguishes `ref T?` from `(ref T)?`; postfix ordering supports
      nullable elements in dynamic arrays and generic containers.
- [x] Null-flow analysis narrows direct variables through if/else comparisons,
      short-circuit boolean expressions, while conditions, and early-return
      guards. Assignment invalidates the proof, and nullable member use without
      narrowing receives a targeted diagnostic.
- [x] The initialization and lifecycle matrix is executable: non-null local and
      global handles require initializers, components copy/destruct per value,
      objects share identity and destruct at the last strong handle, and
      `OnDestruct` is restricted to a parameterless void hook.
- [x] Panic is the unrecoverable path, `Result<T>` remains the recoverable path,
      panic unwinding runs Wio cleanup, and generated native wrappers translate
      standard/unknown C++ exceptions into stable Wio runtime failures.
- [x] `std::resource` ships `Owned<T>`, `Borrowed<T>`, idempotent `Dispose`,
      `Release`, automatic final-owner close, use-after-dispose protection, and
      live-resource diagnostics, including real non-null and nullable native
      opaque-handle tests.
- [x] SDK module descriptors preserve nullable types through the appended
      `WIO_MODULE_TYPE_DESC_NULLABLE` kind and
      `TypeDescriptorView::is_nullable()`.
- [x] The first versioned normative document is published at
      `docs/spec/WIO_LANGUAGE_SPEC_0_8.md`; completing the full lexical/syntax/
      resolution specification remains active work.

## P1-D const generics and native component sprint

- [x] Functions, aliases, interfaces, components, and objects accept ordinary
      integer const parameters such as `<T, const N: usize = 4>`, including
      trailing defaults and use as read-only values in declaration bodies.
- [x] Const arguments accept non-negative integer literals, earlier const
      parameters, and top-level compile-time integer const declarations.
      Type/value slot mismatches and non-integer const declarations are
      diagnosed before backend generation.
- [x] Static arrays support symbolic extents such as `[T; N]`; substitution,
      constructor matching, literal-size checks, deduction, C++ emission, and
      generic aliases preserve the concrete extent.
- [x] Const values participate in generic identity, invariance, exact/partial
      specialization matching, specificity, ambiguity, and C++ template
      argument generation.
- [x] Declaration-level native components now have a single POD-alias contract
      for both type-only and type/value C++ templates. Native generic component
      specializations inherit the primary ABI mapping and no longer generate
      illegal C++ alias-template specializations.
- [x] Native component extensions bind C++ free functions directly. `view`
      receivers dispatch to `const T&` or `const T*`; `ref` receivers dispatch
      to `T&` or `T*`, with reference overloads taking precedence and no POD
      copy.
- [x] The 0.10 normative contract freezes const-generic and native-component
      rules in `docs/spec/WIO_LANGUAGE_SPEC_0_10.md`.
- [x] P1-D qualification passed the focused const-generic/native-component
      runtime and diagnostic tests, including generated C++ compilation and
      direct native extension receiver dispatch.

## P1-C generics and type-system sprint

- [x] Generic functions, aliases, interfaces, components, and objects accept
      trailing default type parameters, including dependent defaults such as
      `<T = i32, U = T>`. Deduction wins and defaults fill only unresolved
      parameters; invalid ordering, pack defaults, and forward references have
      focused diagnostics.
- [x] Object and component specialization now supports exact and partial
      patterns. Exact matches outrank partial matches, specificity orders
      partial candidates, equal best matches are diagnosed as ambiguous, and
      specialization visibility works across merged modules.
- [x] Readable `where Parameter: Trait` clauses are supported on generic
      functions, aliases, interfaces, components, and objects and share the
      established `@Apply` predicate and user-trait machinery. `+` composes
      multiple predicates conjunctively within one parameter slot.
- [x] Defaulted generic parameters participate in `@Instantiate(...)` for
      native functions, including dependent-default substitution before
      concrete backend instantiation.
- [x] `std::meta` gained `AllSame`, `IndexOf`, and `UniqueCount` as free
      functions and `Types<Ts...>` methods.
- [x] The versioned 0.9 generics contract defines completion order,
      specialization ordering and ambiguity, cross-module visibility,
      constraints, invariant compatibility, and native/export boundaries in
      `docs/spec/WIO_LANGUAGE_SPEC_0_9.md`.
- [x] P1-C qualification passed the 109-test generic/pack matrix, the complete
      508-test repository matrix, and 259 deterministic frontend/backend fuzz
      candidates generated from 250 seeded mutations.

## P0 release-blocking correctness sprint

- [x] Cascading diagnostics are stopped at their source. Imported modules with
      parser errors are no longer merged as partial ASTs, poisoned types flow
      through expressions/calls, and derivative overload/operator diagnostics
      are suppressed while independent root errors remain visible.
- [x] `ref` / `view` lifetime semantics now track static, caller, local, and
      temporary borrow origins. The checked matrix covers temporary component
      calls, nested fields, array elements, returned owning arrays, span range
      tokens, object handles, `self`, `deref self`, mutable receivers, and
      native boundaries. The contract is documented in
      `docs/REFERENCE_LIFETIMES.md`.
- [x] Deterministic lexer/parser/semantic/generated-C++ pipeline fuzzing and an
      optional Clang libFuzzer+ASan+UBSan frontend target landed. Corpus and
      mutations cover nested interpolation, malformed generics, deep types,
      import cycles, invalid UTF-8, arbitrary bytes, diagnostic/output budgets,
      timeouts, dry-run/emission differentials, and backend syntax checking.
      The sprint qualification run passed 259 candidates.
- [x] `FunctionType::toCppString()` now emits nested `std::function` types and
      has a direct type-layer regression test.
- [x] Arithmetic result typing no longer depends on the left operand. Mixed
      signed/unsigned and float/integer operations use a deterministic common
      type, float modulo/bit/shift misuse is rejected semantically, and
      integer-looking float literals generate valid C++.
- [x] `null` is restricted to runtime-nullable targets: object/interface
      handles, functions, `ref`/`view`, `any`, and `opaque`. Primitive,
      component, array, dictionary, and context-free inferred nulls are
      rejected before code generation; reference/null comparison codegen is
      covered.
- [x] `use` parsing now has explicit path-segment recovery, stable diagnostics,
      and conventional bitwise precedence. `use path::* as alias` was also
      repaired in module merge so direct and namespaced access both work.
- [x] Compiler filesystem helpers now follow a documented non-throwing policy
      for ordinary OS failures, close failed seek paths, detect short-read
      errors safely, iterate with `error_code`, and make directory creation
      idempotent. Allocation failure remains explicitly exceptional.

## P1-A language and standard-library correctness sprint

- [x] Integer arithmetic now has explicit semantics across all ten integer
      types. Ordinary `+`, `-`, `*`, unary negation, division, remainder, and
      compound assignment wrap deterministically; division by zero is a Wio
      runtime error. `std::numeric` provides checked and saturating add,
      subtract, and multiply for every integer type.
- [x] Implicit numeric conversion now permits safe widening only. Lossy or
      potentially lossy narrowing requires explicit `fit`, and diagnostics
      identify the source and destination types.
- [x] Enums expose `Value()` and `IsValid()`. `std::reflect` provides generic
      `IsValid`, `TryFromValue`, and strict `FromValue` overloads for every
      integer representation; unknown native values remain representable but
      are rejected by checked conversion and reflection validity checks.
- [x] `Result<T>` gained `Map`, `MapError`, `AndThen`, `OrElse`, `Inspect`,
      `InspectError`, `Flatten`, `ToOption`, `Collect`, and `Sequence`, with
      focused propagation and collection coverage.
- [x] `Option<T>` adoption is complete across intrinsic dictionary lookup and
      std containers. `Get`, `First`, and `Last` represent expected absence;
      strict `At` retains bounds/key failure behavior; iteration helpers,
      `Zip`, Result transpose, cloning, capacity, equality, and removal
      contracts are covered by one cross-container regression suite.
- [x] Canonical `std::fs` operations return structured `Result` values carrying
      the filesystem domain, portable code, native OS code, and actionable
      message. Reads/writes, enumeration, metadata, permissions, copy/move/
      remove, canonicalization, and atomic replacement share the model; legacy
      `Try*`/`*Raw` helpers are explicit compatibility escapes.
- [x] Every public `std::path` and `std::fs` operation is exercised from the
      repository and from a clean installed package. Windows and Linux
      qualification covers contextual `Extension` parsing and rejects source
      checkout leakage.

## Standard platform introspection

- Added typed operating-system and CPU-architecture enums plus public
  `std::platform` queries for pointer width, endian, hardware thread count,
  path-list separator, and native newline.
- Self-hosted environment commands now use platform identity directly instead
  of inferring Windows from executable suffixes.

## Self-hosted global CLI dispatch

- Empty invocation, global help, nested `wio help ...` rewriting, version
  routing, top-level command classification, and typo suggestions now run in
  Wio.
- Raw source/compiler invocations still bypass the companion, while unknown
  tooling-shaped commands enter Wio for consistent diagnostics.
- Native version lookup uses the explicit stage-0 bridge and cannot recurse
  back into the self-hosted companion.
- The generic `--native-cli`/environment-variable bypass was subsequently
  removed. Binding and release packaging now use named private compiler
  services, and the obsolete C++ tooling/env/file/perf CLI sources were
  deleted.

## Self-hosted environment CLI

- Wio owns `env` group/subcommand parsing, help/version behavior, typo
  suggestions, shell validation, root/bin discovery, and command rendering.
- `env print` and non-interactive `setup/remove` previews execute entirely in
  Wio.
- The follow-up completed interactive setup/removal, persistent user
  environment and PATH management, detailed status, duplicate-key diagnostics,
  and backend smoke entirely in Wio.
- Public `std::environment` now exposes user-scoped get/set/remove, user PATH
  membership/add/remove, and duplicate process-key inspection. Windows uses
  the user Environment registry; POSIX uses a managed `.profile` block.

## Self-hosted binding CLI

- Wio owns the `bind new/import` group and subcommand contract, including
  required inputs, optional output/header settings, flagset preference,
  help/version handling, and typo suggestions.
- JSON-manifest generation and namespace-aware C/C++ header importing now run
  in Wio using `std::json`, `std::regex`, `std::fs`, and `std::path`.
- The private binding service, its C++/Argonaut parser and generator, and the
  last shared C++ CLI parsing helper were deleted.
- The final standalone `process_cli.cpp/.h` launcher was removed. Stage-0 now
  launches the Wio companion through the shared host build of the public
  runtime process primitive, so process behavior has a single implementation.

## Self-hosted release-package CLI

- Wio owns every top-level `package` option, default, help/version path, and
  incompatible visual-installer option validation.
- Distribution staging, portable backend discovery/copy, metadata and
  quickstart generation, archive production, and installer orchestration now
  execute in Wio. The obsolete C++ package service was deleted.
- Public `std::process::FindExecutable` supports reusable PATH-based executable
  discovery for packaging and other tooling.

## Core Language Foundations

- [x] Fixed arrays support inferred extents through `[T; _]`, including empty,
      copied, global, field, generic-element, and nested initializers.
      Inference materializes ordinary fixed storage before codegen and rejects
      missing, dynamic, ragged, and non-variable inference contexts.
- [x] The first generic slice landed:
      generic free functions, generic aliases, generic `object` / `component` /
      `interface`, explicit generic calls, and `@Instantiate(...)`.
- [x] The first generic constraint / predicate slice landed:
      `@Apply(...)` plus predicate-based generic native/export bridges.
- [x] The first strong variadic generic / generic-pack slice landed:
      trailing-pack functions, pack forwarding, pack storage, `.size`,
      `.array`, `ToStaticArray<T>()`, and value/type-pack indexing.
- [x] The first two `std::meta` waves landed:
      `Head`, `First`, `Last`, `Types`, `Values`, `ContainsType`, `TypeCount`,
      plus related mutation/helper surface.
- [x] The `v1`-scoped `const generics` / `std::meta` wave 3 slice landed:
      top-level `const` integer declarations now work in pack index positions,
      simple compile-time integer expressions over those constants are accepted
      for pack indexing, and `std::meta` gained `Second`, `Penultimate`,
      `SecondValue`, `PenultimateValue`, and matching `Values<...>` helpers.
- [x] The first loop slice landed:
      `for`, `foreach`, `in`, range iteration, dictionary iteration,
      component-binding iteration, and parenthesized `for (...)`.
- [x] The first namespace/import slice landed:
      `realm`, basic `use`, `use ... as ...`, and multi-file module resolution.
- [x] Import ergonomics improved:
      alias-hide semantics, direct import, `use path::*`, and
      `use path::* as alias`.
- [x] The type-alias surface landed:
      `type Name = ExistingType;`, generic aliases, and alias constructor calls.
- [x] The first strong constructor-deduction slice landed:
      constructor-based type deduction for generic `object` and `component`.
- [x] The `else if` parser/codegen path was fixed.
- [x] `const` inside objects and outer-`const` visibility inside methods were fixed.

## Runtime Dynamic Types

- [x] `std::Result<T>` settled as the canonical model.
- [x] `Foo!()` unwrap sugar landed.
- [x] `Foo?()` propagation sugar landed.
- [x] Explicit generic `Foo<T>!()` / `Foo<T>?()` support landed.
- [x] `any` landed at both source and runtime levels.
- [x] The first strong `std::Box<T>` / `std::heap::box<T>` slice landed.
- [x] The `opaque` foreign-payload model landed.
- [x] A serious runtime/test line was established for `any / Box / opaque`.
- [x] `std::event` was added as the first real usage surface for
      `any/context/payload`.
- [x] The public `Result`, `std::Box<T>`, `any`, and `opaque` boundaries were
      frozen across the language/runtime/std documentation set.

## Mutable Data and Semantics

- [x] `ref values[index]` mutable indexed-reference semantics landed.
- [x] Nested mutable access and `ref` ergonomics improved significantly.
- [x] The first real in-place mutation coverage landed for arrays, dicts, and
      components.
- [x] Explicitly typed `let` / `mut` declarations may omit assignment and now
      receive deterministic value initialization across scalars, strings,
      containers, and components. Untyped declarations and `const` still
      require initializers.
- [x] The mutable reference/value-context behavior is now documented as part of
      the intended `v1` language contract.

## Enum / Flagset

- [x] `const` can now be used with enums and flagsets.
- [x] Native enum and native flagset support landed.
- [x] The first enum/flagset reflection slice landed.
- [x] The enum/flagset helper surface expanded with
      `Count`, `Name`, `Has`, `HasAny`, `With`, `Without`, `Toggle`, and `Clear`.
- [x] The normal Wio enum/flagset reflection surface was closed with
      `Value`, `Index`, `UnderlyingType`, and `Size`.

## Native Interop and ABI

- [x] `@Native`, `@CppHeader`, and `@CppName` settled into a real bridge.
- [x] The native `string -> const char*` bridge landed.
- [x] The first serious native `ref / view` passing semantics landed.
- [x] The declaration-level native POD component bridge landed.
- [x] Generic native POD component support landed.
- [x] Native POD field support expanded through static arrays.
- [x] Native `std::Box<T>` signature usage was enabled.
- [x] The top-level function export path landed with `@Export`.
- [x] The first strong public-member surface landed for exported
      `object` / `component`.
- [x] The module lifecycle/state ABI landed in its first strong form.
- [x] Shared/static/executable target separation and host interop were tested.
- [x] The intended `v1` native interop contract is now documented explicitly
      across the language freeze docs.

## Standard Library Surface

- [x] The first source-based std slice landed:
      `std::console`, `std::math`, `std::collections`, `std::strings`,
      `std::fs`, `std::path`, `std::algorithms`, `std::assert`, and
      `std::testing`.
- [x] `std::console` and `std::io` were rebuilt and moved onto the `Result`
      model.
- [x] `std::process` was added.
- [x] The `std::reflect` enum/flagset surface was strengthened.
- [x] `std::console` / `std::io` were aligned with the runtime header surface.

## SDK / Host Integration

- [x] The public host SDK surface gained its own identity.
- [x] The higher-level `WioObject` / `WioComponent` direction was opened.
- [x] SDK-side enum/flagset identity landed through `WioEnum` and
      `WioFlagset`, including exported field metadata and dynamic access.
- [x] Host-side exported types, interop, lifecycle, hot-reload, and
      stale-wrapper tests landed as the first serious wave.
- [x] SDK examples and packaged host-interop flows received real repo coverage.

## Tooling, Project Model, and Packaging

- [x] `wio.makewio` clearly became the primary project manifest direction.
- [x] `wio build` and `wio test` landed.
- [x] `wio file run/check/tokens/ast` landed.
- [x] `wio project new/describe/build/run` landed.
- [x] `wio bind import/new` landed.
- [x] `wio env print/setup` landed.
- [x] `wio package` landed.
- [x] The CLI command surface was frozen as the primary tooling contract.
- [x] The source-based tooling split was formalized:
      core orchestration stays in the CLI while `scripts/wio/*.wio` hosts
      source-based workflow helpers.
- [x] Packages now include installer wrappers (`Install-Wio.ps1`,
      `install-wio.sh`) and source-based tool scripts.
- [x] Packaging and install UX now share one documented model through
      `wio package`, `wio env print/setup`, and packaged `QUICKSTART.md`.
- [x] PowerShell stopped being the primary path; larger scripts became
      compatibility wrappers.
- [x] The first `.wio` tooling folder and source tool examples landed.
- [x] The `wio` CLI was split into separate modules and `main.cpp` became a
      thin entrypoint.
- [x] `wio file run` and source-based tool workflows now keep backend outputs
      out of source directories, use project/repo hidden caches when available,
      and fall back to user-cache locations for packaged/non-project use.
- [x] Ordinary generated `.wio.cpp` files are now treated as backend
      intermediates near the output root, while `--emit-cpp` remains the
      explicit opt-in path for keeping source-adjacent generated C++.

## Binding Automation

- [x] Manifest-based binding scaffolding landed.
- [x] A header -> Wio binding bootstrap importer landed.
- [x] Binding smoke tests were moved onto the CLI path.

## Testing and Productization

- [x] The first serious positive feature-test waves landed.
- [x] The first serious invalid-program corpus landed.
- [x] The first clear backend compile/link failure tests landed.
- [x] Native/shared/static/SDK/package/project-model smoke tests landed.
- [x] The release-facing tooling smoke matrix now covers CLI help, file flows,
      project create/describe/build/run, binding import/new, package staging,
      packaged file-run cache behavior, and source-based Wio tool dry-runs.
- [x] Packaged toolchains were validated through real project
      create/build/run flows.
- [x] Installed-package qualification gained a real installer-based
      Windows/Linux release gate: it uses a clean toolchain root, validates all
      40 public std modules independently and together, exercises a complete
      external project build/run/test/package lifecycle, validates native
      interop, and rejects source-checkout resolution leakage. The first-class
      `wio project test` command supports discovery, manifest overrides,
      filtering, listing, incremental builds, and `--no-build`; `wio project
      package` emits `bin`/`lib`/assets/additional files plus machine-readable
      package metadata.
- [x] The release-facing example set now includes plain app, native app,
      hybrid module, binding import, packaged quickstart, static consumer,
      and the heavier hybrid arena companion example.
- [x] A Windows/Linux release validation workflow now covers the critical
      packaged/project/binding/process smoke matrix in CI.
- [x] The language/reference freeze snapshot now treats the chosen generic,
      Result, native interop, mutable reference, and runtime reference surface
      as the intended `v1` contract.
- [x] The `v1` release contract now has explicit companion notes for
      compatibility policy and performance/memory expectations.
- [x] The docs set was expanded into a release-quality, website-ready
      structure: getting started, CLI reference, interop guide, examples
      guide, troubleshooting, FAQ, refreshed SDK/std/project-system
      navigation, and a clearer root/docs index story now all point at one
      coherent `v1` product narrative.

## Recent Partial Foundation

- [x] The self-hosted CLI bootstrap landed: the native stage-0 compiler builds
      and packages a Wio + Argonaut-Wio companion, provides a recursion-safe
      internal fallback bridge, and routes `project test/package` through Wio
      argument validation with lifecycle and installed-package probes.
- [x] `wio project new` became the first command with both parsing and business
      logic fully implemented in Wio; its plain, native, module, and hybrid
      templates remain compatible with the existing project build/run tests.
- [x] The complete project family moved behind the self-hosted boundary:
      makewio discovery and normalization, `describe`, Wio/native-host
      `build`, `run`, regex-driven `test`, directory `package`, and the
      `wio run` shorthand execute in Wio without calling the legacy native
      project handlers.
- [x] The second self-hosted CLI wave landed: `file run/check/tokens/ast`,
      repository `build/test`, `dev build/test`, and the five-scenario
      `perf smoke` implementation now execute in Wio with parity, packaged
      cache, and installed-layout tests.
- [x] `std::environment` now exposes portable process environment,
      home/cache/temp directory helpers, and `std::statistics` provides
      reusable sum/mean/median/variance/summary operations.
- [x] The first member-operator overloading slice landed.
- [x] Operator overloading was fully closed:
      member/free unary-binary-assignment operators, `fit`, `[]`, `()`,
      generic overloads, and the explicit `deref` ergonomics slice now work
      together.
- [x] Generic validation hardening landed:
      generic bodies keep symbolic validation for codegen, gain concrete
      call-site validation, and now understand numeric/integer constraint
      predicates like `std::traits::IsNumeric<T>` during semantic checks.
      Unary/binary member operator resolution and codegen are working.
      Final hardening continues to be tracked in `TODOLIST.md`.

## Version 1 Contract and Product Closure Milestones

The following completed checklist used to remain in `TODOLIST.md`. It is kept
here as historical evidence rather than active work:

- [x] Operator overloading was completed across member/free,
      unary/binary/assignment, `fit`, `[]`, `()`, generic overload, and
      explicit `deref` paths.
- [x] The intended language surface was frozen across the freeze snapshot,
      language draft, std docs, runtime type model, SDK docs, examples, and
      compatibility policy.
- [x] `wio.makewio` became the official primary project format, with
      `wio.project.json` retained only for compatibility.
- [x] The primary CLI behavior was frozen around `build`, `test`, `file`,
      `project`, `bind`, `env`, and `package`.
- [x] PowerShell helpers were removed from the primary workflow and retained
      only as compatibility wrappers.
- [x] The split between native CLI orchestration and Wio-written tools under
      `scripts/wio` was formalized.
- [x] Single-file/tool output, generated-C++ cleanup, package cache, and
      non-writable-install policies were settled.
- [x] Packaging/install UX was unified around `wio package`, installer
      wrappers, `env print/setup`, `QUICKSTART.md`, and executable-relative
      discovery.
- [x] Windows/Linux release validation was established for the critical
      project, package, binding, process, and packaged-file flows.
- [x] The stable/experimental std boundary was documented.
- [x] `std::Result<T>`, `Foo!()`, and `Foo?()` were sealed as the official
      recoverable error-flow model.
- [x] The `@Native`, `@CppHeader`, `@CppName`, POD/native enum/flagset,
      export, and ABI-safe interop contract was frozen.
- [x] `any`, `Box`, and `opaque` boundaries were made official.
- [x] Mutable indexed/container access and `ref` value-context ergonomics were
      finalized for the chosen contract.
- [x] Enum/flagset support was closed across constants, native interop,
      reflection, helpers, normal Wio APIs, and SDK identity.
- [x] Release-facing tooling tests, documentation, examples, compatibility
      policy, and performance/memory notes were completed.
- [x] The scoped const-generic/std-meta wave 3 slice was completed.

## Standard Library Expansion Through v0.4

- [x] `std::hash` landed with FNV-1a as the default and SHA-256 digest/hex
      support.
- [x] `std::random` landed with MT19937 as the default plus xoroshiro128+,
      LXM, and Wichmann-Hill generators.
- [x] `byte`/`bit`, `ByteBuffer`, generation-checked `BytePool`, and typed
      `Pool<T>` landed.
- [x] Queue, ordered/unordered sets, heterogeneous tuple, regex, time, span,
      range, and adaptive sorting modules landed.
- [x] Type traits expanded across built-in categories and user-defined nominal
      predicates usable by `@Apply`.
- [x] Reflection expanded beyond enum/flagset to component, object, interface,
      field, method, access, base, size, and alignment metadata.
- [x] Checked numeric/string conversion and parsing landed through
      `std::convert`, including base-aware integer formatting/parsing and
      generic `ToString`.
- [x] `std::chars`, expanded string helpers, and the larger generic algorithms
      wave landed.
- [x] `std::Option<T>` landed with presence queries, `Value`, `ValueOr`,
      `Map`, `AndThen`, `Filter`, `OrElse`, and `ToResult`.
- [x] Option-returning lookup helpers landed for array algorithms,
      collections, strings, spans, and iterator search.
- [x] `std::iterator`, `std::range`, and `std::encoding` landed.
- [x] `std::serialization` and the recursive `std::json::Value` system landed
      with parsing, compact/pretty writing, nested arrays/objects, typed
      access, source-positioned errors, and round-trip coverage.
- [x] Checked/saturating `std::numeric`, in-memory `std::stream`, UUID v4,
      packed component/extension SemVer, and structured `std::log` landed.

## Component Extensions and Interpolation

- [x] Stack-preserving component extensions landed as externally lowered
      methods with member-call ergonomics.
- [x] Mutable and immutable extension receivers, access isolation, semantic
      resolution, code generation, documentation, and positive/negative tests
      landed.
- [x] Component extensions were applied to `Span` and vector components.
- [x] Immutable component extension calls on temporary values were fixed in
      the C++ backend by safely materializing the temporary through the complete
      call expression.
- [x] Interpolated-string lexing moved from global flags to balanced nested
      frames.
- [x] Interpolation expressions now support ordinary string literals, nested
      function calls/parentheses/braces, dictionary calls with string
      arguments, and nested interpolated strings.
- [x] Contextual keyword identifiers were hardened across declarations,
      component/object fields, member and realm qualification, imported APIs,
      and native bindings. `std::path::Extension` no longer makes packaged
      `std::path` / `std::fs` imports fail, and a focused end-to-end regression
      test covers the complete path.
- [x] The right-associative `condition ? whenTrue : whenFalse` conditional
      expression landed with lazy branch evaluation, boolean-condition and
      branch-compatibility diagnostics, direct C++ lowering, precedence
      documentation, and focused positive/negative coverage.

## CLI Expansion and Releases

- [x] Bare project commands gained manifest discovery from the current
      directory and its ancestors.
- [x] `wio run` became a project-run shorthand.
- [x] Contextual `wio help`, command-family version/help, application
      arguments after `--`, repeated arguments, working-directory control,
      command printing, and no-build/no-manifest-args controls landed.
- [x] Direct cross-platform process launching replaced shell command strings
      for project, package, and performance subprocesses.
- [x] Argument forwarding now preserves spaces, flag-like values, shell
      characters, literal separators, child exit codes, and Windows PATH
      behavior.
- [x] Wio `v0.2.0`, `v0.3.0`, and `v0.4.0` release branches/packages were
      produced; `v0.4.0` also received a Git tag and GitHub Release with
      installer and portable assets.

## Real-World External Validation

- [x] A large multi-module Wio colony simulation was built outside the repo
      with deterministic generation, component extensions, missions, economy,
      interactive CLI, native persistence, and JSON output.
- [x] A non-game native desktop productivity application was built with Wio
      and raylib, including a responsive task board, quick notes, search,
      focus timer, mouse/keyboard interaction, and JSON persistence.
- [x] The raylib desktop application compiled through the installed Wio
      `v0.4.0` toolchain and completed a real GLFW/OpenGL render smoke test on
      Windows.
- [x] That external validation exposed the installed-package
      `std::path::Extension` contextual-keyword regression; its fix and the
      release gate preventing recurrence remain active P0 items in
      `TODOLIST.md`.

## v0.11 Desktop and Editor Validation

- [x] Atlas Desk landed as a substantial native desktop workspace dashboard
      using the `application` lifecycle, stack-resident systems, async
      coroutine scanning and cancellation, typed attributes, modern native
      declarations, extensions, Option/Result, JSON, and Unicode APIs.
- [x] Atlas Desk was built and smoke-rendered with the published Wio `0.11.0`
      portable toolchain, with its rendered dashboard retained as release
      evidence.
- [x] `wio-vscode` was rebuilt for `0.11.0` around modular CLI, diagnostics,
      source-index, and provider layers with modern grammar and snippets.
- [x] Compiler and extension releases now share their major/minor release line;
      the matching extension check, unit-test, Windows/Ubuntu CI, and VSIX
      package gates are part of the release policy.

## v0.14 Standard Library and SDK Value Parity

- [x] Stable Option, Result/unit, tuple, nested collection, queue, set, checked
      span-range, and owned ByteBuffer values gained C++ SDK round-trips.
- [x] ABI descriptor v8 gained exact const-generic metadata and analysis-time
      rejection for unsupported or unspecialized export shapes.
- [x] The SDK feature inventory gained explicit Supported, Partial, and
      Deferred states and a normative 0.14 parity matrix.
- [x] Unicode normalization, exact JSON numbers, typed serialization codecs,
      bounded regex records, deterministic vectors, and container invariants
      landed with focused conformance tests.

## v0.15 Typed and Behavioral Attributes

- [x] Declaration-leading `[Attribute]` applications, composition, retention,
      target/require/conflict/cardinality policies, and deterministic ordering
      became the canonical typed model for built-ins and user attributes.
- [x] Compile-time validators, checked method derives, typed receiver guards,
      typed post/finally/around hooks, coroutine pre/post/finally execution, and
      explicit rejection of unsupported async around behavior were frozen.
- [x] Runtime reflection exposes normalized attribute applications and method
      pipelines; module ABI v9 exposes retained type/field/method/export
      descriptors to C++ hosts.
- [x] `wio migrate attributes --check|--write` converts legacy `@` syntax while
      preserving strings and comments, and compiler lowering resolves built-ins
      through their canonical typed identities.

## v0.16 Ownership, Async Hosting, and Applications

- [x] ABI descriptor v10 gained host-owned, main-thread-affine application
      state with non-blocking update, explicit main pumping, contained errors,
      stale-generation rejection, and retained module lifetime.
- [x] Stable scalar exported async functions gained typed C++ task handles with
      poll, explicit wait, deadlines, cancellation, completion callbacks,
      main-executor delivery, and safe shutdown.
- [x] Cancellation now propagates into directly awaited children;
      `TryWithCancellation` and filesystem/process/network token overloads
      preserve expected cancellation as ordinary Option/Result outcomes.
- [x] `WioHostCallback` gained typed scalar signatures, retainable userdata,
      thread declaration, and exception containment across the native boundary.
- [x] `WioOwnedNativeResource`, `WioBorrowedNativeResource`, and the move-only
      SDK `UniqueNativeResource` wrapper froze transferred versus borrowed
      native ownership with exactly-once cleanup.
- [x] Applications gained real monotonic frame deltas, deterministic explicit
      and fixed schedules, explicit `ref`/`view` resource injection, hosted
      headless execution, and reverse partial-start rollback.
- [x] Console, resource/tool, desktop event-loop, fixed game-loop, service I/O,
      callback, native-resource, and native-host scenarios entered the Windows
      and Ubuntu release acceptance matrix.

## v0.17 Attribute-driven Application Surface

- [x] Application and system bodies accept ordinary default-mutable fields and
      ordinary helper functions while retaining stack-resident lowering.
- [x] Conventional `Start`/`Update`/`Close` and descriptive
      `[Start]`/`[Update]`/`[Close]` functions normalize to one lifecycle ABI.
- [x] `[Fixed]`, `[After]`, and `[Main]` produce a deterministic stage graph;
      source-module system fields enter that graph automatically.
- [x] `[Worker]` is reserved with an explicit safety diagnostic until
      cross-thread `ref`/`view` conflict analysis is available.
- [x] Legacy v0.16 application syntax remains accepted and has a documented
      migration map instead of a flag-day removal.
- [x] SDK ABI v11 and `ApplicationHost::stages()` expose the normalized stage
      graph, dependencies, fixed frequencies, order, affinity, and stage kind
      flags to native hosts.

## Backend-neutral WIR Foundation

- [x] Typed WIR and canonical Lowered WIR gained stable type/function/block/
      value identities, source spans, deterministic printers, and independent
      structural verifiers.
- [x] Structured control flow, SSA block arguments, conditionals, loops,
      short-circuiting, match projections, arrays, numeric conversion, and
      literal typing lower without backend-specific recovery guesses.
- [x] The place and memory model now makes locals, initialization, loads,
      stores, fields, indices, borrows, construction, and reverse lexical
      cleanup explicit for component values and owning object handles.
- [x] Object/interface methods are receiver-aware WIR functions; nominal types
      retain deterministic override slots and abstract entries, while direct,
      virtual, and interface dispatch remain distinct verified operations.
- [x] Safe object/interface upcasts, checked `fit`, runtime `is`, object
      identity equality, and `self`/`deref self`/`ref`/`view` return semantics
      survive unchanged into Lowered WIR.
- [x] The Callable Model freezes overload results and generic specialization
      identities in WIR; named function values, closures with ordered
      value/reference/retained-self environments, indirect calls, and
      extension implementation calls are distinct verified operations that
      survive canonical lowering unchanged.
- [x] The Value and Container Model preserves dictionary construction and
      keyed read/write places, array/string/text indexing, structured string
      and Unicode interpolation, enum/flagset constants and intrinsics, `any`
      boxing/testing/casting, nullable wrapping, and nominal
      Option/Result/Tuple/Span identities through verified canonical lowering.
- [x] Direct `dictionary[key]` reads and writes are accepted by semantic
      analysis and use the same checked native intrinsic as existing direct
      array/string/text indexing.
- [x] The Ownership and Cleanup Model classifies trivial, owned-value,
      intrusive-reference-counted, borrowed, and generic types; records borrow
      lifetimes; makes managed copy/move/replace/release/drop explicit; lowers
      object copies and drops to retain/release while keeping component/value
      glue distinct; and verifies exactly-once local cleanup across CFG exits.
- [x] The Native Interop and ABI Model gives native declarations a canonical
      C/C++ symbol, header, stable identity, failure boundary, receiver mode,
      parameter/result ownership and marshalling contract. Native POD,
      `opaque`, ref/view, callbacks, and generic template specializations now
      lower through explicit `native-call`/`native-invoke` operations.
- [x] Deterministic native thunk planning and the C-shaped
      `wio_native_abi.h` SDK contract define concrete specialization adapters,
      owner-provided intrusive handle operations, callback lifetimes/thread
      policy, contained failures, and exactly-once foreign-resource release.
- [x] Realm-qualified calls no longer probe namespace identifiers as method
      receivers, and contextual `ref` arguments use the selected ABI parameter
      instead of leaking semantic `<unknown>` placeholders into WIR.
- [x] The Module, Export, and SDK Model gives each module a
      checkout-independent identity and retains explicit Wio/standard/native
      dependencies, stable exports, concrete generic SDK signatures,
      reflection descriptors, lifecycle/state-transfer hooks, and deterministic
      call-table slots through Typed and Lowered WIR.
- [x] Module verifiers reject stable-ID drift, duplicate exports, call-table
      mismatch, invalid targets, malformed reflection, and unpaired hot-reload
      hooks. The versioned C-shaped `wio_module_contract.h` sidecar adds
      stable-ID lookup without breaking the existing `WioModuleApi` v11 ABI.
- [x] The Async, Coroutine, and Thread WIR Model distinguishes task await from
      executor handoff, records known scheduler operations and affinity, and
      lowers every suspension into a cancellation check plus canonical
      suspend/resume state. Async returns become coroutine completion, while
      conservative frame slots retain explicit ownership and cleanup metadata
      for both the future C++ backend and VM.
- [x] Typed and Lowered async verifiers reject payload drift, coroutine layout
      misuse, missing cancellation checks, malformed state/resume edges, and
      ordinary returns inside lowered async functions. Deterministic printers
      expose state ids, affinity, frame size, and thread-switch behavior.
- [x] The Application and System WIR Model freezes stack-resident system types,
      application entry/lifecycle identities, deterministic stage order and
      dependencies, fixed/main affinity, resolved run callables, and typed
      read/write resource injection in the backend-neutral module contract.
- [x] Effective attributes now retain canonical target/origin/argument/
      retention data and ordered processor phases in WIR. Reflection records
      expose fields, methods, visibility, mutability, dispatch slots, async
      state, and shared attribute identities; Typed and Lowered verifiers and
      printers cover the complete contract.
- [x] The Language Surface Model gives globals stable declarations and
      initializer functions, and lowers their reads and writes through explicit
      global places. Range, array, and dictionary `for-in` share canonical
      iterator operations with index/key bindings and structured loop cleanup.
- [x] Result unwrap/propagation, duration literals, range containment, generic
      constant/pack types, per-operand pack expansion, and resolved overloaded
      operators now retain backend-neutral operations and identities through
      verified Typed-to-Lowered WIR.
- [x] Canonical Lowered WIR optimization performs overflow-safe constant
      folding, constant-branch and forwarding-block simplification, unreachable
      block removal, trivial block-argument propagation, and conservative pure
      dead-value elimination without compacting stable IDs or source spans.
- [x] Escape analysis records local/call/store/return/coroutine behavior and
      stack/heap/coroutine-frame storage decisions while preserving intrusive
      ownership. Fixed and freshly constructed arrays expose verified required,
      statically eliminated, or proven-eliminated bounds-check contracts.
- [x] The first independent Lowered WIR C++ backend is executable behind
      `--cpp-backend wir`. It emits stable WIR-derived identities, canonical CFG
      state machines, explicit place storage/projections, component/object
      layouts, intrusive object handles, native symbol calls, source maps, and
      executable entry adapters without reading AST nodes or silently falling
      back to the legacy generator.
- [x] New C++ backend failures use stable source-located `WCPPxxxx` diagnostics.
      A host-compiler syntax gate and an end-to-end CLI gate compile and execute
      the mutable-local control-flow slice; legacy AST generation remains the
      default compatibility oracle during differential parity work.
- [x] Enum and flagset types now carry exact underlying-type/case-bit metadata
      from semantic analysis through both WIR levels. The independent C++
      backend emits strong declarations, constants, bit operations, name/
      validity reflection, and the complete enum/flagset intrinsic family
      without re-reading AST expressions.
- [x] The independent backend emits concrete `Option<T>`/`Result<T>` layouts,
      construction, pattern tests/payloads, checked unwrap, and early error
      propagation. Open generic templates remain canonical metadata and are not
      emitted as invalid C++; unresolved generic calls fail with a stable
      backend diagnostic.
- [x] Cleanup-free value components loaded from places retain owned-value
      identity, and Lowered WIR heap-storage validation no longer mistakes an
      ordinary call/load returning a reference-counted value for an allocation.
- [x] The independent C++ backend maps pinned array/dictionary/string/text
      intrinsics to shared runtime helpers, including void mutators, numeric
      parsing, Unicode operations, and canonical Option lookup construction.
- [x] Range and array/dictionary iterators execute from Lowered WIR with
      index/key/value projections, reference mutation, steps, break/continue,
      empty-container handling, and overflow-safe numeric advancement. Invalid
      iterator projections are rejected before C++ emission with `WCPP1210`.
- [x] Borrowed loads preserve container identity; array place emission preserves
      required bounds checks. Contextual integer literals match numeric operand
      types, including unsuffixed comparisons against `Count()` results.
      Focused generated-code tests compile, link, and execute the new surface,
      including ordered dictionaries, emoji text, lookup misses, invalid steps,
      and signed/unsigned range limits.
- [x] Sprint 17.1 adds backend-neutral non-variadic generic function
      materialization before canonical lowering. Concrete bodies retain their
      generic origin, normalized specialization key, and arguments. Worklist
      caching supports nested/self/mutual recursion, deduplication, deterministic
      output, and idempotent reruns; invalid bindings and expansion limits have
      stable diagnostics.
- [x] Generic substitution carries through captured closures, ref parameters,
      extensions, nested component types, and const fixed-array extents. Explicit
      generic-constant/default-value operations preserve the template contract;
      concrete ownership removes trivial cleanup and preserves managed claims.
- [x] The dedicated `wio_wir_cpp_generics` gate compiles, links, and runs the new
      surface, with structural identity/failure checks and a pinned function
      reference probe. Regression fixes cover symbolic array bounds, equivalent
      explicit/inferred const extents, generic capture declaration identity,
      borrowed extension receivers, place moves, closure environment parameters,
      and object inequality/field access in the independent C++ backend.
- [x] Sprint 17.2 adds backend-neutral hierarchy/cast/dispatch tables and
      declaring-subobject field projections, verified before C++ emission.
      Object inheritance, interface dispatch, multi-level overrides, direct
      super calls, checked fit/is, normalized identity and any hierarchy casts
      execute without AST fallback.
- [x] Constructor overload identities now survive semantic analysis and generic
      validation snapshots into WIR. Actual constructor bodies, base default
      initialization, generic object methods/lifecycle bodies and derived-to-base
      destruction run through the new backend. Object borrows remain non-owning;
      owning self returns and Ref/WeakRef retain the existing intrusive protocol.
- [x] The new `wio_wir_cpp_objects` generated-code gate covers source execution,
      component/object constructor bodies, generic/interface combinations,
      inherited/multiple fields, self ref/view/value returns, scope/boxing/call
      cleanup, failed casts, strong/weak lifetime and malformed dispatch/field
      metadata. Only this sprint's new gate was run for the implementation.
- [x] Sprint 17.3 executes canonical coroutine suspend/resume/complete through
      C++20 coroutines and the shared task runtime, without blocking ordinary
      awaits or falling back to AST code generation. The async entry adapter
      pumps the bound main executor. Worker/blocking/IO/main transitions and
      current-promise cancellation checkpoints preserve explicit WIR states.
- [x] Async object/interface receiver retention is now a verified frame contract;
      generated RAII guards keep self alive across suspension. The new
      `wio_wir_cpp_async` gate covers generic/void/ready tasks, closures and worker
      jobs, interface dispatch, owning results, cancellation/fault cleanup,
      executor identity, cancelled queued main continuations, shutdown failure,
      and invalid frame/state/receiver metadata. Legacy remains the default;
      generalized native/SDK async adapters are Sprint 17.4 work.
- [x] Sprint 17.4 emits concrete native generic/POD/free-extension adapters from
      Lowered WIR, including template argument identity, const/ref overload
      adaptation, native layout checks and checked exception/callback boundaries.
- [x] The independent `WirModuleEmitter` emits canonical SDK wire-v2 call tables,
      native registries, lifecycle/state hooks, stable export/type identities and
      the existing v11 scalar/void compatibility subset. The opt-in native SDK
      owns strings, Unicode text, POD and intrusive handles through producer ops;
      synchronous ref tokens preserve aliasing and copy back mutations on failure.
- [x] Canonical task ready/read/cancel/main-pump and final scheduler shutdown now
      work across a real DLL/independent C++ host. Returned values pin their DLL.
      Only the new `wio_wir_cpp_native` and `wio_wir_cpp_sdk` gates were run;
      Windows/MinGW execution is verified, not full cross-platform cutover.
      Rich application/reflection integration remains 17.5 and differential
      project/std/platform parity remains 17.6. Legacy stays the default.
- [x] Sprint 17.5 gives every application contract a distinct WIR construction
      function. Declared and compiler-generated field initializers execute in
      declaration order before `Start`; standalone and shared-library hosts use
      the same main-thread-affine, nonblocking runtime with partial-start
      rollback, invalid-delta checks, deterministic close, and module leases.
- [x] The additive `WioGetNativeReflectionApi` sidecar exposes stable type,
      constructor, field, method, enum/flagset case, effective attribute,
      normalized argument, origin, retention, and processor metadata. The
      native SDK can enumerate types/attributes, construct exported values,
      access public fields, and invoke public synchronous methods through
      checked wire-v2 thunks while keeping private/async members non-callable.
- [x] Lowered-WIR emission now generates static `TypeReflection<T>` tables for
      native generic consumers and control-safe quoted metadata. The focused
      `wio_wir_cpp_application` and `wio_wir_cpp_reflection` shared-library host
      gates cover lifecycle ordering/failure/thread rules, initializer values,
      reflection access control, runtime attribute retention, stable identities,
      and values/hosts that outlive the module view. Worker stage transfer
      remains deliberately reserved by the language contract.
- [x] Sprint 17.6 adds executable legacy/WIR differential gates for standalone
      sources and clean `wio-app`/`wio-native-app` projects. The gates compare
      exit status, stdout, and stderr, record isolated build durations, and run
      with every focused Lowered-WIR C++ backend gate on Windows and Ubuntu.
- [x] Variadic packs, concrete generic-owner dispatch, constructor identity,
      zero-argument component construction, deduced-vs-explicit native template
      calls, UTF-8 native arguments, and `Entry(string[])` now survive through
      the independent backend without AST fallback.
- [x] Behavioral pre/post/finally/around attributes execute from canonical WIR
      processor `TypeId` and hook `FunctionId` identities. Source/reverse hook
      ordering, typed receiver/result modes, async results, reflection ordering,
      exactly-once finalization, and guarded `proceed` lifetime/call count are
      covered by executable legacy/WIR differential cases.
- [x] Sprint 17 closes with Lowered WIR as the default C++ generation path for
      file and project commands. `--cpp-backend legacy` remains available for
      one release line as an explicit rollback and differential oracle; there is
      no automatic AST fallback.
- [x] The final Sprint 17 parity pass covers async task intrinsics, task polling
      and bounded waits, retained lambda/self captures, expression-bodied unit
      lambdas, match destructuring places, fixed-array projection, runtime field
      attribute reflection, concrete native-generic thunk identities, and
      explicit retained/any-thread native callback contracts.
- [x] Sprint 19 VM runtime closure adds debugger-controlled pause/resume/abort at
      stable instruction boundaries. A controller can wait with a timeout,
      inspect the exact function/block/instruction/source/executor event, and
      resume or abort from another thread without losing frames, registers,
      cleanup state, or the existing source-aware diagnostic contract.
- [x] Sprint 20.1 connects VM `NativeInvoke` to the shared wire-v2 native
      registry by stable key. Scalar, embedded-zero UTF-8 string, Unicode text,
      nullable and ref/view values cross checked thunks; mutable aliases copy
      back even on native failure, and malformed ABI versions, registrations,
      arguments, failures, and results produce stable `WVM122x` diagnostics.
- [x] Sprint 20.2 gives the VM checked native POD, opaque, foreign-object, and
      runtime-value storage. Stable type identities are verified at the
      bytecode and runtime boundaries, producer handle operations preserve
      intrusive ownership, and the final VM copy performs exactly one foreign
      release.
- [x] Sprint 20.3 admits call-scoped and retained scalar callbacks with pinned
      lifetime/thread contracts. Caller-thread violations and stale machines
      are rejected before VM entry; any-thread callbacks serialize execution,
      preserve captures, and contain Wio/native failures at the ABI boundary.
- [x] Sprint 20.4 adapts canonical native `coroutine<T>` handles into ordinary
      VM tasks through an additive completion sidecar. Foreign-thread
      completion resumes awaits on their recorded executor, faults and
      cancellation retain stable VM behavior, main queues are pumped together,
      teardown cancels pending producers, and settlement/releases are
      exactly-once under completion/cancellation races.
- [x] Sprint 20.5 adds canonical-path native DLL/shared-library discovery,
      registry and async-sidecar validation, shared library leases, and producer
      code pinning for escaped foreign values and task adapters. The self-hosted
      project CLI builds paired bytecode/native-bridge outputs from one Lowered
      WIR and runs them with manifest and application arguments through
      `--backend vm`; both artifacts and their source/header dependencies
      participate in incremental reuse.

## Sprint 20.6 Native Differential Qualification

- [x] A real generated-thunk C++/VM differential gate checks independent success
      oracles, exact successful outputs, native failure payload/effects, mixed
      ref/view aliases, POD/generic round-trips, enum/flagset scalars, opaque and
      foreign-any ownership, string/text callbacks, retained worker entry, and
      native async scalar/runtime-value results.
- [x] Wire-v2 owned callback slices survive temporary VM frames; malformed scalar
      widths, Unicode slices, nullable references, and mutable-reference type
      mismatches have focused boundary regressions.
- [x] Fixed non-nominal native ABI identity emission, nullable opaque conversion
      and comparison, retained worker callback locking, ref/view alias matching,
      enum/flagset wire encoding, and embedded-zero C++ string/text literals.
- [x] Both new tests are labeled `native-differential;release-gate` and selected
      by the Windows/Linux CI matrix. Local Windows qualification passes; Linux
      remains CI verification. Known local-POD/local-any/structured-callback
      parity boundaries are executable negative cases and remain in the backlog.

## Build and Test Registration Maintenance

- [x] The app CMake file is limited to application/self-host/playground targets
      and guarded test-module includes. Language, invalid, runtime, interop,
      and CLI registrations live under `tests/cmake/`, with shared helpers and
      CTest convenience targets separated from the app build definition.
- [x] Traceability, freeze, and coherence membership is explicit additive
      labeling instead of test-name regexes. The freeze target runs the union
      once; test names, commands, environments, timeouts, and resource locks
      are preserved. A CMake catalog fixture checks label preservation,
      deduplication, renamed cases, and `BUILD_TESTING` on/off behavior.

## VM Native POD Layout and Field Materialization

- [x] Added an optional producer-owned POD layout/pack/read sidecar without
      changing wire-v2 or bytecode versions. Layout registration validates field
      names/types, stable nominal identities, size/alignment/bounds/overlap, and
      conflicts before publishing a snapshot with the native function registry.
- [x] Local VM native components cross scalar/nested-POD adapters; native-produced
      PODs expose normal fields and mutable places. Concrete generic PODs, empty
      components, whole-value aliases, and native async POD payloads follow the
      same path. Old producers without the sidecar keep raw-byte round-trips.
- [x] Preserved C++ fields omitted by Wio through immutable backing/prototype
      storage, and preserved pre-existing field borrows during nested copy-back.
      Partial/failing adapter outputs use the normal exactly-once wire release.
- [x] Fixed ignored native aggregate constructor arguments in the VM and
      order-dependent native aggregate construction in generated C++. Native
      char codecs now exchange unsigned byte values on signed-char hosts.
      Native generic POD fields accept enum/flagset underlying scalars, with
      enum definitions emitted before native template layout assertions.
- [x] Added focused malformed-layout/ownership tests and a real generated-thunk
      C++/VM differential fixture covering padding and reordered declarations,
      hidden C++ fields, generic/enum/flagset/scalar fields, local construction,
      field mutation, copy isolation, references/aliases, and async results.
      Remaining field shapes and structured callbacks remain explicit backlog
      work; this slice is not a full native parity claim.
- [x] Local Windows qualification: both new tests plus the updated native
      differential gate passed (3/3, 34.48 seconds). No full-suite or local Linux
      pass is claimed; the `native-differential` labels select CI coverage.

## VM Native POD Fixed-array Fields

- [x] Native POD field adapters now accept concrete fixed arrays of supported
      scalars/enums/flagsets, nested native PODs and further fixed arrays,
      including concrete generic component fields and zero extents. The
      additive sidecar, wire-v2 and bytecode versions are unchanged.
- [x] Added the `NativePodArrayCodec` helper with checked element counts,
      scalar ranges/representations, recursion/size limits, contiguous field
      extents and nested producer-layout lookup. Empty-array storage and
      nested-empty strides use producer metadata, not a guessed MSVC/GCC size.
- [x] Native mutable copy-back preserves existing element/field places inside
      arrays, nested POD hidden fields, independent copies and async results.
      Native enum metadata is not emitted as a component layout descriptor.
- [x] Added focused negative/ownership/registration codec tests and a generated
      C++/VM differential fixture for local construction, native factories,
      generic arrays, multidimensional and empty fields, native enum arrays,
      high-byte chars, bool/f32 fields, borrows, copies and async results.
- [x] The two new Windows gates pass; full-suite and local Linux qualification
      are not claimed. Opaque/nullable fields, const-generic/pack shapes,
      member-default initialization and aggregate/field overlapping arguments
      remain tracked in the active backlog.

## VM Native POD Opaque and Nullable-opaque Fields

- [x] Native POD adapters now accept `opaque`/`opaque?` fields and aliases,
      pointer arrays, nested PODs and concrete generic components. The existing
      C++ `void*` representation is used without changing ABI/bytecode versions.
- [x] VM registration validates pointer storage width; field decoding checks
      registered wire identities/kinds and normalizes nullable payload identity.
      Array codecs check value kinds and distinguish nullable null from a raw
      opaque value. Unsupported nullable wrappers remain rejected.
- [x] Fixed C++ backend nullable wrapping of opaque pointers: generated code
      forwards the pointer or emits `nullptr`, never invalid `void*{...}` syntax.
- [x] Added a real generated-thunk C++/VM differential fixture covering local,
      native, alias, generic, nested and async values, null/pointer arrays, hidden
      fields, copy isolation, mutable copy-back and pre-existing field/element
      borrows. Added malformed metadata/wire, owned-pointer lifetime/failure
      cleanup and frontend nullable-surface rejection tests.
- [x] Ownership remains explicit: raw POD pointer fields/arrays borrow pointees;
      copying producer bytes does not infer resource retention/deletion. Owned
      field-read handles retain their producer until the decoded value dies.
      Library pinning descends into decoded fields/elements; an independent-DLL
      test qualifies escaped owned pointers and borrowed native static data
      after machine teardown, including the final array-only pin.
- [x] Four new focused Windows gates and the adjusted array-codec gate pass.
      No full-suite or local Linux qualification is claimed. Const-generic/pack
      shapes, member-default initialization and overlapping aggregate/field
      arguments remain open; this is not a full native parity claim.

## VM Native POD Const-generic and Variadic Components

- [x] Qualified concrete integer const/default/zero-extent arguments, native
      partial/explicit specialization layouts, variadic type arguments and an
      empty trailing pack. Template argument closure is checked separately from
      physical field ABI, so unused string/text arguments do not exclude an
      otherwise POD-compatible C++ type. Actual pack storage remains unsupported.
- [x] Matched equal evaluated integer const values independently of their
      literal/const-symbol width while preserving different-value allowlist
      rejection. Native generic function allowlists and the existing
      nonnegative const-argument domain remain unchanged.
- [x] Re-selected generic signatures through the primary specialization
      registry and preserved semantic specialization layout provenance in WIR.
      Generic materialization no longer replaces selected fields with primary
      fields. Empty pack-only bindings emit C++ `Template<>` rather than an open
      template name.
- [x] Fixed type-table pointer invalidation while building nested array
      expressions/places. Concrete variadic and partial-specialized native
      fields now undergo analysis-time POD compatibility checks.
- [x] Fixed ignored memberwise arguments for ordinary VM Wio components, and
      lowered Wio component field defaults previously skipped by both backends.
      The new differential fixture separately verifies generic component
      round-trips, no-argument defaults, explicit memberwise arguments and
      explicit constructors.
- [x] Resolved default lifecycle bodies independently of virtual method
      layouts, which intentionally exclude constructors. Default component and
      object constructors run once after field initialization, not zero times
      or again through the VM construction continuation.
- [x] Added adapter/registry contract tests, a generated C++/VM differential
      fixture covering local/native/default/zero/specialized/variadic/empty-pack/
      unused-type/nested/ref/borrow/copy/async cases, and frontend rejection
      fixtures for non-POD variadic/specialized fields, pack storage and
      non-allowlisted const values.
- [x] All three new focused Windows gates passed (3/3, 20.06 seconds).
      No full-suite or local Linux pass is claimed. Existing CI labels select
      these gates; wire-v2, the layout sidecar and bytecode versions are unchanged.
      Visible C++ member defaults and overlapping aggregate/field arguments
      remain separate backlog work.

## VM Native POD Producer Defaults

- [x] Added the optional version-1 default factory sidecar alongside the
      unchanged version-1 layout API. Generated producers evaluate C++ `T{}`
      and return checked owned POD output; the existing registration overload,
      wire-v2 and bytecode versions remain unchanged.
- [x] Qualified explicit/implicit defaults, memberwise overrides, nested and
      fixed-array defaults, concrete generic/zero-extent/empty PODs, native
      fields in Wio components/objects and native async results. Wio memberwise
      arguments do not execute defaults for native fields already supplied.
- [x] Preserved hidden producer defaults through immutable prototypes and
      fixed incidental C++ default initialization during POD decoding by using
      byte-copy/bit-cast materialization. Independent initializer counters
      verify exactly-once construction across copies and native calls.
- [x] Validated default registration transactionally and checked owned wire
      identity, size and owner operations. Partial/failing/malformed factory
      results release owned storage once. Legacy fully specified construction
      still works; missing required default factories report `WVM1223`.
- [x] Pinned producer libraries through decoded opaque fields and immutable POD
      backing bytes. A real independent-DLL gate checks escaped static pointers
      after machine teardown, including the final backing-only lease. Raw
      pointers remain borrowed rather than implicitly owned.
- [x] Added four focused gates, including generated C++/VM differential tests
      and a module with defaults but no native functions. All passed locally
      on Windows (4/4, 23.66 seconds). No full-suite or local Linux pass is
      claimed; overlapping aggregate/field argument aliases remain open.

## VM Native POD Overlapping Reference Projections

- [x] Deduplicated native ref/view tokens by actual VM storage cell rather than
      place-wrapper identity. Independently acquired aliases share storage;
      independent copies remain independent.
- [x] Added bounded producer-layout projection lookup for POD fields, nested
      PODs and concrete fixed-array elements, including multidimensional arrays.
      Native thunks prebind outer POD roots regardless of parameter order and
      projected fields/nested aggregates share one typed producer-frame object.
- [x] Qualified mixed view/ref access, mutable fields under view roots,
      hidden-field preservation and live pre-existing borrows. Dirty roots
      commit/copy-back once; projected child tokens do not independently
      overwrite them. Bytecode pins primitive ref-parameter codec identities.
- [x] Added checked call-scoped projection descriptors and an additive producer
      capability bit. Legacy producers explicitly reject overlapping calls as
      `WVM1225`; existing ABI structure sizes and format versions are unchanged.
      Retained references, structured callbacks and ownership transfer remain
      separate contracts.
- [x] Fixed MSVC POD owner-operation instantiation by using named operations
      instead of incomplete-class lambdas in static initializers. Added contract
      tests for malformed metadata, mutability/alignment/bounds, repeated commit,
      exception copy-back and storage identity.
- [x] Added a generated C++/VM address-identity differential fixture covering
      reversed/nested/array/scalar/pointer/alias/copy scenarios, plus a real
      capability-disabled registry rejection. Both new focused Windows gates
      passed (2/2, 15.25 seconds). No full-suite or local Linux pass is claimed.

## VM Native Any Object Payloads

- [x] Qualified direct synchronous Any/ref-Any/view-Any boundaries for hosted
      object families, base/interface payloads and concrete type/const generic
      objects. Any-only boundaries discover their required object adapters.
- [x] Preserved exact producer payload cell types, concrete object wire identity,
      shared mutation and VM host identity across native retention and returns.
      Missing object registration and forged identities reject explicitly.
- [x] Removed extra owning claims from borrowed Any loads and owned place
      transfers; release overwritten Any values and owned fit temporaries at
      their lexical cleanup points rather than function teardown.
- [x] Added a focused generated-C++/VM/live-host gate with 30 lifecycle rounds,
      rebinding, self-assignment, escaping values and negative ABI checks.
      The new focused Windows gate passed (1/1, 25.29 seconds).
      Broader nullable/Box/container/callback/async contracts remain open;
      this does not close native parity or claim a full-suite/Linux pass.

## VM Native Concrete Generic POD Any and Containers

- [x] Shared physical native POD eligibility between POD, Any, dynamic array
      and dictionary producer adapters. Concrete generic arguments no longer
      disqualify an otherwise supported layout; open/non-POD storage still does.
- [x] Qualified type/const/default/zero-extent and selected-specialization shapes,
      concrete variadic/empty packs, unused template types and nested POD fields.
      Native hidden state, specialization identity and value/ref semantics remain
      consistent between C++ and VM execution.
- [x] Added a focused 30-round differential fixture and live-host selection/code-
      lease checks, including escaped Any/POD/pointer/backing-only ownership.
- [x] Fixed contextual Typed WIR conversion's stale destination pointer after
      recursive generic expression building grows the interned type table.
      Arbitrary nullable/Any-bearing/object container, Box and callback/async
      contracts remain separate work; no broad native closure is claimed.
      The new focused Windows gate passed (1/1, 30.78 seconds); no full-suite or
      local Linux pass is claimed.

## VM Native Nullable Handle Any Payloads

- [x] Qualified direct synchronous native Any/ref-Any/view-Any boundaries for
      nullable owning object/interface/concrete generic object and opaque handles.
      Null handles erase to null Any; present handles share the ordinary payload
      descriptor, without duplicate nullable adapters or invented typed nulls.
- [x] Preserved static Base/interface payload shape, concrete object identity,
      native retention, alias mutation and exactly-once destruction. Nullable
      type tests/casts use the canonical handle type; null Any checked casts
      still fail with `WVM1184`. Zero opaque pointers erase consistently too.
- [x] Unified nullable Any semantic eligibility for owning handles while keeping
      borrowed references and callbacks out of Any storage. Added focused
      rejection fixtures rather than widening their lifetime contracts.
- [x] Fixed flow-narrowed object fit operands and nullable/inner opaque equality
      lowering without relaxing Typed WIR verifier requirements.
- [x] Added a focused 30-round generated-C++/VM/live-host gate, including generic
      objects, null/present replacement, escaping values, missing adapters and
      forged payload identity rejection. The new Windows gate passed
      (1/1, 25.39 seconds). ABI/bytecode versions are unchanged; broader native
      parity remains open. No full-suite or local Linux pass is claimed.

## VM Native Any and Object Container Values

- [x] Qualified direct synchronous dynamic arrays/dictionaries containing Any
      cells and supported hosted object/interface/concrete generic object handles.
      Container-only boundaries discover their object/Any dependencies; nested
      conversion propagates call-local object access without a global bridge.
- [x] Validated the full object/Any/container candidate graph atomically. Missing
      sidecars, mismatched element metadata and forged nested object wires reject
      explicitly; partial decoding does not lose object lifecycle cleanup.
- [x] Preserved independent container value copies, shared Any cells/object handles,
      nullable object elements, native-held containers and same-shape ref copy-back
      into existing borrowed element cells.
- [x] Fixed literal operand cleanup and VM owned container place transfers.
      Borrowed container loads and caller-rooted projections no longer acquire
      hidden frame-long ownership. Projection retirement is allocation-free and
      only traverses a root's own linked descendants, including unwind/global
      cleanup. Expired projections reject instead of reading released storage.
- [x] Lowered nullable object equality through the existing identity opcode with
      narrow Typed/Lowered WIR validation, without widening object cast rules.
- [x] Added a focused 30-round generated-C++/VM/live-host gate covering arrays,
      ordered/unordered dictionaries, generic/interface/nullable handles, boxed
      container growth, ref aliases, retention, malformed metadata, partial decode
      failure, expired borrows and escaped Any-child/backing DLL leases.
      The new Windows gate passed (1/1, 35.08 seconds). No full-suite or local
      Linux pass is claimed.
      Existing ABI and bytecode versions are unchanged. Boxed Any-in-Any, std::Box,
      arbitrary foreign/container ownership, projection arguments, callbacks,
      consumed arguments and async lifetime contracts remain separate work.

## Free-Native Caller-Bound Borrowed Results

- [x] Enabled synchronous native ref/view results backed by an exact-compatible
      input reference cell. C++ wrappers validate the native result address;
      ABI thunks name the caller token, never their temporary frame storage.
      VM results reuse the original caller Place with no new owning claim.
- [x] Preserved ref rebinding, readonly views, same-cell aliases and forwarding
      through Wio frames. The focused surface includes scalar/string/text,
      registered POD, Any/array/dictionary, hosted object/interface/concrete
      generic and nullable object/opaque handle cells.
- [x] Added structural semantic identity matching for native borrow cells,
      excluding numeric promotion, readonly upgrades and mismatched nested types.
      Native and indirect call origins include every compatible candidate;
      a caller-owned first argument cannot hide a later local borrow escape.
- [x] Added a 30-round C++/VM/live-host qualification plus nine new semantic
      rejection fixtures. Host checks reject unrelated producer/global storage,
      projected overlap returns, forged pointer/type/ownership/mutability/size
      contracts and reads after caller root cleanup; object cleanup balances.
      Narrowed nullable/covariant input cells and implicit native receivers reject
      explicitly; borrowed-result bytecode cannot use owned-result view narrowing.
      Existing ABI and bytecode formats were unchanged. At this stage, projected result mapping,
      arbitrary foreign storage, async/callback lifetime and SDK-exported borrow
      results remain separate work; Native Step 8 is still open.
      Only the ten new tests were run on Windows: 10/10 passed in 18.32 seconds
      (differential/live-host gate: 17.12 seconds). No full-suite or local Linux
      qualification is claimed.

## Explicit Native Any-in-Any Payloads

- [x] Emitted a self-payload Any adapter at every qualified native Any boundary,
      including container-only signatures without AnyBox instructions. Outer
      and inner producer cells remain distinct; wire/sidecar/bytecode versions
      are unchanged.
- [x] Enabled checked `is any`, `fit any` and pattern-bound extraction of a nested
      Any layer without changing ordinary shared-cell Any assignment semantics.
      Null and ordinary scalar boxes do not become identity casts.
- [x] Preserved nested cell mutation/rebinding, boxed null, Unicode, containers,
      hidden native POD state and hosted object retention. Final VM Any cleanup
      releases native proxy backing before child cleanup, fixing delayed scope
      destruction while preserving shared cells and destructor failure reporting.
- [x] Added a focused C++/VM/live-host gate with 30 lifecycle rounds, bounded and
      cyclic conversion rejection, partial-failure ownership, concurrent packing,
      malformed registration and independent escaped child/backing DLL leases.
      Native Step 8 remains open; std::Box, arbitrary ownership and callback/async
      contracts remain separate work. Only the new focused Windows gate was run:
      1/1 passed in 14.76 seconds (14.83 seconds total). No full-suite or local
      Linux pass is claimed.

## Structural Native Container Copy-Back

- [x] Enabled dynamic array resize/clear and ordered/unordered dictionary key-set
      updates at native `ref` boundaries. Same-shape cells stay stable; surviving
      dictionary key/value places remap while removed/reallocated descendants
      reject stale reads. Unaffected siblings and independent heap fields remain
      valid. Shared iterator/snapshot storage and overlapping native arguments
      still reject explicitly.
- [x] Staged all decoded mutable outputs and recursive copy-back actions before
      caller graph mutation. Added a narrow internal planner with Value friend
      access, leaving public mutable container internals private.
- [x] Drained retired hosted proxy claims before old-value recursive cleanup,
      preserving exactly-once destruction and reporting failing destructors at
      the native call boundary. Intrusive projection-domain teardown detaches
      escaped owner-backed places from the destroyed program's tracking list.
- [x] Added a 30-round C++/VM/live-host gate for nested containers, scalar/string/
      text/bool/POD/Any/generic/nullable-object values, surviving mutable borrows,
      invalid descendants, native/cleanup failures, recovery, malformed second
      outputs with observable caller roots, and mapping/storage/depth checks.
      ABI and bytecode versions are unchanged. Native Step 8 remains open.
      Only the new focused Windows gate was run: 1/1 passed in 23.63 seconds
      (23.71 seconds total). No full-suite or local Linux pass is claimed.

## Dynamic-Array Native Reference Projections

- [x] Added capability-gated, call-scoped logical index projection references
      without changing wire-v2, sequence sidecar-v1 or bytecode versions.
      Producer Frame roots are bound first; nested index resolution preserves
      actual C++ element/row address identity regardless of parameter order.
- [x] Preserved repeated aliases, readonly roots with mutable elements, outermost
      copy-back and structural nested-row growth. Redundant projected token
      copies release object/Any claims before invocation; nullable object storage
      and static native handle IDs remain distinct.
- [x] Kept root-bound borrowed results working, while rejecting projected-element
      results, missing producer capability and unqualified dictionary/bool-proxy/
      container-to-POD-field paths at this stage (extended by the next slice).
      Native code still obeys C++ invalidation rules.
- [x] Added a focused 30-round C++/VM/live-host gate plus producer contract tests
      for scalar/string/text/POD/Any/generic/nullable-object elements, reversed
      order, nested aliases/growth, stable siblings, lifecycle cleanup/failures,
      ten malformed path/wire variants, capability rejection and recovery.
      Only this new Windows gate was run: 1/1 passed in 21.56 seconds (21.64
      seconds total). Old expectations were updated but old gates were not run.
      Native Step 8 remains open; no full-suite or local Linux pass is claimed.

## Dynamic-Array / Native POD Field Projection Composition

- [x] Added an independent sequence/POD capability and projection payload without
      changing existing wire, sidecar, bytecode or projection structure versions.
      Logical indices select the producer POD; generated typed accessors validate
      declared fields, stable identities, exact C++ types, offsets and extents.
      Padding and undeclared hidden-field requests are rejected.
- [x] Preserved one outermost producer address/copy-back slot across array, POD,
      nested generic field, fixed-array element/row and repeated alias arguments.
      Reverse order and mutable fields under readonly roots work. Different Wio
      declarations of one C++ POD share a specialization but retain separate
      contract identities and declared field surfaces.
- [x] Qualified fixed-array descendants only with a containing POD projection;
      standalone unsupported fixed-array references still reject explicitly.
      Native vector invalidation rules and VM descendant expiry remain enforced.
      At this stage, projected borrowed results and dictionary/bool-proxy paths were excluded.
- [x] Added a new 30-round C++/actual-VM/live-host gate with 18 malformed ABI cases,
      missing capability, expired borrow/native failure/result rejection, native
      entry counters, hidden-state preservation, compatibility and recovery checks.
      Only this new Windows gate was run: 1/1 passed in 12.83 seconds (12.90 total).
      The old POD-field expectation was updated but its gate was not run.
      Native Step 8 remains open; no full-suite or local Linux pass is claimed.

## Dictionary Value and Mixed Native Container Projections

- [x] Added capability-gated mixed index/key projection payloads without changing
      existing ABI/sidecar/bytecode or old projection structure sizes. Typed
      producer accessors validate container/child/key identities and key codecs,
      index bounds, existing-key lookup and terminal C++ type. Keys own their
      portable wire storage for the call and release with the native frame.
- [x] Preserved one outermost producer slot for array/map/nested-map/POD/fixed-array
      paths, repeated aliases, reversed argument order and readonly roots with
      mutable children. Key-based copy-back preserves surviving borrows after
      rehash/insert/remove; removed descendants expire. Hidden POD state survives.
- [x] Qualified Any and generic/nullable object mapped values, exactly-once
      ownership cleanup and copy-back destructor failure reporting. Root-bound
      results work; at this stage, projected results, map-key references, bool
      proxies and broader callback/foreign ownership remain excluded.
- [x] Added a new 30-round C++/actual-VM/live-host gate with 32 forged dictionary/
      mixed-path variants, native-entry counters, old capability rejection,
      lifecycle balance and failure recovery. Only this new Windows gate was run:
      1/1 passed in 33.27 seconds (33.34 total). The old overlap negative expectation
      was updated but that gate was not run. Native Step 8 remains open;
      no full-suite or local Linux qualification is claimed.

## Caller-Bound Projected Native Borrowed Results

- [x] Added a result-only input-argument alias flag and producer capability.
      Existing ABI/sidecar/bytecode versions and structure sizes stay unchanged.
      Generated thunks compare exact compatible ref/view addresses and re-resolve
      the live producer path after native execution. VM results name only exact
      caller argument wires; foreign pointers/descriptors are never dereferenced.
- [x] Enforced exact type/marshalling, per-argument mutability, capability and
      post-copy-back Place lifetime. Surviving dictionary values work after
      insert/erase-other/rehash; structurally retired array elements reject.
      Returning a newly selected relocated element or foreign/global address is
      not a caller borrow. Native code must still obey ordinary C++ lifetimes;
      this contract does not detect arbitrary foreign erase/reinsert ABA misuse.
- [x] Fixed Wio-frame forwarding of projected references by moving only the
      PlaceStorage metadata into the caller. Root links, invalidation and borrowed
      ownership remain intact. Frame-local projection metadata cannot escape as
      a direct public VM-host result.
- [x] Fixed VM structural array intrinsic expiry before vector relocation/erase.
      No-op reserve, missing-value remove and empty extend retain valid borrows.
- [x] Added a new 30-round C++/actual-VM/live-host gate covering scalar/string/POD,
      generic fields, fixed/dynamic rows, dictionary/mixed paths, readonly/repeated
      aliases, forwarding, root rebinding, native failures, missing capability,
      14 malformed/mutability result variants and 11 structural array operations.
      Only the new Windows gate was run: 1/1 passed in 17.01 seconds (17.08 total).
      Four old result expectations were updated but their gates were not run.
      Native Step 8 stays open; projected Any/object/nullable result qualification,
      dictionary intrinsic remapping, map-key/bool-proxy references and broader
      callback/foreign ownership remain separate work. No full-suite or local
      Linux qualification is claimed.

## Dictionary Intrinsic Borrow and Cleanup Consistency

- [x] Extracted VM Place storage/projection bookkeeping into named internal
      classes. Added mutation guards that capture live direct key/value aliases
      before relocation/reordering and rebind surviving cells by their keys.
      Repeated and owner-backed aliases preserve identity; removed cells expire.
- [x] Qualified Set/GetOrAdd/Extend/TryGet/Remove/Clear across intrinsic and native
      projected borrows. Direct mapped-cell references survive value assignment;
      replaced container descendants expire, and actual same-storage assignments
      preserve descendants. Remove/reinsert never revives expired aliases.
- [x] Added explicit VM cleanup for replaced/removed object claims, updated map
      state before destructor callbacks and preserved cleanup errors/recovery.
      Dictionary insertion reserves a whole key/value pair with geometric growth.
- [x] Fixed Typed WIR dictionary intrinsic argument lifetime in both C++ and VM:
      owning argument copies release after the call/result rather than retaining
      objects until function teardown. Borrowed outputs remain borrowed.
- [x] Added a focused 30-round C++/actual-VM/live-host gate covering ordered and
      unordered mutation, repeated aliases, mapped scalar/string/array/POD/object/
      nullable-object results, lexical/output/fallback/extension ownership,
      direct key-cell unit checks, child expiry, three destructor-failure paths
      and recovery. Native Step 8 stays open; native key-reference signatures,
      projected Any results and broader ownership/callback work remain separate.
- [x] Only the new Windows gate was run: 1/1 passed in 23.36 seconds (23.47
      total). No full-suite or local Linux qualification is claimed.

## Caller-Bound Projected Any Borrowed Results

- [x] Qualified projected `ref/view any` results with the existing exact
      caller-argument wire identity and live-path checks. No new ABI/sidecar/
      bytecode version, compiler feature or runtime representation is needed.
- [x] Qualified array/map/mixed paths, reverse readonly inputs, repeated aliases,
      distinct Any wrapper cells sharing one payload, Wio-frame forwarding and
      scalar/string/text/POD/nested-Any/object/nullable-object/null replacement.
      Shared payload mutation and independent owning copies retain their
      existing semantics; a borrowed wrapper does not own its container.
- [x] Added a 30-round C++/actual-VM/live-host gate covering owning return escape,
      array structural expiry, root rebinding, erased/cleared map entries,
      relocated/foreign results, native/destructor failure and recovery.
      Seventeen forged-result variants, readonly-to-mutable alias upgrade and
      missing producer capability are rejected with the existing diagnostics.
- [x] Only this new Windows gate was run: 1/1 passed in 14.18 seconds (14.25
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for broader object/nullable/callback lifetime and
      foreign ownership, map-key/bool-proxy/iterator surfaces and packaging.

## Caller-Bound Projected Object, Interface and Nullable Results

- [x] Qualified existing supplied-handle `ref/view` result contracts across Wio
      objects, concrete type/value generics, interfaces and nullable variants.
      Array/map/mixed paths preserve direct cell identity through compatible
      rebind/null transitions. Two cells holding one object do not collapse.
- [x] Qualified reverse readonly inputs, repeated aliases, Wio forwarding,
      owning return escape, native member entry and balanced intrusive cleanup.
      The existing ABI/sidecar/bytecode forms and versions remain unchanged.
- [x] Fixed virtual/interface dispatch diagnostics for expired receiver Places:
      WVM1043 now reports lifetime expiry before resolving a live object.
      Ordinary null/non-object receivers keep the preceding dispatch diagnostic.
- [x] Added a 30-round C++/actual-VM/live-host gate with array/root/map expiry,
      relocated/foreign/native-failure results, two copy-back destructor-failure
      paths, 19 malformed/mutability/nullable-contract variants, missing
      capability and recovery. Every host failure path checks object balance.
- [x] Added an analyzer-negative gate for a nullable result without a compatible
      nullable caller reference. Required and nullable reference contracts are
      not interchangeable merely because they share an underlying object type.
- [x] Only these two new Windows gates were run: 2/2 passed, with the runtime
      matrix taking 30.08 seconds and the negative gate 0.09 seconds (30.31
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for callback/async lifetime, foreign ownership,
      consumed arguments, remaining projection surfaces, pruning and packaging.

## Qualified By-Value Native POD Callbacks

- [x] Added VM callback argument/result conversion for registered native POD
      components using the existing layout adapters, including concrete generic,
      nested and fixed-array fields. Ref/view, nullable-object and unsupported
      structured signatures remain rejected before native entry.
- [x] Generated callback codecs now carry resolved argument/result identities,
      including two Wio declarations backed by the same C++ struct. Same-symbol
      call/retained and caller/any contracts have distinct native thunk keys;
      default call/caller keys remain unchanged. Wire-v2 and sidecar versions
      remain unchanged; non-default callback bridges must be rebuilt together
      with their bytecode because their previously ambiguous keys are corrected.
- [x] POD callback results own VM byte storage, not temporary frame or producer
      storage. Data and hidden native fields survive callback, machine and DLL
      teardown. Argument conversion cleanup is installed before creating owned
      wire arguments, so a later conversion failure cannot leak earlier values.
- [x] Added a new 30-round C++/actual-VM/live-host gate covering distinct contract
      identities, multiple arguments, padding, hidden fields, native/local POD,
      empty/generic POD, independent copies, retained/worker calls, signature
      rejection, six malformed inputs, wrong argument count, native result type
      validation, panic containment/recovery and stale-owner entry. The existing
      differential gate's POD callback expectation is updated, but not run.
- [x] Only this new Windows gate was run: 1/1 passed in 16.61 seconds (16.69
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for broader borrowed/object/container/nullable
      callbacks, async lifetime, foreign ownership, pruning and packaging.

## Qualified By-Value Wio Object Callbacks and Capture Cleanup

- [x] Added native free-function callback argument/result conversion for Wio
      objects, concrete type/value generic objects, interfaces and nullable
      variants, guarded by registered object proxy contracts. Object adapter
      discovery now includes types reachable only through callback signatures.
- [x] Preserved original VM identity and mutation visibility across C++ proxies;
      wrong identities, malformed handles, untagged null, cross-machine and
      arbitrary foreign-owned objects remain rejected rather than reinterpreted.
      Existing wire/sidecar/bytecode versions are unchanged.
- [x] Added a named callback lifetime domain: final native release retires a
      closure without executing Wio on a foreign thread. VM safe points drain
      captures through normal Wio cleanup; shutdown drains outstanding leases
      and later callback entry returns stale-handle. Destructor failures are
      reported as WVM1211 at the native call boundary.
- [x] Fixed shared Typed WIR capture staging ownership. Closure creation copies
      capture values, so its temporary retain is now released immediately;
      objects no longer survive lexical cleanup until enclosing-frame teardown.
- [x] Added a 30-round C++/actual-VM/live-host gate for identity/mutation,
      multi-argument callbacks, callback-only result types, nullable interfaces,
      generic strings with NUL/Unicode, call/retained and caller/any contracts,
      multiple leases, foreign-thread final release, wrong count/thread, six
      malformed arguments, cross-machine rejection, panic/recovery, cleanup
      failure and external callback/result wires during owner shutdown.
- [x] Only this new Windows gate was run: 1/1 passed in 23.35 seconds (23.45
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for borrowed/container/hosted-method callbacks,
      async lifetime, broader foreign ownership, pruning and packaging.

## Qualified By-Value Native Container Callbacks

- [x] Added native free-function callback arguments/results for arrays,
      dictionaries and Any through registered sidecars. Discovery includes
      callback-only structural signatures and their nested codec identities.
      Wire, sidecar and bytecode versions remain unchanged.
- [x] Recursive callbacks preserve hosted object/interface identity and reject
      another machine's objects. Nullable object elements require explicit
      nullable tags, just like direct callback parameters.
- [x] Cached Any results now acquire an independent owning claim. Owning
      container callback results retain libraries loaded by the VM, including
      any pre-existing wire owner. Final release retires the code lease rather
      than unloading producer code while returning through its ABI frame;
      live-machine and explicit host safe points drain retired leases.
      Manually registered APIs still require host-managed producer lifetime.
- [x] Fixed Typed WIR method dispatch across type-table growth: complex receiver
      lowering could invalidate a type pointer and emit a direct call to an
      abstract interface method. Dispatch kind is now captured by value.
- [x] Added a 30-round C++/actual-VM/live-host gate covering empty/bool/string/
      Unicode/POD/nested/object/interface/nullable arrays, ordered/unordered maps,
      nested Any, callback-only map factories, retained worker invocation,
      missing sidecars, malformed input/tag rejection, cross-machine isolation,
      panic/recovery and balanced destruction. Escaped array/map/Any results are
      read after the machine, callback and manual loader have closed.
- [x] Only this new Windows gate was run: 1/1 passed in 43.22 seconds (43.43
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for borrowed and hosted-method/callable-valued/fixed-
      array callbacks, async lifetime, foreign ownership, pruning and packaging.

## Qualified Hosted-Method Native Callable Arguments

- [x] Hosted Wio methods accept native callback function values for qualified
      by-value scalar/string/text/POD/object/nullable/container signatures.
      Method-only callback/container dependencies are discovered transitively
      and structural callable IDs are preserved in bytecode reflection.
- [x] Added named native callable backing using the aggregate's existing native
      backing slot and intrusive retain/release. Normal Wio closures gain no
      fields or allocations. Loaded producer libraries stay pinned by foreign
      callables and captured native values; manual registries still require
      host-managed producer lifetime.
- [x] Added native indirect invocation and forwarding through free native
      callback boundaries. Wio closures may capture the native function value;
      retained caller-thread leases preserve it after hosted method return and
      drop the owning claim at the normal VM safe point.
- [x] Shared native codec spelling now retains exact callback signature IDs in
      both free-function thunks and hosted object method adapters. Wrong type,
      kind, operations, ownership/thread flags or missing sidecars reject entry;
      callback errors and result mismatches stay inside the ABI boundary.
- [x] Added a 30-round C++/actual-VM/live-host gate for direct/captured/retained
      forwarding, NUL/Unicode, native POD hidden fields, independent array/map
      storage, Any, nullable object identity, ten malformed contracts,
      exception/result mismatch recovery, receiver affinity and stale shutdown.
      Borrowed callback signatures remain unadvertised and rejected.
- [x] Only this new Windows gate was run: 1/1 passed in 18.17 seconds (18.25
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for ref/view and callable-valued/fixed-array callbacks,
      callable method results, async lifetime, foreign ownership, pruning and
      artifact packaging.

## Qualified Scalar And Native POD Callback Borrows

- [x] Added invocation-scoped scalar and qualified native POD ref/view callback
      arguments, both Wio-to-native and native-to-hosted-Wio. Direct invocation,
      forwarding and retained leases use the same concrete pointee IDs and
      mutability policy; no caller stack pointer is stored in a retained lease.
- [x] Added named NativeCallbackReferences for VM materialization, alias reuse
      and staged copy-back. Readonly and mutable aliases share one cell,
      independent of argument order. Mutable writes commit after both success
      and contained panic/exception; view access does not authorize writes.
- [x] Added CallbackArguments and typed callback argument codecs in the C++
      bridge. Owned wires are guarded before conversion and failure results
      are cleared/released. Ordinary by-value callbacks avoid borrow allocation.
- [x] Added a 30-round C++/actual-VM/live-host gate covering scalar and padded
      POD aliasing, hidden fields, hosted forwarding, retained fresh-cell calls,
      native exception copy-back, view write rejection, malformed token shape/
      flags/identity, mixed scalar alias types, range rejection, unsafe VM
      reference-capture rejection, Wio panic and
      stale shutdown. Updated older negative expectations without rerunning
      those gates, in accordance with the new-tests-only request.
- [x] Only the new Windows gate was run: 1/1 passed in 13.07 seconds (13.16
      total). No full-suite or local Linux qualification is claimed. Native
      Step 8 remains open for other borrow payloads/results, callable-valued/
      fixed-array signatures, async lifetime, ownership, pruning and packaging.

## Qualified String And Unicode Text Callback Borrows

- [x] Added invocation-scoped string/text ref/view callback arguments in both
      directions, including hosted Wio methods, forwarding and retained
      callbacks invoked later with fresh caller cells. Aliases retain shared
      storage and readonly access does not authorize mutation.
- [x] Owning string/text callback values now carry canonical identities in both
      their wire and owner operations. Mutable copy-back releases the previous
      owning claim exactly once; escaped results survive machine/callback and
      manual-loader shutdown.
- [x] C++ callback copy-back now decodes every replacement before writing any
      caller cell. C++ and VM reject mismatched pointee identities and invalid
      UTF-32 without committing earlier valid replacements. Completed valid
      writes still survive a contained callback panic.
- [x] Added a 30-round C++/actual-VM/live-host gate covering empty and 32 KiB
      strings, append, embedded NUL, emoji/ZWJ/combining text, both alias orders,
      hosted forwarding, retained fresh-cell calls, malformed reference/slice
      contracts, unaligned UTF-32 data, invalid Unicode, ownership, recovery and
      stale machine shutdown. Byte strings remain binary-safe rather than
      receiving Unicode validation.
- [x] Only this new Windows gate was run: 1/1 passed in 17.52 seconds (17.61
      total). The older unsupported hosted callback fixture now uses container
      references instead of newly supported string references; it was not rerun.
      No full-suite or local Linux qualification is claimed. Native Step 8
      remains open for object/container/nullable reference arguments, borrowed
      results, callable-valued/fixed-array signatures, async lifetime, broader
      ownership, pruning and artifact packaging.

## Qualified Container And Hosted Object Callback Borrows

- [x] Added invocation-scoped ref/view callback arguments for qualified dynamic
      arrays, ordered/unordered dictionaries, Any, hosted objects/interfaces
      and nullable hosted objects in both directions. Borrowed results and
      callable-valued/fixed-array signatures remain excluded.
- [x] Separated static cell identity from dynamic object token identity.
      Object codecs validate assignability and original-machine provenance;
      nullable transitions preserve the underlying nominal identity and tag.
      Aliases share materialization only when their static cell types agree.
- [x] Added invocation-local lifecycle hooks to callback materialization.
      Decoded backing pins producer libraries; mutable container copy-back
      receives an owning code lease. Replaced values and staged failures use
      normal Wio cleanup rather than only dropping storage references.
      Materializations remain alive until callback result encoding completes.
- [x] Hosted proxy qualification now accepts private dynamic array/dictionary/
      Any fields that default to empty native storage. Public fields, fixed
      arrays and lifecycle-bearing component storage remain unqualified.
      The new processor fixture owns an Item array and checks balanced
      destruction without constructing elements in native host placeholders.
- [x] Added a 30-round C++/actual-VM/live-host gate for nested arrays, bool/string/
      text/native-POD/object/nullable elements, maps, Any payload replacement,
      object/interface rebinding, null transitions, readonly-only access,
      reversed aliases, retained fresh-cell calls, hosted forwarding, malformed
      handles and missing sidecars, cross-machine rejection, two-cell validation
      before publication, unsafe VM borrow capture, panic/recovery and reads of
      escaped container results after machine/callback/manual-loader shutdown.
- [x] Only this new Windows gate was run: 1/1 passed in 50.73 seconds (50.84
      total). The older negative hosted fixture now uses a callable-valued
      callback signature, which remains unsupported; that gate was not rerun.
      No full-suite or local Linux qualification is claimed. Native Step 8
      remains open for borrowed results, callable-valued/fixed-array signatures,
      async lifetime, broader foreign ownership, pruning and artifact packaging.

## Qualified Fixed-Array Native And Callback Boundaries

- [x] Added structural sequence transport for concrete fixed arrays, including
      zero extents, standalone native arguments/results, callback values and
      invocation-scoped ref/view callback arguments in both directions.
      Recursive payloads include nested/dynamic arrays, dictionaries, Any,
      string/text, native POD and hosted object/interface/nullable objects.
- [x] Kept native POD fixed-array fields on their physical byte ABI using an
      explicit PodStorageCodec. An identical array type can now be a physical
      POD field and a standalone structural callback value in the same module.
      Sequence-sidecar-v1 and wire-v2 layouts remain unchanged.
- [x] Fixed-array packing constructs elements directly instead of default-
      constructing then overwriting them. A side-effecting native POD member
      initializer counter verifies that transporting existing elements does
      not execute initializers again.
- [x] Enforced exact fixed extents in producer packing and VM packing/decoding,
      including empty arrays. Added real fixed-array element projection and
      fixed bool addressability. Corrected C++ generation that incorrectly used
      vector<bool> proxies for fixed bool cells, losing native ref writes.
- [x] Added a 30-round generated-C++/actual-VM/live-host gate. It covers Unicode
      and binary strings, nested containers, object identity and balanced
      lifecycle, nullable transitions, hosted native callbacks, whole-array/
      element aliases, hidden/reordered POD fields, malformed wire admission,
      missing sidecars, producer count mismatches, wrong VM array extents,
      recovery and owning result reads after machine/callback/loader shutdown.
- [x] Only this new Windows gate was run: 1/1 passed in 31.21 seconds (31.28
      total). The older row-only projection expectation was updated to success
      but its gate was not rerun. No full-suite or local Linux qualification is
      claimed. Native Step 8 remains open for callable-valued signatures and
      method results, borrowed callback results, async/broader ownership,
      identity canonicalization, pruning and VM artifact packaging.

## Native Callback Closure Packages 1 And 2

- [x] Qualified recursive callable arguments/results and callback factories,
      including nullable native results, nullable hosted method parameters and
      callable/nullable-callable hosted method results. Concrete generated
      codecs preserve nested signature identities and validate ownership/ops.
- [x] Unwrapped function aliases consistently in semantic and Typed WIR
      indirect-call resolution.
- [x] Qualified synchronous ref/view callback results through an explicit
      compatible caller argument. Both sides restore the original caller cell
      before destroying callback materializations; readonly upgrades, unrelated
      addresses and implicit local/projected result escapes are rejected.
- [x] Covered scalar, string/text, native POD, fixed/dynamic array, dictionary,
      Any, object/nullable-object and callable cells. Nullable cell contents do
      not turn a live reference into an absent reference.
- [x] Prevented direct/nested lexical borrowed captures from crossing the
      native callable boundary. Invocation-scoped borrowed capture construction
      and expired materialization reads are rejected. Callable environments are
      checked again at native entry, including after mutation.
- [x] Fixed VM closure environment addressing: reference captures have their
      own hidden environment cell, and mutable value captures remain rooted in
      the same closure rather than a fresh per-invocation clone. Local borrowing
      closures remain usable while their caller cell is alive.
- [x] Preserved call-scoped versus retained leases and caller-thread versus
      any-thread entry. Retained ref results use fresh caller cells per call;
      callbacks reject entry after machine shutdown without reading stale memory.
- [x] Qualified owning callable/object captures across VM coroutine suspension,
      completion and cancellation, plus native worker callback execution.
      Captured object construction/destruction counts balance after completion
      and cancellation. Async ref/view parameters/results remain forbidden.
- [x] Added a 30-round generated-C++/actual-VM differential gate followed by
      live-host lifetime tests, and two async borrowed-parameter/result negative
      gates. Only these new Windows tests were run: 3/3 passed, 42.29 seconds
      total (41.54 seconds for the differential/live-host gate). No full-suite
      or local Linux qualification is claimed.
- [ ] Native Step 8 remains open for general native task-parameter/std helper
      parity, std::Box/broader foreign ownership and resource contracts,
      consumed arguments, alias identity canonicalization, native-only pruning
      and VM executable/shared/static packaging. These are not closed by the
      qualified callback/lifetime packages above.

## Native Ownership And Alias Closure Packages 3 And 4

- [x] Added `std::attribute::NativeConsume` for native parameters. Concrete WIR
      specializations preserve consume policy in their ABI signature identity.
      Borrow/opaque/callable/task values and non-native declarations reject.
- [x] Added an ABI-v2 consumed-argument capability without changing descriptor
      layout. Allocation-free thunk adoption and caller slot clearing balance
      transferred claims on decoding, native exception and successful return.
      Consumption transfers an invocation claim, not unique caller ownership;
      the existing intrusive reference counter and Wio copy/value rules remain.
- [x] Promoted borrowed foreign object handles to owning native claims correctly.
      Foreign object scope release, place transfers and live borrowed loads now
      follow the VM's existing ownership machinery. Constructor argument SSA
      claims retire once their constructor frame has acquired them.
- [x] Qualified hosted Box Get/Set/Replace/Clone/Swap for dynamic arrays/maps,
      Any and resource-handle payloads, including native-created handles.
      Method-only container signatures now receive producer and bytecode ABI
      identities. Raw host-placeholder field access is not a qualified API.
- [x] Canonicalized typed producer-cell identity across direct POD, sequence-POD
      and mixed container projections. POD accesses validate registered declared
      fields, never arbitrary padding/hidden storage. Repeated paths share one
      root, exception copy-back and caller-bound borrowed result.
- [x] Made unsupported callback aggregate/descendant overlap an explicit rejection
      before invocation in native wrappers and VM encoding. Exact typed-cell
      aliases and independent cells remain supported; use one aggregate borrow
      for children. This does not claim a new callback projection protocol.
- [x] Added focused C++/actual-VM/live-host resource and alias qualification,
      a runtime claim/projection contract unit, and five invalid consume gates.
      Host probes cover 40 cancellation/retention rounds and a producer-owned
      result surviving Machine/loader shutdown until its final release.
- [x] Increased Windows compiler stack reservation to demand-backed 16 MiB after
      the deep mixed-projection fixture exposed large unoptimized lowering
      frames on the default 1 MiB stack. No stackless-lowering claim is made.
- [ ] General native task-parameter/std helper parity and VM executable/shared/
      static packaging remain outside these two packages. The later native-only
      dependency selection slice is recorded below.
- [x] Only the seven new Windows tests were run: 7/7 passed in 47.09 seconds
      total (45.37 for differential/live-host qualification). Forged consumed
      reference/nullable-reference/opaque/callable bytecode also rejects.
      The older manual POD-alias fixture was updated for registered field access
      and built, not rerun. No full-suite or local Linux qualification is claimed.

## Native-only bridge dependency selection

- [x] Added an independent verified C++ emission view of Lowered WIR. Bytecode
      remains the full original program; stable module/function/global/type/ABI
      identities are not renumbered. A sparse dependency closure retains native
      bindings, POD descriptors, hosted hierarchies, lifecycle, method dispatch,
      callables, generic dependencies, globals, reflection and processors.
- [x] Separated full-program dynamic Any discovery from C++ function selection.
      Native payload/object/container/callback codecs remain available without
      retaining VM Entry or callback bodies. VM-only Any boxes do not become
      bridge roots without a native Any boundary.
- [x] Added compiler `--native-bridge-only` with strict shared/sidecar validation;
      VM project commands select it automatically. Ordinary sidecar and SDK
      producer behavior remains unchanged without the flag.
- [x] Added focused plan/size, native/full-VM/pruned-VM differential and project
      incremental gates. The synthetic size probe selects 21/154 functions and
      emits 81,462 rather than 167,393 bytes (51.3% smaller); a 25% minimum
      reduction budget is enforced. No runtime or universal build-time gain
      is inferred from this source-size measurement.
- [x] Only the three new Windows gates were run: 3/3 passed in 27.64 seconds
      on the final run (0.42 plan, 22.40 differential, 4.64 project). Rebuilt the
      self-hosted CLI companion for the updated project invocation. No old
      suite or local Linux qualification is claimed.
- [ ] General native task parameters and VM artifact packaging remain separate.
      The inferred literal-array Any ambiguity exposed here is fixed below.
      Final include/PCH/sharding/runtime cost work remains Backend item 9.

## Canonical Any array payload identity

- [x] Materialize compiler-internal literal-array shapes at AnyBox, recursively
      through array/dictionary children and after generic specialization. Keep
      source SSA types intact; declared fixed extents, nominal identities and
      callable signatures remain distinct. No ABI/bytecode version or intrusive
      Any cell layout change is needed.
- [x] Typed WIR, Lowered WIR and bytecode verifiers permit only that precise
      materialization, rejecting fixed-array and element-type identity forgery.
- [x] Added new unit and native/full-VM/pruned-VM differential gates for inferred,
      direct, nested, generic and declared fixed array payloads, independent
      copies, native generic echo and identical full/pruned bytecode.
- [x] Only the two new Windows gates were run: the final run passed 2/2 in
      25.09 seconds (0.68 unit, 23.81 differential). No old suite or local Linux
      qualification is claimed. Modified C++ ranges and new C++ files follow
      the repository clang-format configuration.
- [x] Direct/nested inferred-array argument WIR1422 is fixed by canonical owned
      container-copy materialization. Removed the nonnumeric same-TypeKind
      numeric-promotion shortcut without retagging source places or weakening
      numeric verification. Unconstrained T-to-Any remains unsupported.
- [x] New copy-verifier and C++/full-VM/pruned-VM gates passed 2/2 on Windows in
      15.03 seconds. Typed/Lowered/bytecode reject forged element identities;
      numeric Convert on array values still rejects. No old suite/Linux run.

## Native task inputs and standard async helper closure

- [x] Added owning VM/host task-source wires and concrete producer TaskCodec
      materialization, stable task/result identities and cancellation/failure
      preservation. Retain/release handles source owners separately from their
      slice payload. ABI-v2 thunk capability flags protect older producers.
- [x] Identity-preserving native helpers recover the original VM task through
      weak origins. Native source retirement, shutdown cancellation, Machine
      movement and different-Machine/generation rejection have live-host gates.
- [x] Blocking native waits, including composed tasks, cooperatively progress
      the owning VM main executor. Pending external/producer tasks are not
      synchronously waited during native argument conversion.
- [x] Qualified POD/object/Any/container/nested-task payloads and task callbacks
      are covered. Native object reachability follows task payloads; existing
      whole-surface proxy and async borrow constraints are preserved.
- [x] Free std task status/start/wait/cancellation, array combinators, worker/
      blocking calls and scope helpers have native/full-VM/pruned-VM parity.
      A real std::async fixture also covers Poll/Select and timeout wrappers.
      VM enum/flagset defaults initialize through the underlying zero value.
      TimeoutOption races boolean readiness and constructs Option in Wio.
- [x] Codec probes include inline/terminal completion, 100 foreign-thread
      races, cancellation/failure, malformed headers/ownership and balanced
      source claims. NativeConsume task storage remains a negative gate.
- [x] Only these four new Windows tests were run for task-input closure: 4/4
      passed in 91.81 seconds total (54.03 native differential/live-host,
      37.02 real std differential). Full/pruned bytecode remains identical.
      No old suite or local Linux qualification is claimed. VM artifact
      packaging and backend cost/optimization work remain open.

## Runtime-only VM and embedded executable packaging

- [x] Extracted the bytecode model/codec/verifier/disassembler into the
      compiler-independent `wio_bytecode_core` archive. WIR conversion remains
      in the compiler; runtime targets do not transitively link it. CMake checks
      target dependency closure, including aliases and static LINK_ONLY edges.
- [x] Added standalone `wio-vm` raw-module execution and verified bytecode
      embedding into an executable. Embedded options are application arguments;
      exit codes and async entry completion preserve VM runner semantics.
- [x] Native producers are deployed beside the executable and located relative
      to it. Unicode executable/library names and argument paths use UTF-8 on
      Windows; fixed CurrentExecutablePath's ANSI-code-page conversion.
- [x] Bounded versioned footer/metadata, corruption checksum, strict UTF-8
      basenames, bytecode verification and executable entry validation reject
      malformed or library-only inputs before publication/execution.
- [x] Existing outputs and conflicting libraries are not overwritten. Completed
      executables publish exclusively; cleanup does not remove existing files
      or required libraries after successful publication. Deployment-directory
      locks serialize cooperating packagers and qualify interrupted-lock refusal.
- [x] Added the `WioVmRuntime` install component for runner/core/VM archives and
      public wire/runtime headers, and included the runner in CLI distribution
      builds. Rebuilt the self-hosted CLI companion.
- [x] Only the two new Windows gates were run: 2/2 passed in 6.55 seconds on the
      final run (0.13 codec/guards; 6.14 installed pure/native/async deployment).
      They cover no-compiler installation, source/bytecode-independent execution,
      relocatable native libraries, Unicode names/arguments, missing libraries,
      entry requirements, corrupt metadata and publication collisions. Added
      the gates to Windows/Linux CI; no local Linux qualification is claimed.
- [ ] Project-command packaging integration, SDK-backed shared/static Wio module
      artifacts and final backend cost/optimization work remain open. Installing
      a runtime archive is not equivalent to shipping a Wio `.lib` module.

## VM project executable/deployment integration (2026-10-14)

- [x] Project build/run/package select the VM backend through the Wio CLI.
      The manifest exposes `vm.executableOutput`, and describe reports both the
      deployed image and adjacent standalone runner. Native and VM executable
      paths remain separate; source/toolchain/native-output collisions reject.
- [x] Compilation and packaging have separate incremental inputs. Runner and
      explicit registry-library changes invalidate packaging without an unrelated
      producer rebuild. Missing deployed libraries can be repaired from current
      intermediates; no-build explicitly rejects incomplete deployments.
- [x] Guarded replacement verifies the existing embedded image and versions native
      filenames by content. Publication preserves old generation files, rejects
      unrelated/corrupt outputs and does not publish on a failed compiler build.
- [x] Artifact deployment reads embedded bytecode and copies only recorded active
      libraries. Project no-build run/package does not need source, raw bytecode
      or bridge build intermediates. Existing distributions require explicit
      clean; validation happens first and cleanup cannot target protected source,
      include/link, SDK/toolchain or artifact directory ancestors.
- [x] Project run preserves manifest arguments, explicit argument values, working
      directory and process exits. Help recognition uses the parsed HELP flag,
      not an unrestricted raw-token search; `--help` after `--` or as an `--arg`
      value reaches the application normally.
- [x] The Unicode project gate exposed and fixed UTF-8 path boundaries in the
      compiler driver, module/header lookup, output/profile paths, diagnostics
      and backend invocation. Source paths are explicitly decoded/encoded as
      UTF-8; Windows backend execution and PATH overrides use wide APIs.
- [x] Windows synchronous process Run/Capture no longer route UTF-8 commands
      through an ANSI shell. They preserve standard handles, working-directory
      isolation, quoted/literal arguments, merged output bytes and exit codes.
      Launch and invalid-directory errors retain explicit Result categories;
      shell built-ins/scripts require an explicit shell invocation.
- [x] Only the three new Windows gates were run on the final revision: 3/3 in
      15.69 seconds (0.24 process UTF-8/large capture, 0.20 generation/deploy
      guards, 15.05 project/native/async/Unicode acceptance). Rebuilt the self-hosted
      CLI and added these gates to the Windows/Linux release workflow. No old
      full suite or local Linux/macOS qualification is claimed.
- [ ] VM-backed shared/static Wio module wrappers, complete installed SDK/platform
      acceptance, generation garbage collection and backend performance budgets
      remain open. This closes project executable integration, not all packaging.
