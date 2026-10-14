# Wio Known Issues and Qualification Gaps

Snapshot: **2026-10-12**, implementation baseline **147422f**, product metadata
**0.18.1**. This includes development work after the published 0.18.1 release;
it is not a statement that the installed 0.18.1 package contains those fixes.

This is the consolidated register of **currently known** defects, limitations,
and missing acceptance evidence found in this repository. It cannot establish
that undiscovered bugs do not exist. No compiler suite, benchmark, or installed
toolchain matrix was run for this documentation/branch-maintenance task.
The external editor/site repositories were not audited in this pass.

The inventory was checked against the active backlog, completion records,
native/VM/backend/SDK contracts, source rejection paths, performance audit, and
GitHub issue/PR/release metadata. There were no open GitHub issues or PRs in
`tynes0/wio` at the snapshot. That does **not** mean there are no known bugs.

## How to read this register

- **Defect**: an observed failure or a documented implementation/performance
  problem. Historical measurements are identified explicitly.
- **Limitation**: intentionally unsupported or bounded behavior, not proof that
  a supported program is broken.
- **Contract gap**: an accepted target differs from today's implementation.
- **Qualification gap**: behavior exists, but the required evidence is missing.
- **Deferred feature**: unfinished product work; not automatically a v1 blocker.

Priorities are proposed triage priorities: **P1** correctness, major performance
or accepted release-contract work; **P2** bounded limitations/qualification;
**P3** broader deferred capabilities. No newly confirmed P0 is asserted here.
Register IDs below are documentation tracking IDs, **not compiler error codes**.
The future unified normative diagnostic registry remains a separate task.
The current source-derived [error/status code reference](WIO_ERROR_CODES.md)
is available now; it does not assign IDs to legacy uncoded diagnostics or
implement the future structured diagnostic registry.

## 1. Defects and observed implementation problems

### KI-001 — Inferred-array generic calls produced WIR1422 (closed)

Fixed by the current inferred-container-copy change. The builder now emits
owned Copy/CopyValue materialization instead of a numeric Convert; literal SSA
types stay intact. Type-kind equality alone is no longer numeric compatibility.
Typed/Lowered/bytecode verification rejects forged element identities and
retains strict numeric-conversion rejection. The two new Windows gates passed
2/2 in 15.03 seconds; no old suite or local Linux run is claimed.

The reproducer below is retained as historical evidence, not an open bug.

Observed during the Any identity qualification at the baseline revision. The
generic/native array calls followed by an Any cast fail Typed WIR verification
with `WIR1422: Typed WIR numeric conversion requires one numeric operand and a
numeric result`. Semantic analysis accepts the array call. The exact inference/
contextual-typing cause was the builder's nonnumeric same-kind conversion
fallback. The verifier was not weakened to accept nonnumeric conversion.

The failing forms were:

```wio
fn Box<T>(value: T[]) -> any { return value; }

// Native declaration uses a C++ template and Instantiate(i32[]).
[Native, CppName(vm_any_array_identity::Echo), Instantiate(i32[])]
fn Echo<T>(value: T) -> T;

let inferred = [3, 5, 7];
let generic = Identity(Box(inferred)) fit i32[];
let echoed: any = Echo<i32[]>(inferred);
let restored = Identity(echoed) fit i32[];
```

This excerpt is a reproducer fragment, not a standalone program; `Identity`,
native headers and Entry are supplied by the
[focused fixture](../tests/vm_native_any_array_identity.wio). Replace its
explicit dynamic-array arguments with the forms above to reproduce the failure.
Explicit dynamic `i32[]` arguments passed that fixture. Inferred/direct/nested
arrays boxed directly as Any now work; that separate identity bug is fixed.

The new [inferred-call fixture](../tests/wir_inferred_array_calls.wio) and
[copy verifier](../tests/wir_inferred_array_calls_test.cpp) cover that closure.

