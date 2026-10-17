# N03: native string borrow transport

2026-10-17. This follows the scalar-hook correctness change, whose higher native
times were explicitly not accepted as an optimization. N03 is still in progress;
this does not close the native runtime budget or start the heavy VM pass.

## Production change

A local owning string previously needed a shared heap cell even when its only
reference use was a synchronous native `view string`/`ref string` call. The C++
storage proof now permits existing `InlinePlace<std::string>` storage for a
bounded native leaf ABI:

- External, synchronous, non-method/non-extension free function, no receiver.
- Only primitive value arguments and nonnullable UTF-8 string borrows.
- A scalar/void value result; no returned borrow, object, callback or resource.
- No retained/foreign-thread transport; signatures must match the operands.
- Every other cell use must independently pass the closed-use proof. Reborrows,
  captures, CFG-edge transport and unknown calls keep the general representation.
- Async/coroutine callers retain their existing frame representation.

For these leaves the emitter adds a private generated `_borrow` entry taking
ordinary C++ string references. A proven inline caller invokes it directly,
without constructing the general proxy/callback-capable `Place` transport.
The ordinary function identity and `Place` signature remain available for
function values, exports, callbacks and unproven callers. Both entries preserve
the existing native exception boundary. There is no bytecode, SDK or public Wio
API change, no raw reference returned to Wio, and no benchmark-name special case.

This is an allocation/transport proof, not a claim that arbitrary native code is
pure. Native code must continue to respect the declared call-scoped borrow.
It may mutate a mutable string, grow it or throw. Owning snapshots remain copies.

## Rejected shared-WIR experiment

An earlier experiment streamed each hook, builtin value and literal into one WIR
buffer, exposing numeric append fusion. It passed semantic gates but added VM
work. It is **not** in the final implementation: the original shared formatting
call lowering is restored byte-for-byte. No target-dependent language semantics
or hidden VM formatter callbacks were introduced to hide the cost.

The experimental comparison native medians were 2.1218/4.5709 ms; VM medians were
62.7486/96.2332 ms (single/ordered hook cases). Its separate final VM medians were
61.1155/94.6717 ms. These observations, including the VM regression, remain in
evidence. An initial redundant drop after a consuming move failed WIR1470; the
failure is also retained rather than weakening ownership verification.

## Validation and measurement protocol

