# N03: shared scalar user ToString calls

2026-10-17. This is a bounded KI-008 correctness slice within N03, not completion
of N03, the VM optimization pass, or the N14 performance target.

## What is now shared

Synchronous scalar object/interface interpolation and concrete generic
`std::ToString` calls now materialize public zero-argument string-returning
ToString as ordinary WIR virtual/interface dispatch. View receivers and temporary
objects use the same path. There is no arbitrary Wio callback hidden inside the
VM's low-level builtin ValueFormatter, and no new bytecode opcode or SDK ABI.

`FormattingCallLowerer` runs after generic specialization and before canonical
control-flow lowering. All source operand expressions have already been
evaluated. Eligible formatting operands are then rendered left-to-right; builtin
operands in a mixed expression are materialized in that same order, so native
numeric facets cannot move past a user hook. Final interpolation consumes strings.
Null/call/merge blocks preserve the existing runtime empty-handle spelling and
invoke a nonempty receiver's hook exactly once. This does **not** make source
`Object` nullable or add `Object?` interpolation support.

The pass preserves source spans and existing receiver-root cleanup. Its new
string temporaries have explicit cleanup after final interpolation, with branch
results transferred through owned block parameters. It uses stable fresh ids,
does not create wrapper functions or export identities, indexes function ids once,
and is idempotent. Normal verifier, hierarchy and ownership/storage lowering still
apply to the resulting calls and CFG.

Implicitly public interface methods previously carried private WIR visibility
when no access keyword was written. The type builder now preserves the interface
public default, including reflection metadata, while keeping explicitly private
or protected members unchanged. The default for object members is unchanged.

## Explicit bounds

- Object-containing arrays/dictionaries remain on the old path; mixed expressions
  with an unsupported surface are not partially reordered. VM still reports
  WVM1062 for the existing object-array fixture.
- Async functions/coroutine frames are not rewritten. Async ToString, native hook
  declarations, still-open generic hook declarations and ambiguous/private hooks
  are not selected. A generic caller specialized to a normal object is supported;
  this is not a claim that generic-owner hook specialization is complete.
- This does not introduce a richer formatting/precision API, nullable/container
  hook traversal, arbitrary callbacks inside a builtin formatter, or new native
  interop borrowing rules. KI-007 is unchanged.

## New-only validation

The two new tests passed locally in 63.76 seconds. The Wio gate emits and inspects
actual Lowered WIR, compiles native O0/O2, and runs VM bytecode with its bridge.
It covers effectful expression evaluation before formatting, repeated/ordered
stateful calls, NUL, temporary objects, generic interpolation/std::ToString,
view receivers and an implicitly public interface. The remaining user-container
WVM1062 is an explicit negative boundary, not feature parity.

The IR unit verifies the input/output modules, null/call/merge shape, typed calls,
temporary cleanup, idempotence and conservative guards. Initial development
failures are retained: a synthetic unit lacked required module/callable identity,
and the interface gate exposed the private visibility metadata described above.
Those are not waived by narrowing the final assertions. No old functional corpus
was run locally. Three-platform CI runs the new gates alongside the recent N03
gates; the preceding portability CI 38994486788 passed all three platforms.

See [the Wio fixtures and reproduction](../tests/performance/formatting_calls/README.md).

Evidence: [two tests](evidence/n03-hook-tests-2026-10-17.xml),
[Wio processes](evidence/n03-hook-validation-2026-10-17.json),
[initial development test failure](evidence/n03-hook-initial-test-failure-2026-10-17.xml).

## Controlled measurement baseline

Related Wio, C++ and C# workloads were prepared before enabling the production
pass for collection. The baseline compiler is a controlled build with the new
FormattingCallLowerer invocation disabled, frozen as
`build/n03-hook-disabled-tools-2026-10-17/wio.exe` (SHA256
`2935258fa76557b5ea14587f4769b202f2be514284299736ebe79e86ad2cdbdb`).
This is not described as an untouched released binary. The unused pass source
exists in the static library, but it is not called and its CFG does not appear
in baseline generated code. The invocation is restored for the after build.
Frozen workload files/inputs and the unchanged Wio controller are shared.

Relocating the baseline compiler initially omitted `wio-selfhost`, so --version
failed **before any timing sample**. That separate
[preflight failure](evidence/n03-hook-preflight-failure-2026-10-17.json) is retained.
The companion was copied, and the shared preparation harness now hashes an
existing companion as a declared input. Existing binaries were not rebuilt or
retimed to select favorable samples; the one actual baseline collection is
[retained in full](evidence/n03-hook-before-2026-10-17.json).
Its 64/84 process outputs are correct, with 20 unsupported VM WVM1062 failures.
Those missing timings are not zero-cost observations or a VM speedup denominator.
Baseline [build commands](evidence/n03-hook-build-before-2026-10-17.json) and
[allocation diagnostics](evidence/n03-hook-memory-before-2026-10-17.json) are separate.

