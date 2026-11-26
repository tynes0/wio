# Direct construction into proven-empty string cells

N03 formatting/runtime slice, 2026-10-17. This is not N03 closure, a WIR
ownership rewrite, a public/native ABI change or the start of N04.

## Transformation and proof

`WirCppStringInitPlan` removes staging between a fresh owned builtin string
producer and its sole `PlaceInit` into a closed local `InlinePlace<string>`.
The original producer expression is evaluated at its original instruction:

```cpp
cell.initializeFrom([&]() -> std::string { return originalExpression; });
```

The source optional-string slot and the later `LocalPlace` reset/`PlaceInit`
move disappear. The cell is predeclared empty even in a linear function. A
placement-new factory preserves C++ prvalue elision; forwarding `factory()`
through `construct_at` would introduce another move. Ordinary `write(T)` and
all generic `InlinePlace<T>` behaviors are unchanged. This internal factory is
constrained to exact by-value `std::string` results, not references or conversions.

The existing storage proof must first establish a closed nonescaping local
string cell. Native call-scoped borrow adapters remain valid; shared/escaping
references retain Place. Source loads/copies/moves, borrowed values, Text,
metadata uses, extra uses/cleanup, unknown effects and async/coroutine frames
are excluded. Only owned builtin-string cleanup may intervene, and remains in
its original order. Native/component/object resource cleanup is a barrier.

The important additional proof is **empty at the producer**, not merely empty
after the later LocalPlace reset. A monotone may-initialized CFG analysis joins
predecessors by OR, including loop backedges and backedges to the entry. Writes
set a bit; LocalPlace, DropPlace, ReleasePlace and taking a cell clear it. One
possibly-live incoming path rejects the transformation. Unreachable producers
are excluded. Function-entry local placeholders start empty. Last-event set/
clear masks and candidate prefixes share a bounded worklist; no candidate-by-
instruction product scan is needed. Budgets are 64 cells/candidates, 512 blocks
and a 32-instruction producer-to-init window. Exceeding a budget preserves the
original path; these are not language limits.

The factory checks an already-live cell before invoking user/native code. It
marks storage initialized only after successful construction. A throwing
factory leaves an empty, safely destructible/retryable cell. The compiler-only
contract forbids a factory from observing/reentering the destination; closed-
cell and same-block effect proofs establish that contract at emitted sites.
Formatting is not deferred across hooks, snapshots remain independent, and
ordinary destination cleanup remains in place. No WIR/bytecode opcodes or VM
storage instructions change.

## Measurement and validation

The fresh baseline reuses the previous pass's frozen binaries, with a new full
session before source edits. Its copied build provenance still names the
original `n03-transfer-after` directory; it is not a new rebuild. Candidate
artifacts are rebuilt from the five identical frozen workload inputs, at
10001 operations, with GCC 15.2 C++20 O2 and FP contraction off. Wio-written
`wio-bench` controls every sample, including A/A controls and a separate final
native/VM phase. Python only prepares builds and checksum oracles. Heavy work
is serial, with no trimming, retry-to-pass, priority/power/security changes or
unrelated process kills.

