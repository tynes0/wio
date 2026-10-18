# Rendered scalar fragments instead of intermediate strings

N03 formatting/runtime slice, 2026-10-18. This is a C++ emission optimization,
not N03 closure, an ABI/type-system change or a new WIR/VM instruction.

## Representation and ordering

Mixed user-hook interpolation intentionally renders operands left-to-right
after source expressions have been evaluated. An integer before a hook cannot
be converted after that hook: native code can change locale or install stateful
numeric facets. `WirCppScalarFormatPlan` preserves that ordering while replacing
private intermediate strings with already-rendered bytes:

- Bool becomes a `std::string_view` of the stable `true`/`false` literals. The
  condition is read at the original producer, not the consumer.
- i16/i32/i64/isize and their unsigned counterparts use a private
  `RenderedInteger` cell. Classic-locale decimal bytes fit its inline storage.
  The live locale is checked at each original producer. Custom locales retain
  the same outlined stream formatter and an owning string fallback, allowing
  arbitrary lengths and embedded NUL. The final interpolation only reads the
  bytes; it never invokes numeric formatting again.

The fragment is noncopyable/nonmovable, predeclared even in a linear function,
and formatted in place. Cold fallback destruction occurs at the original
DropValue/Release through `clear`, with destructor safety on unwind. Formatting
computes the new spelling before clearing the old one. Scratch bytes are not
zero-filled at function entry; only the explicitly written prefix is read.
The runtime unit checks that its size does not exceed optional<string> on the
host. This is not a claim that the integer fragment itself always uses fewer
stack bytes: bool and removed string transport are separate savings.

## Conservative selection

The producer must be a fresh owned builtin-string Interpolate with exactly one
non-reference integer/bool operand, matching actual/signature type, no literal
prefix/suffix, targets or borrow origin. Its only semantic consumer must be one
string-typed operand position of a later Interpolate. Multiple consumers/operand
positions, returns, copies, stores, calls/native transport, phi-edge arguments
or metadata observations preserve the ordinary string. Explicit cleanup is only
accepted after the consumer in that same block; cleanup before the consumer or
on other paths rejects selection. Formatting can precede intervening calls or
CFG blocks because the fragment owns its rendered bytes, not a delayed scalar
reference. Verified WIR dominance/ownership remains a prerequisite to emission.

Async/coroutine frames are excluded. Floats, char/byte, signed/unsigned 8-bit,
enums, Text, containers, nullable/generic/native values and broader formatting
surfaces keep their existing path. In particular, 8-bit formatting has an
existing distinct helper contract and is not silently switched to num_put.
Public/native string identity, ownership, reflection and function signatures
remain unchanged; fragments never cross those boundaries. A cheap no-single-
interpolation check avoids constructing the proof index for unrelated functions.

## Correctness scope

New tests cover representation/use exclusions, metadata/extra use, cleanup order,
cross-block reads and emitted non-string source storage. Runtime checks compare
boundaries and deterministic random values with canonical formatting under
classic, grouped and long custom-facet locales, including NUL, repeated fallback/
inline transitions, clear/reuse and snapshots after locale changes.

The expanded Wio gate compiles native O0/O2 and checks hooks that switch classic/
grouped locale between numeric operands, plus a counting num_put facet proving
one conversion at each original point. NUL, bool, 8-bit fallback and Unicode
Text mixed-hook cases also run in VM. Custom-facet mutation is deliberately a
native-only integration check: an MSVC VM host and GCC native bridge can use
different C++ runtime locale globals. The VM does not promise that those globals
are one object. Its unchanged mixed-hook surface and frozen benchmark workload
remain measured independently; this is not a claim of cross-runtime locale ABI.

## Evidence

Final new/expanded gates pass **3/3 in 145.87 seconds**, including the Wio
O0/O2 + VM processes above. Initial 3/3 checks are also retained, before scratch
initialization was removed. Strict GCC C++20 syntax and a separately compiled
GCC O2 runtime-unit execution pass. An initial test-authoring failure used a
nonexistent TypeTable::i64Type accessor; its diagnostic summary is retained.
The fixture now uses the actual intern API. No old full corpus ran.

The preceding commit's platform CI passed. The present change needs its own
Windows/Ubuntu/macOS qualification; local checks are not that evidence.

## Frozen measurement

The fresh baseline runs the preceding pass's frozen binaries without rebuilding
them. Its copied build provenance still names `n03-init-after-2026-10-17`.
This is not a clean released checkout rebuild. Candidate builds use the five
identical frozen Wio/C++/C# workload files, 10001 operations, GCC 15.2 C++20 O2,
and FP contraction off. The Wio-written `wio-bench` controls all samples, A/A
controls and the final native/VM phase; Python only prepares builds/oracles.
All heavy work is serial, without priority/power/security changes, unrelated
process kills, trimming or retries to obtain favorable timing.

