# N03: direct primitive builder append

2026-10-17; second production slice of N03. This is not N03 closure, N04, the
general VM optimization pass, or acceptance of the N14 native-overhead budget.

## Wio API and exact types

```wio
use std::builders as builders;

fn Entry() -> i32
{
    let output = builders::StringBuilder(256usize);
    output.Append("id=");
    output.Append(9007199254740993u64);
    output.Append(", ratio=");
    output.Append(1.25f64);
    output.AppendLine(true);
    let snapshot = output.ToString();
    output.Clear();
    return snapshot == "id=9007199254740993, ratio=1.25true\n" ? 0 : 1;
}
```

The new generic Append/AppendLine overloads have a frozen primitive allow-list:
i8/i16/i32/i64/isize/u8/u16/u32/u64/usize/byte/f32/f64/bool. The frontend enforces
`[apply(...)]`, including explicit-specialization calls. They do not silently
widen u64/i64 through double, invoke arbitrary native stream operators, or add
generic user/container formatting. Existing string overloads and AppendChar
remain. AppendLine appends exactly one newline; ToString still owns a snapshot.
Reserve and Clear retain their existing capacity contracts. Std follows the
scoped explicit visibility, multiline body, attribute and blank-line rules.

## Backend-independent optimization

The generic Wio body remains `self.value.Append($"${value}")`. A dedicated
`NumericAppendOptimizer` runs in canonical Lowered WIR and changes only an
adjacent, sole-use primitive interpolation plus mutable string Append pair:

```text
%formatted = interpolate string ["", ""] (%number)
intrinsic-call string::Append(%target, %formatted)
drop-value %formatted

// becomes, with the exact numeric signature retained
intrinsic-call string::Append(%target, %number)
```

Eligible leaves are trivial numeric/bool values. The pass requires exactly one
format operand, empty affixes, an owning string result and a mutable string
reference receiver. Any ordinary extra use, branch argument, borrow origin,
cross-block cleanup, intervening instruction, text/container/user formatter,
async function or coroutine frame retains the old path. Only matching ordinary
same-block string cleanup is removed; arbitrary cleanup is not discarded. Value
ids are not renumbered, the append's source span is retained, and the pass is
idempotent. Profiling exposes WIR.Optimize.NumericAppend and its fused-pair count.
This is not a new numeric public builtin string member in semantic analysis.

Generated C++ uses a constrained internal StringAppend overload and the existing
shared integer/float helpers directly into storage. VM uses the same typed
intrinsic, mutating its actual string place while retaining the distinct-target
transactional path. Primitive type metadata selects byte/f32 behavior and rejects
unsupported payload/type families. No growing string is marshalled through a
native ABI thunk for every number; no bytecode opcode/schema or SDK ABI is added.
An old VM host does not implement this new typed intrinsic surface and must not
be substituted for the matching rebuilt host.

Display remains defaultfloat precision 6, not shortest-roundtrip or JSON number
serialization. Classic-locale/nearest finite floats on qualified MSVC/libstdc++
hosts take the stack-buffer path; libc++ retains the host stream contract;
locale, non-default rounding and nonfinite fallbacks retain stream behavior.
Numeric i8/u8/byte keep their locale-independent decimal policy. Snapshot, NUL,
signed-zero and exact integer transport are covered separately from timing.

## Portability and validation findings

The preceding N03 CI run 38970644076 passed Windows Debug and Ubuntu Release,
but macOS Release failed compilation: libc++ const vector<bool> iteration still
returns a proxy. `AppendBasicFormatted` now explicitly normalizes a vector<bool>
element to bool, without treating arbitrary proxy/native/user types as builtin
formatters. The original failure remains visible in that run; the new CI result
must be checked independently, not inferred from local MSVC/libstdc++ success.

New-only validation consists of the numeric fusion proof matrix, a native helper
allocation/locale/rounding test, and Wio native O0/O2 + VM. The Wio gate checks
that at least 13 concrete numeric Append bodies in actual emitted Lowered WIR
contain direct append and no interpolation, as well as runtime results and the
frontend's explicit-string specialization rejection. The native reserved loop
requires zero observed allocations and unchanged capacity, not a whole-VM heap
claim. New tests also cover libc++ bool-vector proxy normalization.

