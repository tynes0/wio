# N03: bounded private receiver entries

Subsequent work: [single-value string edge forwarding](WIO_STRING_TRANSFER_OPTIMIZATION.md).
Historical observations below remain unchanged.

2026-10-17. Follow-up to `8ade2e8`. N03 remains in progress; N04 has not
started. This is not closure of the C++ performance budget or the VM pass.

## Production change

Small object methods previously moved a general `Place<Ref<T>>` into their
implementation and copied that representation for readonly self aliases. Even
after primitive field projection became a pointer, receiver transport still
carried shared cells, callback tokens and optional cached handles.

The new `WirCppReceiverPlan` proves a restricted private entry whose self and
readonly self aliases use the existing one-pointer `ObjectBorrow<T>` instead.
The original function symbol/signature remains as a wrapper, so function values,
exports and ABI consumers still receive the same general Place entry. That
wrapper validates its supplied borrow and keeps its owning cell/token alive
through the private call. No public source, native ABI, symbol identity, WIR or
bytecode representation changes.

Conceptually:

```cpp
// Established ABI remains available.
std::string method(Place<Ref<Counter>> self)
{
    return method_receiver(ObjectBorrow<Counter>{self.Get()});
}

// Private implementation, only after the proof succeeds.
std::string method_receiver(ObjectBorrow<Counter> self)
{
    auto* field = std::addressof(value_base(self).count);
    *field = WrappingAdd(*field, 1ULL);
    const auto count = *field;
    // Formatting only after the final receiver access.
    return format_count(count);
}
```

Vtable and legacy method wrappers call the selected implementation's private
entry, not a statically guessed override. Direct method instructions use it
only for that already-selected eligible callee. Newly formed raw receiver
borrows validate the active callback lifetime before entering; the general ABI
wrapper validates its own supplied Place token instead. Field address formation
retains the established null check. No global borrow/null check is disabled and
no intrusive retain/release ownership contract is weakened.

## Proof boundary

- One synchronous Wio object method, self only, one block, at most 128
  instructions. No external/native/abstract/extension/closure/async/coroutine
  body, capture or generic specialization. More complex cases retain Place.
- Regular Wio object layout with either no base or empty Wio object roots.
  Nonempty/inherited/native/interface base layouts and native proxy hierarchies
  stay on the established entry. A derived override is still dispatched normally.
- Scalar field projections must separately pass the existing bounded field
  interval proof. No root self load/rebind, escaping reference, callback,
  arbitrary call, branch argument or unknown receiver metadata is permitted.
- Only primitive field reads/writes and primitive computation precede the final
  receiver access. String allocation/copy/formatting is a hard reentrancy barrier:
  no subsequent receiver or derived-field access is allowed. Native locale
  facets must not be assumed incapable of calling user code.
- Before the *first* receiver access, only scalar constants and readonly aliases
  are accepted. Even scalar division/conversion may fail: the ABI wrapper's
  initial Get must not move borrow/uninitialized errors before an earlier
  arithmetic failure. A new negative test exposed this initially permissive
  prelude; that failed result is retained and the production predicate tightened.
- Return values are scalar/string/void. No receiver/field reference can escape.
  Behavioral processors retain their original pipeline and general entry.
- Failure to prove any condition keeps the old path; these bounds are not new
  source-language restrictions. The plan does not rewrite the IR.

The change targets transport/stack/code cost, not heap allocations already
removed by previous passes. It does not resolve KI-007, KI-008 or KI-009.

## Second production slice: integer hot/cold separation

The receiver-only observations did not close the gap, especially in the longer
case. A second, separately measured change moves integer stream construction,
formatting and exception state into `detail::AppendIntegerStream`. The usual
decimal append retains its stack `to_chars` buffer and calls the fallback only
when the live global locale is not classic. MSVC marks the fallback noinline;
GCC/Clang additionally mark it cold. Other compilers retain the same portable
function behavior without those attributes.

