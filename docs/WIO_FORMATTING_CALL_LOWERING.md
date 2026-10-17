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