### KI-002 — Growing StringBuilder appends repeatedly copy the prefix (P1)

`StringBuilder.AppendLine` in [std/builders.wio](../std/builders.wio) uses
`self.value += value + "\n"`. The
[Mandelbrot audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md) confirmed generated
full-prefix string addition/replacement rather than capacity-preserving append.
Accumulated copying is quadratic for the measured fixed-size-line workload.
The builder currently has no independently optimized backing buffer contract.

The historical full-output Wio runs at the audited revision timed out after
300 seconds, even with `-O2`; those are lower bounds, not exact execution times.
No fresh timing at this snapshot is claimed. ByteWriter/ByteBuffer and binary
file output exist, but switching formats is not a fix for builder growth.

Close with amortized-linear append/reserve/clear/numeric formatting, explicit
finalization costs, self/aliased append correctness and native/VM measurements.

### KI-003 — Non-escaping scalar places incur avoidable runtime machinery (P1)

The audited compute-only Mandelbrot kernel was **35.03x** slower than equivalent
C++ at matching `-O2`, with checksum equivalence. The audited generated kernel
allocated a heap-backed Place for inner-loop `xtemp`; optimized assembly still
contained allocation calls. This is a workload-specific historical result,
not a current universal speed ratio.

The current [C++ backend](../compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp)
still contains general `make_shared<optional<T>>` Place storage, optional
temporaries and CFG dispatch machinery. Scalar stack/SSA promotion, proven
initialized loads and direct control-flow emission remain open. Source
`Option<T>`/nullable semantics must not be confused with compiler-internal
initialization state, or removed to optimize it.

Close against the [checksum-qualified benchmark](../tests/performance/mandelbrot_compute/README.md),
allocation/assembly inspection and separately measured Debug/Release budgets.

### KI-004 — Generated C++ size/build scalability remains excessive (P1)

The historical full Mandelbrot source generated 2,664,592 bytes / 53,278 lines;
even its import-free compute fixture generated 37,446 bytes / 932 lines.
These figures belong to the audit revision, not every current program.

Native-only bridge selection now removes unreachable Wio bodies from the VM's
C++ producer: its synthetic gate recorded a 51.3% source reduction. It does not
finish executable capability pruning, header minimization, PCH, canonical
dependency sharding, native-object caching or build-time/memory budgets.
See [backend item 9](../TODOLIST.md) and [performance requirements](WIO_PERFORMANCE.md).

Close when trivial programs and real workloads have enforced generated-size,
cold/warm/incremental build, binary-size and runtime budgets on supported hosts.

### KI-005 — Older normative-looking docs contradict accepted v1 decisions (P1)

[WIO_V1_FREEZE.md](WIO_V1_FREEZE.md) still describes automatic Option-returning
retrieval and postfix `with` as canonical native attachment. The newer
[surface-consolidation plan](WIO_V1_SURFACE_CONSOLIDATION_PLAN.md) explicitly
requires Result-first standard retrieval and bracket-only attachment before v1.
The freeze document also excludes true OS async I/O, while later backlog slices
record bounded-executor I/O and leave native completion backends open.

Some plans still describe old release states/version examples. Historical
versioned notes should remain history, but current guides must distinguish
implemented behavior, accepted pending migration and deferred capabilities.
Use the later consolidation decision for intent; it is not implemented merely
because it is approved. Close with a versioned contract/docs consistency pass.

## 2. Native, VM, ownership and SDK limitations