## Observations, including the native cost

The five Wio/C++/C#/header/project workload files are byte-identical before/after.
The same controller SHA256 remains
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`.
Native/C++ are O2 with FP contraction disabled; C# is .NET 9 Release with tiering
disabled. At 10,001 iterations, the controller uses two internal warmups per
program, one external warmup, five rotating/reversed comparison rounds with
native A/A controls, then a distinct three-round final native/VM phase. Serial
builds complete before sampling. No power/priority/security changes are made.

Comparison medians in milliseconds, **before -> after**:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| One stateful hook | 1.9466 -> 2.1221 | unsupported -> 51.9680 | 0.4464 -> 0.4389 | 1.0031 -> 1.0668 |
| Two ordered hooks + builtins | 4.0860 -> 4.6920 | unsupported -> 73.6743 | 1.3734 -> 1.4132 | 2.5155 -> 2.5000 |

Separate final native/VM medians, milliseconds:

| Case | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| One hook | 1.9668 -> 2.1053 | unsupported -> 52.9432 |
| Ordered hooks | 4.0911 -> 4.6600 | unsupported -> 73.0398 |

The after session has **84/84 correct process outputs**, applied affinity and
unchanged endpoint artifacts/power profile. The before session has 64/84 correct
outputs, with all 20 failed VM processes retained. Both sessions are globally
**timingQualified=false**; after C# single-hook range is 0.30 and ordered-hook
range 0.165. No sample was trimmed/replaced or rerun to obtain qualification.
The old collector Markdown prints empty timing medians as zero; its unsupported
VM rows have no valid timing sample, not zero execution cost.

Native times rose in **both** comparison and final phases. This is a real
observation to investigate, not a speedup or an acceptable native budget result.
The new CFG/materialization exposes work to WIR and supports VM dispatch, but
does not establish a zero-cost native implementation. Full cost attribution
and removal of avoidable managed/merge/string transport remain open under N03
and the fixed N05/N08 passes. Overall unqualified timing prevents accepting a
definitive regression percentage too; it does not justify hiding the higher
observations. No VM speedup factor can be computed from unsupported execution.

Native allocation diagnostics are unchanged:

| Case | Allocations = frees, before -> after | Requested = freed bytes | Peak live bytes | Final live blocks/bytes |
| --- | ---: | ---: | ---: | ---: |
| One hook | 10003 -> 10003 | 560120 -> 560120 | 176 -> 176 | 0 / 0 |
| Ordered hooks | 25005 -> 25005 | 1175212 -> 1175212 | 237 -> 237 | 0 / 0 |

This is a global-new/delete diagnostic window, not all malloc/custom traffic,
copied bytes, VM heap accounting or production timing. Current comparison median
root-process peak RSS (KiB):

| Case | Native | VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| One hook | 10324 | 22212 | 5892 | 29820 |
| Ordered hooks | 10324 | 22184 | 5892 | 36716 |

Native before RSS was 10332/10328 KiB; these small shifts are not isolated heap
savings. C# current-thread managed allocation bytes are 1589768/3940040, a
different metric from native requested bytes or RSS. All declared-input hashes
are retained; this is not a clean full-transitive-source qualification, and the
user's existing dirty std/playground edits remain untouched.

Raw after evidence: [controller session](evidence/n03-hook-after-2026-10-17.json),
[build commands](evidence/n03-hook-build-after-2026-10-17.json),
[allocation diagnostics](evidence/n03-hook-memory-after-2026-10-17.json).

## Final generic-owner guard

Post-measurement review tightened a boundary: a nongeneric method symbol may
still belong to an open generic owner. The pass now explicitly rejects open
owner types (both receiver/callee and containing function), not merely a method's
own generic parameter list. A dedicated IR negative case covers that distinction.
The final two-test recheck passed in 98.96 seconds:
[final tests](evidence/n03-hook-final-tests-2026-10-17.xml),
[final Wio processes](evidence/n03-hook-final-validation-2026-10-17.json).

The measured before/after collection above is the `bae711d` implementation;
this final guard was added afterwards. Re-emitting the same frozen benchmark
source with the final compiler produces byte-identical whole generated C++:
SHA256 `7fa375cd71cd31cdffeebd86d9329777cec54e6bf5ab24508c8fea51a00145f0`,
matching `frozen.cpp`. This proves that the measured native workload code did
not change under the guard, not that every unmeasured generic/VM program has a
performance qualification. Native/bridge/bytecode benchmark artifacts and all
old/new samples remain frozen. No favorable timing rerun was substituted.

## Native transport follow-up

The higher native observations above are the starting point for
[the native string borrow transport pass](WIO_NATIVE_STRING_BORROW_OPTIMIZATION.md).
It removes bounded shared cells/general Place argument transport without changing
this shared call-lowering implementation. A measured streaming-WIR experiment
was rejected for added VM cost; it is not silently described as a production win.
The follow-up retains the before, rejected and final evidence separately.