The Release VM host is unchanged, SHA256
`d5897fa48bbf346c5b3c2f5f979dcabf66e589534aed6cab5d92cb7b9b035879`.
The Wio controller is also unchanged, SHA256
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`.
Existing dirty std/playground files were preserved. Declared artifact hashes
are evidence, not a clean full-transitive-source qualification. The original
runtime header's parent Git blob is retained with the prologue evidence; the
candidate header is additionally declared by the updated manifest builder.

Comparison medians, **fresh before -> after**, milliseconds:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 0.7707 -> 0.6586 | 53.2055 -> 52.9304 | 0.4312 -> 0.4470 | 1.0708 -> 1.2208 |
| Ordered hooks + builtins | 2.1480 -> 2.1031 | 74.3426 -> 74.0140 | 1.3763 -> 1.3638 | 2.8237 -> 2.8700 |

Separate final repeated native/VM phase, milliseconds:

| Case | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| Single hook | 0.7730 -> 0.6762 | 52.5261 -> 52.9972 |
| Ordered hooks | 2.1295 -> 2.0893 | 74.1420 -> 74.5937 |

Both sessions retain 84/84 correct outputs, **168/168 together**, applied
affinity, unchanged endpoint declared artifacts and unchanged endpoint power
profiles. Both remain globally **timingQualified=false**. Candidate native
comparison ranges are 0.100/0.048 and A/A differences 0.022/0.008. VM single
comparison range/drift is 0.272/0.125; C# ordered comparison range/drift is
0.683/0.230. Keep every sample, including these noisy controls. Exit 1 from
the benchmark runner rejects timing qualification, not workload correctness.
No accepted percentage speedup, C++ performance-budget closure or VM speedup
is claimed. The native medians are lower in both phases, but remain above C++.

Historical dated observations are separate, not interchangeable denominators:
the preceding transfer pass observed native comparison 0.7787/2.1290 ms and
final 0.8260/2.1240. The current fresh baseline with those same binaries observes
0.7707/2.1480 and final 0.7730/2.1295. Earlier pre-string-transport observations
were 2.3763/5.2681; none supplies qualified universal overhead percentages.

## Memory and emitted code

The exact frozen Work function loses two optional-string declarations
(7 -> 5), gains two direct initializations, and shrinks from 10128 -> 9864
source characters. This does not remove all remaining string temporaries.
Its actual Windows GCC O2 prologue reservation falls **696 -> 584 bytes**,
with the same eight saved general registers. The 112-byte reduction includes
spill/layout/call-space choices, not all caller/callee frames or whole-program
peak stack. See [actual prologue excerpts and hashes](evidence/n03-init-stack-prologues-2026-10-17.json).

Heap diagnostics are unchanged:

| Case | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 2 -> 2 | 64 -> 64 | 64 -> 64 | 0 / 0 |
| Ordered hooks | 10003 -> 10003 | 315097 -> 315097 | 97 -> 97 | 0 / 0 |

This is a diagnostic global-new/delete window, not all malloc/custom allocator
traffic. Native comparison process peak RSS medians rise **9212/9212 ->
9224/9228 KiB**; no RSS reduction is claimed. Candidate VM/C++/C# RSS medians
are 21320/5204/28548 KiB for single and 21324/5200/35424 for ordered. Managed
current-thread bytes and process resident peaks are separate accounting scopes.

| Artifact bytes | Before | After |
| --- | ---: | ---: |
| Generated C++ | 3264419 | 3262483 |
| Unstripped Windows native executable | 6398093 | 6397581 |

The source-size change includes four bytes of output-folder provenance; it is
not all removed instructions. Executable size falls 512 bytes. Neither metric
is a heap reduction or timing claim.

## Gates and retained evidence

Final relevant gates pass **4/4 in 88.89 seconds**: the new plan/emitter test,
new runtime initialization test, directly affected edge-transfer test and
expanded Wio integration test. The latter covers native O0/O2 and VM snapshots,
NUL/long/Text strings, ordered hooks, repeated locals, continue/early return,
native returns and failures both at an edge and inside a local initializer.
Plan checks cover cleared/non-cleared loops, mixed/all-cleared predecessor
joins, unreachable producers, extra/metadata/cleanup uses, type/ownership/
effect/escape exclusions and proof-budget fallbacks. Runtime checks cover
exact factory constraints, empty/SSO/long/NUL values, live-cell rejection
before factory invocation, moves/take and throwing-factory retry.

The new helper, shared classifier and runtime test also pass strict GCC C++20
syntax checking. Initial focused gates passed before a proof-indexing cleanup
and the additional native-initializer failure fixture; both XML reports remain.
No old full corpus ran. The previous transfer commit's three-platform CI passed;
this change requires its own platform CI, not an inherited success claim.

- Before: [all samples](evidence/n03-init-before-2026-10-17.json),
  [copied build provenance](evidence/n03-init-build-before-2026-10-17.json),
  [heap](evidence/n03-init-memory-before-2026-10-17.json).
- After: [all samples](evidence/n03-init-after-2026-10-17.json),
  [build](evidence/n03-init-build-after-2026-10-17.json),
  [heap](evidence/n03-init-memory-after-2026-10-17.json).
- Validation: [final CTest](evidence/n03-init-tests-2026-10-17.xml),
  [initial CTest](evidence/n03-init-initial-tests-2026-10-17.xml),
  [Wio/native/VM process results](evidence/n03-init-validation-2026-10-17.json),
  [strict GCC syntax](evidence/n03-init-gcc-syntax-2026-10-17.json).

N03 remains open. Next cost attribution includes separately materialized
primitive parts, live-locale formatting work and richer/legacy formatting;
KI-008 hooks also remain. Do not defer user formatting across observable hooks
or revive the VM-expanding streaming experiment without a valid cost model.
N04 has not started; N05 proof sharing does not close N05, and KI-007/KI-009
and N01/N14 acceptance remain explicit.