| ID | Priority / state | Current boundary and closure requirement |
| --- | --- | --- |
| NI-001 | Closed within qualified native payload boundaries | Owning task inputs, free std task helpers, completion/cancellation, blocking main progress and retained-source shutdown now have focused codec, C++/full-VM/pruned-VM and live-host gates. See [native task-input closure](WIO_VM_NATIVE_BRIDGE.md#owning-native-task-inputs-and-std-helper-parity). Existing unsupported object surfaces and async borrows are not enabled by this closure. |
| NI-002 | P1 backend distribution/platform gap | Shared/static VM artifact production, explicit native dependency bundles, final-image component install and explicit repair targets are implemented. Project CLI build/run/package now covers shared/static modules, optional native hosts and bare archives, including independent packaged-archive consumption. Explicit library sidecar identity and recorded image/native hashes preserve no-build selection; missing bundles repair without relinking. Focused Windows gates cover relocation, Unicode/private DLLs, immutable bundles, manifests and retained generations. Declared dependency discovery/generation-management policy and broader platform artifact/runtime qualification remain open. Full SDK value/async/application/reflection parity belongs to the separate v1 kit gate NI-006, not backend packaging closure. See [artifact contract](WIO_VM_ARTIFACTS.md). Existing C++ backend artifact targets are unchanged. |
| NI-003 | P2 limitation | Async ref/view parameters/results remain forbidden. Native escaping closures may not capture invocation-scoped borrowed Places; copy/retain values instead. No arbitrary suspension-lifetime promise is made. |
| NI-004 | P2 limitation | Exact qualified callback aliases work, but aggregate/descendant callback overlap is rejected before invocation. Use one aggregate borrow and access its children. Rejection is a safety boundary, not a memory-corruption bug. |
| NI-005 | P2 limitation | Box support is bounded to the qualified scalar/string/text/container/Any/defaultable handle families. Arbitrary native POD/fixed-array/component defaults, unsupported method payloads and lifecycle-bearing fields are not automatically materialized. Qualified native POD **default factories** are a separate supported facility. |
| NI-006 | P1 v1 SDK-kit blocker, separate from backend closure | Complete kit parity includes complex/nested containers and Option/Result, concrete generic component/object/interface exports, Any/Box, lifetime-safe host ref/view/callback APIs, complex/nested async payloads and foreign task creation/transfer, application hosting, detailed reflection/accessors and reload/capability coverage. Existing VM scalar/session/task adapters are qualified foundations, not full kit parity or automatic closure of the older SDK boundary. Hosts must collect off-thread final session releases before the owner thread exits; pending retired instances otherwise stay pinned until process termination. Track a product-wide inventory/conformance matrix in the [SDK plan](WIO_SDK_EVOLUTION_PLAN.md). Shared low-level ABI/native-bridge safety remains a backend obligation; missing kit adapters do not block that milestone but still block v1. |
| NI-007 | P2 unsupported-shape fallback | `WioFieldAccessor` still has dynamic getter/setter `SignatureMismatch` fallback paths saying “not yet supported.” Their presence alone does not prove a supported exported field fails. Audit reachable advertised shapes; unsupported public exports should diagnose during analysis instead of surprising hosts at runtime. |
| NI-008 | P2 limitation | Intrusive reference counting does not provide a general tracing cycle collector. Strong cycles need explicit cycle-breaking/weak edges; an OnZeroStrong hook cannot collect a cycle whose strong counts never reach zero. Cycle-aware leak diagnostics remain open. |
| NI-009 | P2 bounded behavior | VM execution segments are serialized while globals are mutable. True parallel VM execution requires proven synchronization/thread-transfer rules; worker scheduling alone is not parallel-safe shared global access. |
| NI-010 | P2 hardening gap | Deep expression lowering remains recursive. A qualified Windows Debug fixture needed a 16 MiB stack reservation; this is not stackless lowering or acceptance of arbitrary nesting. Retain bounded-input/fuzzer and deep-expression hardening. |

Evidence: [native bridge contract](WIO_VM_NATIVE_BRIDGE.md),
[VM runtime](WIO_VM_RUNTIME.md), [SDK header](../sdk/include/wio_sdk.h),
[SDK roadmap](WIO_SDK_EVOLUTION_PLAN.md), [Ref implementation](../runtime/include/ref.h),
and [active backlog](../TODOLIST.md). Older bridge slice sections are historical;
later qualification may supersede their limits.

## 3. Language, std and application contract gaps

| ID | Priority / state | Outstanding work |
| --- | --- | --- |
| ST-001 | P1 accepted migration | Standard checked/fallible APIs still need the Result-first pass. Option remains an explicit user utility; automatic Option-returning retrieval has not yet been migrated across intrinsics/std/async/SDK/CLI. |
| ST-002 | P1 accepted migration | `[Attribute]` is the selected final attachment syntax, but `@...` and postfix attribute `with ...` compatibility still exists. Migrate maintained sources/tools first, then reject obsolete spellings. Keep scoped `using cpp::header(...)` and built-in name aliases. |
| ST-003 | P1 accepted cleanup | Canonical std naming, errors, Try/Result/Raw duplicate surfaces, ownership and bounds/Unicode conventions need a coordinated module-by-module audit. Do not delete distinct nonblocking/probe capabilities by name alone. |
| ST-004 | P2 limitation | Unconstrained `fn Box<T>(value: T) -> any { return value; }` is rejected by semantic analysis. Concrete array boxing is supported; arbitrary generic T-to-Any is not qualified. This is separate from KI-001. |
| ST-005 | P2 language gap | Full lexical/syntax/name-resolution/ownership/evaluation-order specification and remaining generic edge/default/specialization diagnostics are not frozen as one current normative contract. Advanced boolean constraints, associated types and richer compile-time packs are broader planned extensions. |
| ST-006 | P2 application gap | Parallel/worker application stages are not executable without a thread-transfer lowering contract; the current C++ backend rejects them with WCPP1214. Imported-system metadata, typed contexts and scheduler inspection still need closure. Sequential/fixed lifecycle and host-driven execution are not missing. |
| ST-007 | P2 Unicode qualification | Unicode text, transcoding and pinned Unicode 17 normalization exist. Broader case/category tables, locale policy and full upstream conformance coverage remain open. Do not advertise wchar_t width or full locale correctness as current semantics. |
| ST-008 | P2 regex limitation | Conservative regex pattern/input restrictions exist, but an enforceable execution-timeout backend and fuller Unicode-mode contract remain open. The std::regex-based surface is not a general hard-timeout guarantee. |
| ST-009 | P2 std qualification | Time-zone/calendar types, floating-point edge matrices, streaming encoders and broader cross-platform time/random/hash/numeric vectors remain. Implemented algorithms are not missing simply because more vectors are needed. |

Evidence: [accepted consolidation](WIO_V1_SURFACE_CONSOLIDATION_PLAN.md),
[language plan](WIO_LANGUAGE_EVOLUTION_PLAN.md), [async plan](WIO_ASYNC_EVOLUTION_PLAN.md),
and the language/std sections of [TODOLIST.md](../TODOLIST.md).

## 4. Tooling and release acceptance gaps

| ID | Priority / state | Outstanding work |
| --- | --- | --- |
| TO-001 | P1 tooling gap | Compiler-owned language service, semantic completion/navigation/tokens and workspace/project understanding are not release-qualified as the shared IDE authority. A lightweight editor source index is not equivalent. |
| TO-002 | P1 ecosystem gap | Rider-first/CLion shared plugin and Visual Studio integration still need version-aligned packaged installation, daily build/run/check/test flows and compatibility gates. Other repositories were not inspected for fresh implementation status in this pass. |
| TO-003 | P1 tooling gap | Canonical syntax-aware formatter and unified structured diagnostics/reference/JSON catalog are not closed. Stable codes already exist in WIR/bytecode/VM; not every legacy logger error/warning has one. Full debugger UI, advanced refactors and package-aware IDE intelligence are separate later work. |
| TO-004 | P2 CLI qualification | Exit codes, quiet/verbose/color behavior, progress/cancellation, config precedence, machine output and help/error rendering need a consistent all-command matrix. Self-hosted CLI migration itself is complete. |
| TO-005 | P2 build gap | Correct content-addressed/incremental/parallel reuse, deterministic invalidation, offline/pinned bootstrap and reproducible artifacts are not completely qualified. Existing project reuse does not imply these are all implemented. |
| TO-006 | P1 release gap | Windows/Ubuntu gates exist; macOS, arm64, compiler/ABI/static/shared/PIC matrices, upgrade/uninstall/non-admin/Unicode-space paths and installed GUI/SDK flows need broader evidence. |
| TO-007 | P1 acceptance gap | Significant real console/GUI/library/SDK/async/network/package applications must become maintained release gates, not demo-only evidence. Product versions and references across compiler/std/SDK/editors/site must remain aligned. |
| TO-008 | P2 release/security gap | Signed artifacts/metadata, provenance/SBOM, dependency/security policy, clean rollback/channel automation and compatibility migration gates are not completed as one release process. Native code is privileged, not a sandbox for untrusted plugins. |

Evidence: [release gates](WIO_V1_RELEASE_PLAN.md),
[editor architecture](WIO_EDITOR_ECOSYSTEM_PLAN.md),
[workflow](../.github/workflows/v1-validation.yml) and [backlog](../TODOLIST.md).

## 5. Missing evidence, not automatically failing behavior

- **QA-001 (P1):** No full current native/C++/VM/SDK/std corpus run is claimed.
  The project library CLI slice ran only two new Windows Debug gates:
  `wio_vm_project_library_cli` and `wio_vm_project_record_policy`, 2/2 in 91.65
  seconds on 2026-10-15. Coverage includes shared/static hosts, bare libraries,
  independent static-archive consumption, relocated packages, no-build without
  source/bytecode, missing-file repair, hash/path rejection and executable-sidecar
  compatibility. This is not fresh full-corpus or Linux/macOS qualification.
  The final-image install/repair slice adds only the new Windows Debug gate
  `wio_vm_sdk_native_install`: invalid API configurations, image-preserving repair,
  component manifests, Unicode relocation, collision rejection and old/new native
  generations after a real private-library rebuild. It passed 1/1 in 52.14 seconds
  on 2026-10-15. Earlier gates were not rerun.
  `DESTDIR`, Linux/macOS and concurrent cross-generation loading in one process
  are not qualified by this gate; macOS bundle/framework images reject explicitly.
  The declared native dependency slice ran only three new Windows Debug gates:
  `wio_vm_module_native_paths`, `wio_vm_sdk_native_dependencies` and
  `wio_vm_native_bundle_policy`, 3/3 in 30.64 seconds. The bundle policy gate
  passed again after adding an escaping symlink-parent/nonexistent-child case.
  This qualifies explicit dependency deployment and registration rollback, not
  automatic discovery/install, full SDK parity or the Linux/macOS branches.
  The owning SDK task-input slice on 2026-10-14 ran only two new Windows Debug
  gates: `wio_vm_task_source_import` and `wio_vm_sdk_task_inputs_artifacts`,
  2/2 in 23.17 seconds. This covers 100 import/lifetime repetitions, installed
  relocated shared/static hosts, canonical/legacy generic ABI checks and raw
  input rejection, not fresh qualification of the earlier corpus or platforms.
  The async session slice on 2026-10-14 ran only the two new Windows Debug
  gates `wio_vm_task_source_shutdown` and `wio_vm_sdk_async_artifacts`, 2/2 in
  36.67 seconds.
  Installed shared/static async hosting and 80 worker/main/timer shutdown
  repetitions are narrow evidence, not a full-suite/platform pass.
  The session/lifecycle slice on 2026-10-14 ran only two new Windows Debug gates:
  `wio_vm_sdk_session_lifecycle` and `wio_vm_sdk_session_artifacts`, 2/2 in
  28.28 seconds. Earlier focused gates were not rerun in this pass.
  The VM SDK module adapter on 2026-10-14 ran only two new Windows Debug gates:
  `wio_vm_sdk_module_adapter` and `wio_vm_sdk_module_artifacts`, 2/2 in 40.89
  seconds. The installed-artifact gate builds and executes shared, two static
  and malformed modules against the relocated runtime-only CMake package.
  The project-artifact closure on 2026-10-14 ran only three new Windows gates:
  `wio_process_utf8_project`, `wio_vm_artifact_generation` and
  `wio_vm_project_artifact`, 3/3 in 15.69 seconds. Earlier narrow gates are
  evidence for their own revision/scope, not a fresh full-suite pass.
- **QA-002 (P1):** The unpushed development baseline had no GitHub Actions runs
  on its performance branch. The current workflow auto-runs on main,
  `releases/**` and PRs, not an arbitrary feature-branch push. Pushing alone is
  not CI qualification or permission to merge/release.
- **QA-003 (P2):** Recent native slices do not claim local Linux/macOS tests,
  broad cross-toolchain ABI conformance, or a newly green full-platform suite.
- **QA-004 (P2):** ASan/UBSan and fuzzing targets exist; TSan where applicable,
  broader leak/unload/cancellation stress and long-running soak evidence remain.
- **QA-005 (P1):** Current integrated compiler/runtime/std/VM performance
  baselines and agreed regression budgets are not complete. Historical audit
  timings must be rerun after fixes, not silently treated as fresh results.

## 6. Deferred capabilities: unfinished, not confirmed bugs

The backlog also contains package/dependency management and official native
packages; richer binding import for macros/unions/bitfields/ownership metadata;
streaming JSON/binary codecs, RFC 6902 Patch/schema/derivation; TOML/archive/
database/decimal modules; TLS/HTTP/WebSocket/proxy/certificate facilities;
OS dialogs/clipboard/notifications/signals/native watcher backends; native
completion-port I/O; async streams/generators; payload enums and richer matching;
extension properties/constraints/import-scoped conflict tools; richer compile-
time packs; REPL/lint/API-doc generation/refactors; broader package/site search;
and JIT/tiering. These are not all current correctness failures or v1 blockers.
Routing belongs to [post-v1 roadmap](WIO_POST_V1_ROADMAP.md) and the
[finite release plan](WIO_V1_RELEASE_PLAN.md), not this issue register.

## 7. Recently fixed: do not report as still broken

- Inferred/direct/nested literal-array Any native identity ambiguity: fixed
  by **147422f** without changing the intrusive Any cell or ABI format. Declared
  fixed array identities remain distinct. KI-001 is the separate generic-call
  failure, not this fixed boxing problem.
- Native-only producer emitted unrelated Wio bodies: **c4cc888** adds verified
  dependency selection while preserving the full canonical bytecode. KI-004
  tracks remaining whole-product cost work, not absence of pruning.
- Consumed ownership/canonical alias and returned-callable/lifetime contracts:
  qualified by **d287da4** and **2633447** within their documented boundaries.
  Do not describe already-qualified callback/task-result transport as missing.
- Historical self/deref, char/NUL, interpolation, project includes/entry and
  released WIR unary fixes are not reopened merely because old conversations
  mentioned them. Record a fresh reproducer if they regress.

## 8. Triage and maintenance

Suggested execution order: VM artifacts;
SDK and platform/installed-toolchain acceptance; backend cost work; Rider/tooling
and remaining v1 contracts; final integrated optimization before beta.
Fix catastrophic KI-002/KI-003 costs earlier when needed; the final optimization
stage is not an excuse to postpone known severe regressions.

When adding an issue, include revision/platform, minimal reproducer, expected
and actual behavior, diagnostic code, evidence/test, scope, workaround and
closure gate. Label unknown root causes as unknown. When closing it, record the
commit and test scope; move historical evidence to completion notes without
pretending a focused pass is a full suite. Keep this register discoverable from
the backlog and docs index. It must not replace or renumber the backend/v1 plan.