Development failures are retained: an initial proof fixture assumed a nonexistent
i64Type convenience accessor; a generic-dependent public builtin call was rejected
by the frontend and was not shipped; the negative CLI assertion initially looked
only at stderr although file-check diagnostics used stdout. A MinGW unit allocator
diagnostic mixed replacement new/delete with a shared C++ runtime and fast-failed;
the independent GCC unit now links its C++ runtime into the same diagnostic image.
That linkage fix is test-only, not a change to benchmark/native production flags.
No retries/trimmed timing samples or old functional corpus run are implied.

## Measurement contract

The related programs cover growing mixed rows, clear/reuse with exact large signed
and unsigned integers, and reserved f32 appends. All use runtime input, full UTF-8
FNV checksums and matching capacity hints. C++ uses independent direct to_chars;
C# uses stack-span TryFormat and direct builder append, invariant G6/lowercase e
for floats. C# UTF-16 storage, UTF-8 hashing arrays and eventual GC are real costs,
not identical allocator/destruction contracts. Native local destruction is inside
the kernel; no forced C# collection is inserted to make an artificial comparison.

The baseline revision is 0921924 **plus the interpolation implementation of the
new numeric Append API** and the bool-proxy portability correction. The std
source is frozen as builders-baseline.wio. It is not an untouched
released std. AppendLine is additionally qualified after the optimization, but
the measured workloads call only Append. The same workload-source copies are
rebuilt after the compiler/runtime change. Frozen generated C++, binaries and
bytecode remain locally; the old VM host was retained and matches its baseline
SHA256. The Wio controller image is unchanged throughout both sessions.

Builds and processes are serial, native GCC C++20 O2 with contraction disabled,
C# net9 Release with tiering disabled, Release VM host. Five rotating/reversing
comparison samples, excluded external warmups, two in-process warmups and native
A/A controls precede a distinct final three-sample native/VM-only phase. CPU 2
affinity is requested; power/priority/security settings are not changed. Allocation
diagnostics are separate requested/live/peak new/delete windows, not plain timing,
all malloc traffic, stack storage, VM heap counters or whole-process RSS. Declared
source and binary hashes do not claim a full transitive clean-repository snapshot.

See [fixtures and reproduction](../tests/performance/numeric_builder/README.md).

The new three-test run passed in 64.15 seconds; the final duplicate-cleanup guard
was then rebuilt and the proof matrix rechecked independently (1/1 in 2.63s).
Native O0/O2 and VM runtime assertions, explicit frontend rejection and the actual
13-specialization Lowered WIR check are not inferred from performance checksums.

## Observations, not timing-budget acceptance

Comparison medians, ms for 10,001 repetitions; **before -> after**:

| Workload | Wio native | Release VM | C++ direct to_chars | C# stack-span |
| --- | ---: | ---: | ---: | ---: |
| Growing rows | 8.7500 -> 7.9471 | 123.7713 -> 115.0297 | 1.3466 -> 1.2801 | 3.1119 -> 3.1276 |
| Clear/reuse integers | 8.0708 -> 7.3130 | 157.2808 -> 147.8166 | 1.2108 -> 1.1326 | 5.4875 -> 5.5674 |
| Reserved floats | 2.3373 -> 2.2435 | 46.3059 -> 40.9132 | 0.5097 -> 0.5223 | 2.4016 -> 2.3996 |

Distinct final native/VM phase (fresh warmups and A/A controls):

| Workload | Native before -> after, ms | VM before -> after, ms |
| --- | ---: | ---: |
| Growing rows | 8.0138 -> 8.4193 | 126.9112 -> 115.1800 |
| Clear/reuse integers | 9.5011 -> 7.8544 | 156.4103 -> 145.2788 |
| Reserved floats | 2.3209 -> 2.1646 | 44.1947 -> 40.7193 |