The global locale is checked on **every call**. It is not cached or assumed
immutable: native code can replace it without a Wio setter. Grouping, signed and
unsigned byte promotion, min/max boundaries, prefix/NUL preservation and native
locale changes remain part of the contract. Custom-locale calls now pay an
out-of-line call; no blanket zero-regression claim is made for that cold path.
The purpose is a smaller optimizer-visible hot body, not a different output
format or skipped locale behavior. Heap traffic need not change.

## Validation and measurement

The new plan gate covers accepted readonly aliases and negative async/native/
generic/inherited/capture/call/rebind/escape/processor/metadata cases, including
receiver access after formatting. The Wio fixture covers primitive updates,
readonly calls, formatting-before-later-self-use fallback, base/derived override
selection and explicit super calls. The emitted C++ additionally exercises the
unchanged ABI through function pointers, null rejection and expired callback
borrow rejection at O0/O2. Existing relevant formatting/borrow checks run with
native O0/O2 and VM; known VM negative boundaries remain explicit.
The added integer hot-path unit compares append/Stringify against live stream
oracles for limits and deterministic values, with alternating classic/custom
global locales. The Release VM measurement host is deliberately unchanged;
this native header/code generation pass is not a heavy VM optimization result.

Fresh before measurements reuse the previous committed artifacts without
rebuilding them. Their generated source SHA256 is
`08e6e3aa48144ad551495347734ec3861a21194195763d37ce85edf8b574fb91`.
The earlier warning-only formatted-size annotation is not a runtime difference
in these frozen artifacts. The workload's five source files, 10,001 iterations,
controller and Release VM host are unchanged. Native/C++ use O2 with contraction
disabled; C# uses .NET 9 Release. Python prepares builds and oracles; the Wio
controller owns all timing, controls, comparison rounds and distinct final
native/VM repetitions. Heavy work is serial. No trimming, retry-to-pass,
priority/power/security changes or unrelated process kills are used.

The user's existing dirty std/playground edits remain untouched. Declared
artifact hashes are evidence, not a clean full-transitive-source qualification.
Global-new/delete diagnostic windows, root-process peak RSS, C# current-thread
managed bytes and executable/generated-source size remain separate metrics.

The receiver-only collection precedes the final prelude-proof tightening. Its
red regression test is retained; the final production predicate rejects that
case. Final generated C++ for the measured workload is identical to the
receiver-only body after normalizing the output-folder source path (one five-
byte path difference). Thus that proof correction does not silently change this
kernel between stages. The new integer header is a distinct build input and the
final executable is rebuilt, not assumed binary-equivalent. See the retained
[source identity check](evidence/n03-receiver-source-equivalence-2026-10-17.json).

## Observations: fresh before -> receiver -> receiver + cold integer fallback

Comparison medians, milliseconds:

| Case | Wio native | Release VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 1.0847 -> 0.9826 -> 0.8473 | 55.2799 -> 60.4085 -> 61.9950 | 0.4266 -> 0.4796 -> 0.4438 | 1.0504 -> 1.1906 -> 1.3218 |
| Ordered hooks + builtins | 2.7687 -> 2.7876 -> 2.6165 | 77.5139 -> 85.5386 -> 91.6352 | 1.3913 -> 1.3961 -> 1.6084 | 2.4572 -> 2.8035 -> 2.7981 |

Distinct final native/VM phase, milliseconds:

| Case | Native before -> receiver -> final | VM before -> receiver -> final |
| --- | ---: | ---: |
| Single hook | 1.0850 -> 1.0222 -> 1.1231 | 56.1724 -> 63.7571 -> 63.9653 |
| Ordered hooks | 2.8539 -> 2.9330 -> 2.7891 | 81.0157 -> 92.5832 -> 87.9024 |

All three sessions retain **84/84 correct outputs** (252/252 total), applied
affinity, unchanged declared endpoint artifacts and unchanged endpoint power
profiles. All remain globally **timingQualified=false**. In the final collection,
single native comparison range is 0.43 with A/A difference 0.14; single C++ range
is 1.66 and drift 0.78. Final native ranges/drifts are 0.13/0.13 and 0.16/0.16.
These samples are retained rather than replaced or trimmed. The runner's exit
code 1 denotes rejected timing qualification here, not a failed workload.