Only the three new relevant gates run locally: formatting-call IR, native string
borrow storage proof, and Wio native O0/O2 plus VM semantics. The Wio fixtures
exercise embedded NUL, string growth/mutation, owning snapshots, scalar arguments,
native function values (native backend only), Unicode hook output, and translated
native exceptions.
Negative IR guards cover callback/retained/foreign-thread/nullable/unknown ABI,
returned borrows and escaping cells. The object-container WVM1062 boundary stays
an explicit negative test, not a parity claim. The extra indirect-native test
exposed the unchanged VM frame-entry WVM1003 boundary: direct borrows succeed,
but native function values are not VM parity. Its failed discovery is retained
and tracked as [KI-009](WIO_KNOWN_ISSUES.md#ki-009--vm-indirect-native-function-values-bypass-native-dispatch-p1).
Cross-platform CI is separate.

The before measurement reuses frozen production binaries from the preceding
scalar-hook change, not a rebuilt clean release. Its generated C++ SHA256 is
`7fa375cd71cd31cdffeebd86d9329777cec54e6bf5ab24508c8fea51a00145f0`.
Copied build logs retain their original paths; `--reuse-build` only creates the
manifest and collects allocation diagnostics. Final builds use byte-identical
Wio/C++/C#/header/project workload inputs at 10,001 iterations. The Wio-written
controller is unchanged, SHA256
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`.

Both comparison and a distinct repeated final native/VM phase retain all warmups,
A/A controls, reversed/rotating rounds and raw samples. No retries, trimming,
power/priority changes or unrelated process kills are used. C++/native use O2;
C# uses .NET 9 Release. VM uses the unchanged Release host. Python prepares builds
and independent output fingerprints; it does not control timing samples.

Allocation diagnostics are a separate global-new/delete window, not production
timing, RSS, all malloc traffic or a VM heap counter. Existing user std/playground
changes are preserved. Declared input hashes are retained; this is not a clean
full-transitive-source or qualified performance-budget claim.

## Results and remaining gap

Comparison medians, milliseconds, **frozen before -> final candidate**:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 2.3763 -> 1.5767 | 55.5157 -> 56.3759 | 0.4361 -> 0.4407 | 1.0871 -> 1.1557 |
| Ordered hooks + builtins | 5.2681 -> 4.0452 | 78.2178 -> 78.3105 | 1.3793 -> 1.3752 | 2.3833 -> 2.8943 |

Separate final native/VM phase, milliseconds:

| Case | Native before -> final | VM before -> final |
| --- | ---: | ---: |
| Single hook | 2.3287 -> 1.6029 | 57.0303 -> 55.3751 |
| Ordered hooks | 5.3404 -> 3.9319 | 80.3785 -> 77.8506 |

Both full sessions have **84/84 correct outputs**, applied affinity, unchanged
declared endpoint artifacts and unchanged endpoint power profile. They remain
globally **timingQualified=false**. In the final candidate the single-hook native
comparison range is 1.22 and its final-phase range is 0.30; ordered native final
range is 0.16. All samples remain, including those high observations. No timing
rerun was substituted to force qualification. The rejected implementation has
its own separately named session; changing that implementation is not a retry of
the final candidate. Lower native medians in both phases are observations, not
an accepted speedup percentage or a closed 5-10% performance budget.

The earlier historical hook-disabled session measured native 1.9466/4.0860 ms;
the previous shared-hook session measured 2.1221/4.6920 ms. Those are different,
dated sessions, not interchangeable denominators for the fresh frozen-binary
before measurement. The present native observations recover the preceding
regression, but the gap versus direct C++ and C# is still visible. VM has no
accepted performance gain from this native-only transport pass.

Separate allocation diagnostics (same workload and checksums):

| Case | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 10003 -> 2 | 560120 -> 64 | 176 -> 64 | 0 / 0 |
| Ordered hooks | 25005 -> 15004 | 1175212 -> 615156 | 237 -> 156 | 0 / 0 |

This removes one shared cell per iteration in each case. It does **not** remove
all output-buffer allocations in the longer case. Current comparison root-process
peak RSS medians (KiB), native/VM/C++/C#: 10296/22188/5892/29784 and
10292/22168/5892/36700. Before native RSS was 10324/10344 KiB; small RSS changes
are not interchangeable with the diagnostic heap savings. C# current-thread
managed allocations remain 1589768/3940040 bytes, a different accounting scope.

The helper entries have a code-size tradeoff: generated C++ grows from 3299217
to 3305077 bytes and the unstripped native executable from 6557448 to 6565203
bytes. This is not a binary-size optimization. The final generated C++ SHA256 is
`a979c38b00006f8b2c9b5c926860e6cda61da81e0dc13e7b9a44f3fdc589310c`.

Next N03 cost work must separate integer locale/fallback formatting and the
remaining object receiver/owned-string/merge transport. The generated kernel
still contains those representations; this is not proof of their individual
cycle costs. Do not revive the rejected shared-WIR expansion without a measured
VM cost model. Keep KI-008, KI-009 and N01/N14 qualification open; N04 has not begun.

## Retained evidence

- Before: [session](evidence/n03-borrow-before-2026-10-17.json),
  [original build commands](evidence/n03-borrow-build-before-2026-10-17.json),
  [allocation window](evidence/n03-borrow-memory-before-2026-10-17.json).
- Final: [session](evidence/n03-borrow-after-2026-10-17.json),
  [build commands](evidence/n03-borrow-build-after-2026-10-17.json),
  [allocation window](evidence/n03-borrow-memory-after-2026-10-17.json).
- Rejected streaming implementation: [session](evidence/n03-borrow-rejected-stream-2026-10-17.json),
  [build](evidence/n03-borrow-rejected-stream-build-2026-10-17.json),
  [memory](evidence/n03-borrow-rejected-stream-memory-2026-10-17.json),
  [initial WIR1470 failure](evidence/n03-borrow-initial-ownership-failure-2026-10-17.xml).
- Final local 3/3 tests in 65.44 seconds:
  [CTest](evidence/n03-borrow-tests-2026-10-17.xml),
  [all Wio build/run processes](evidence/n03-borrow-validation-2026-10-17.json).
  The additional indirect VM discovery is retained separately:
  [failed CTest](evidence/n03-borrow-indirect-discovery-2026-10-17.xml),
  [failed process](evidence/n03-borrow-indirect-discovery-2026-10-17.json).

Subsequent work: [bounded scalar fields and pre-sized rendered interpolation](WIO_SCALAR_FIELD_OPTIMIZATION.md)
records two further production optimizations with distinct before/intermediate/
final observations. The historical measurements above are not replaced.