Both sessions have **126/126 correct outputs**, applied affinity and unchanged
power profile/endpoint artifacts. The controller SHA256 is identical. Both are
globally **timingQualified=false**: native A/A and some reference/VM stability
checks fail. For example, after clear-integer C++ range is 0.39 and growing-row
VM drift is 0.124. Growing-row native comparison looks lower, yet its separate
final median is higher. Keep that contradiction/noise visible; no definitive
speedup factor or before/after percentage is inferred, and no retry-to-pass was
run. Even the observed native gap versus the stronger direct C++ reference is
still substantial; this does not achieve the 1.05-1.10x objective.

Native diagnostic allocation accounting (allocations equal frees, requested bytes
equal freed bytes; **zero final live bytes/blocks** in every window):

| Workload | Allocations before -> after | Requested bytes before -> after | Peak live bytes before -> after |
| --- | ---: | ---: | ---: |
| Growing rows | 18 -> 18 | 1327595 -> 1327595 | 803358 -> 803358 |
| Clear/reuse integers | 40007 -> 20005 | 1631632 -> 1011570 | 311 -> 311 |
| Reserved floats | 5 -> 5 | 215173 -> 215173 | 215173 -> 215173 |

The large-integer case removes two temporary-format buffer allocations per
iteration. The observed requested-byte delta is 620062; this is allocation
traffic, not measured copied-byte volume. Short float/bool strings already fit
small-string storage, so their allocation counts need not improve. Final owning
snapshots remain deliberately allocated. Peak requested memory is unchanged,
not halved along with allocation count. The original isolated pre-reserved C++
primitive helper loop observed zero allocations on the local Windows hosts,
not the whole builder object/VM machine. The portability follow-up scopes the
cross-platform zero-allocation assertion to integer/bool append; floating
fallback may allocate inside the host stream.

After comparison median root-process peak RSS, KiB:

| Workload | Native | VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Growing rows | 10088 | 21372 | 5848 | 28368 |
| Clear/reuse integers | 9296 | 21612 | 5216 | 29564 |
| Reserved floats | 9432 | 19736 | 5244 | 25112 |

Native RSS before was 10080/9308/9396 KiB: not all RSS values fell. RSS includes
startup/host/bridge/warmup memory and is not an isolated heap metric. C# reports
current-thread managed kernel allocation bytes of 1400232/1840384/485192 for the
three workloads; those GC counters are not equivalent to native requested bytes.

Frozen generated C++ bytes are 3492895 -> 3491973, native executable bytes
6772489 -> 6769060, bytecode 2715974 -> 2715599, bridge DLL 5498529 -> 5498529.
These describe the whole scoped patch/API and path/link layout, not a standalone
fusion-only size attribution or compilation-throughput proof. The broad code-
size/build-scalability objective remains open.

Raw evidence: [before](evidence/n03-builder-before-2026-10-17.json),
[after](evidence/n03-builder-after-2026-10-17.json),
[baseline std](evidence/n03-builder-baseline-2026-10-17.wio),
[build before](evidence/n03-builder-build-before-2026-10-17.json),
[build after](evidence/n03-builder-build-after-2026-10-17.json),
[allocation before](evidence/n03-builder-memory-before-2026-10-17.json),
[allocation after](evidence/n03-builder-memory-after-2026-10-17.json),
[three tests](evidence/n03-builder-tests-2026-10-17.xml),
[final proof recheck](evidence/n03-builder-plan-final-2026-10-17.xml),
[Wio validation](evidence/n03-builder-validation-2026-10-17.json),
[initial unit failure](evidence/n03-builder-unit-failure-2026-10-17.json),
[diagnostic-channel assertion failure](evidence/n03-builder-diagnostic-channel-failure-2026-10-17.json).

## Remaining work

N03 remains open for richer formatting/precision, legacy runtime::Format and
remaining user-container/async/native/open-generic hook lowering (KI-008).
[Synchronous scalar hooks](WIO_FORMATTING_CALL_LOWERING.md) now use shared WIR
calls; that bounded correctness slice does not close the whole issue. The native/VM
mutating borrowed-LHS issue KI-007 is unchanged. Native object/call/representation
overheads are not excused or declared solved by this formatting slice; they remain
in the fixed later N05/N07/N08 passes. N01 residual attribution/qualification and
N14 acceptance stay visible. N04 has not started.