The lower comparison native medians are observations, not accepted percentage
speedups. Receiver-only ordered time rises slightly; final-repeat single native
time rises versus both its intermediate and fresh baseline. That is **not**
reported as a universal improvement. The measured C++ gap remains substantial,
and the 1.05-1.10x target is not met. VM timings rise despite an unchanged Release
host and unchanged WIR/bytecode semantics; no VM optimization/regression cause
is attributed from this native-only, globally unqualified series.

For historical context, the previous dated field/reserve session observed
1.0735/2.7163 ms native comparison medians (distinct final 1.1589/2.8653).
The current fresh frozen-binary baseline is 1.0847/2.7687 (final 1.0850/2.8539).
The earlier pre-string-transport session observed 2.3763/5.2681. Those dated
denominators are not selectively interchanged or described as one paired run.

## Memory and artifact tradeoffs

Native global-new/delete windows are unchanged in all three stages:

| Case | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks / bytes |
| --- | ---: | ---: | ---: | ---: |
| Single hook | 2 | 64 | 64 | 0 / 0 |
| Ordered hooks | 10003 | 315097 | 97 | 0 / 0 |

This pass removes general receiver transport, not another heap cell per
iteration. No additional heap saving or measured stack-peak saving is claimed.
Final comparison root-process peak RSS medians (KiB), native/VM/C++/C#:
9236/21324/5200/28532 and 9240/21324/5204/35428. Before native RSS is 9212/9216;
those small shifts do not demonstrate lower resident memory. C# current-thread
managed bytes remain 1589768/3940040, a different accounting scope.

| Artifact bytes | Before | Receiver | Receiver + cold fallback |
| --- | ---: | ---: | ---: |
| Generated C++ | 3265016 | 3265921 | 3265926 |
| Unstripped Windows native executable | 6407351 | 6399095 | 6401165 |

Keeping ABI wrappers grows generated source. The final five source bytes are
the output-directory path difference, not a larger kernel. Cold outlining adds
2070 executable bytes versus receiver-only, while the combined executable is
6186 bytes smaller than before. Code size is not runtime or heap usage.

## Gates, evidence and next work

The final 3/3 focused CTest gates pass in 91.76 seconds (integer unit, receiver
plan and expanded Wio O0/O2 + VM integration). The integer unit also compiles
and runs under GCC O2 with `-Wall -Wextra -Werror -pedantic`; the new receiver
plan/test pass GCC C++20 syntax checking in addition to the MSVC build. No old
full functional corpus was run locally. Cross-platform CI remains independent.

- Before: [all samples](evidence/n03-receiver-before-2026-10-17.json),
  [original build provenance](evidence/n03-receiver-build-before-2026-10-17.json),
  [memory](evidence/n03-receiver-memory-before-2026-10-17.json).
- Receiver-only: [all samples](evidence/n03-receiver-intermediate-2026-10-17.json),
  [build](evidence/n03-receiver-build-intermediate-2026-10-17.json),
  [memory](evidence/n03-receiver-memory-intermediate-2026-10-17.json).
- Final: [all samples](evidence/n03-receiver-after-2026-10-17.json),
  [build](evidence/n03-receiver-build-after-2026-10-17.json),
  [memory](evidence/n03-receiver-memory-after-2026-10-17.json).
- Correctness: [CTest](evidence/n03-receiver-tests-2026-10-17.xml),
  [Wio/native/VM processes](evidence/n03-receiver-validation-2026-10-17.json),
  [retained prelude-proof discovery](evidence/n03-receiver-order-discovery-2026-10-17.xml),
  [strict toolchain checks](evidence/n03-receiver-strict-2026-10-17.json).

N03 stays open. Next cost attribution is remaining formatted-value/merge
transport and live-locale work, then richer formatting/legacy Format and KI-008
surfaces. Do not broaden a raw receiver proof across calls or weaken locale,
null, ownership or callback contracts to manufacture a lower number. N04 has
not started; N01/N14 timing qualification and KI-007/KI-009 remain open.