The Release VM host remains SHA256
`d5897fa48bbf346c5b3c2f5f979dcabf66e589534aed6cab5d92cb7b9b035879`;
the controller remains
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`.
Existing dirty std/playground files are untouched. Declared artifact hashes are
retained, not a claim of clean full-transitive-source qualification. There are
no added VM instructions; bytecode provenance paths can differ between folders.

Comparison medians, **fresh before -> after**, milliseconds:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 0.7181 -> 0.6938 | 67.0696 -> 56.3237 | 0.4303 -> 0.4424 | 1.0582 -> 1.3074 |
| Ordered hooks + builtins | 2.1909 -> 1.8798 | 90.6389 -> 79.2460 | 1.4599 -> 1.4070 | 2.7661 -> 2.7581 |

Separate final repeated native/VM phase, milliseconds:

| Case | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| Single hook | 0.8258 -> 0.6788 | 64.1815 -> 56.5762 |
| Ordered hooks | 2.9425 -> 1.9695 | 92.7396 -> 78.7842 |

Both sessions are complete with **84/84 correct outputs each**, 168/168 together,
applied affinity, unchanged endpoint declared artifacts and unchanged endpoint
power profiles. Both remain globally **timingQualified=false**. Candidate native
comparison relative ranges are 0.266/0.138, ordered drift is 0.135, and A/A
differences are 0.006/0.016. C++ single range/drift is 0.560/0.209. Final native
ranges are 0.010/0.123. Preserve those noisy observations; runner exit 1 rejects
timing qualification here, not workload correctness. No accepted percentage
speedup, VM speedup or C++ performance-budget closure is claimed.

The ordered case exercises the new integer/bool fragments; the single-hook
case has no staged scalar to replace. Its timing movement is not attributed to
the targeted conversion optimization. The unchanged VM host's substantial
movement also warns against treating these sessions as a precise causal timing
estimate. C++ uses invariant to_chars: it is an identical-output classic-workload
comparison, not an implementation of Wio's full live-locale/facet contract.

Historical dated observations remain separate: the preceding report observed
native 0.6586/2.1031 ms (final 0.6762/2.0893), while the current fresh reuse of
those binaries observes 0.7181/2.1909 (final 0.8258/2.9425). Earlier pre-string-
transport observations were 2.3763/5.2681. They are not interchangeable paired
denominators or qualified universal slowdown/speedup percentages.

## Memory and code size

Work's optional-string slots fall **5 -> 3**, replaced by one integer fragment
and one bool view; its source body falls 9864 -> 9620 characters. Actual Windows
GCC O2 prologue reservation falls **584 -> 520 bytes**, with the same eight saved
general registers. This 64-byte reduction includes spill/layout/call-space
choices, not all caller/callee frames or whole-program peak stack. See
[actual prologues and artifact hashes](evidence/n03-fragment-stack-prologues-2026-10-18.json).

Heap diagnostics in the frozen classic-locale workload are unchanged:

| Case | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 2 -> 2 | 64 -> 64 | 64 -> 64 | 0 / 0 |
| Ordered hooks | 10003 -> 10003 | 315097 -> 315097 | 97 -> 97 | 0 / 0 |

This is a diagnostic global-new/delete window, not all malloc/custom allocator
traffic. The workload's small decimal index already fits string SSO, so no heap
reduction is expected from that leaf. Long custom-facet output intentionally
retains owning storage. Numeric boundaries are correctness checks, not an
additional four-language large-number performance claim.

Native comparison peak RSS medians are **9228/9228 -> 9212/9216 KiB**. Such small
shifts are not a whole-process memory-win claim. Candidate VM/C++/C# medians are
21320/5204/28612 KiB for single and 21328/5200/35512 for ordered. C# current-thread
managed allocations remain 1589768/3940040 bytes; these and process resident
peaks have different scopes from native allocation counters.

| Artifact bytes | Before | After |
| --- | ---: | ---: |
| Generated C++ | 3262483 | 3262273 |
| Unstripped production native executable | 6397581 | 6392777 |
| Separate allocation-diagnostic executable | 6481153 | 6482045 |

The production executable is 4804 bytes smaller; the diagnostic executable
grows 892 bytes and remains visible rather than being conflated with production.
The source-size difference includes a four-byte increase in folder provenance
and the new helper include; it is not all removed instructions. Neither file
size is a heap or timing measurement.

- Before: [all samples](evidence/n03-fragment-before-2026-10-18.json),
  [copied build provenance](evidence/n03-fragment-build-before-2026-10-18.json),
  [heap](evidence/n03-fragment-memory-before-2026-10-18.json).
- After: [all samples](evidence/n03-fragment-after-2026-10-18.json),
  [build](evidence/n03-fragment-build-after-2026-10-18.json),
  [heap](evidence/n03-fragment-memory-after-2026-10-18.json).
- Validation: [final CTest](evidence/n03-fragment-tests-2026-10-18.xml),
  [final Wio/native/VM processes](evidence/n03-fragment-validation-2026-10-18.json),
  [initial CTest](evidence/n03-fragment-initial-tests-2026-10-18.xml),
  [initial processes](evidence/n03-fragment-initial-validation-2026-10-18.json),
  [fixture diagnostic summary](evidence/n03-fragment-initial-fixture-error-2026-10-18.json),
  [strict GCC syntax/runtime unit](evidence/n03-fragment-gcc-runtime-2026-10-18.json).

N03 stays open for live-locale costs, richer/legacy formatting and KI-008 hooks.
Do not defer conversion across hooks or trade the old VM-expanding streaming
experiment for a native-only microcase win. N04 has not started. Sharing private
storage proof work does not close or renumber N05; KI-007/KI-009 and N01/N14
performance qualification remain explicit.
