# N04: inline cells for closed native byte-array observers

2026-10-19. This targets the existing owning ReadBytes/ReadSomeBytes workload,
not another opt-in reusable API or closure of N04/N05/N14.

## Measured hypothesis and implementation

The previous generated owning-read loop allocates a 65536-byte payload plus a
48-byte heap Place cell per iteration: 514 new/delete calls and 16855088 requested
bytes for 257 reads. The array cell is local; its only observer is a synchronous
native Hash taking view u8[] and returning a scalar. The payload must stay owned,
but that closed-use cell does not need shared heap identity.

The C++ storage plan now reuses the existing InlinePlace<T> for proven local
dynamic u8/byte arrays. Every use must be direct initialization/store/cleanup or
a validated call-scoped immutable native leaf. Reborrows, loads, captures, CFG
cell arguments, Wio/indirect calls and unknown uses keep full Place storage.
Stack/local-or-store escape metadata is necessary but not sufficient.

The private leaf entry takes const std::vector<uint8_t>&. The original Place
entry stays available for function values, callbacks, exports and wire/VM
adapters. WIR IDs, native ABI versions, object RC and module/SDK contracts do not
change. Payload ownership and snapshot semantics remain unchanged: this removes
the cell allocation, not the owning payload or its initialization.

Byte-leaf qualification is deliberately narrow: C++ default calling convention,
closed external/free function, immutable dynamic byte-array borrows plus optional
trivial scalar values, scalar/void result, matching types, call lifetime and caller
thread. Nullable, retained, foreign-thread, callback, receiver, mutable-array,
fixed-array, non-byte, generic/open and escaping-result surfaces fall back.
String leaf qualification shares the common validation but keeps its existing
scope; its regression proof is rerun. This is native emission work, not a new VM
array representation or a general array-place optimization.

## Validation and measurement design

The new plan proof includes positive u8/byte cases and conservative rejection
cases. The new Wio fixture checks actual emitted InlinePlace/private helper,
native O0/O2, same-array aliasing, Wio-call fallback, native function values,
exception translation and an actual generated-Wio heap window. Both native
bridge optimization levels exercise the supported VM path.

The initial VM fixture exposed the already tracked KI-009 indirect native
function-value gap (WVM1003). Native indirect calls remain a positive gate; VM
indirect calls are now explicitly labeled expected-known-gap negatives. This
optimization does not claim to fix KI-009. Preserve the initial failed run.

Both before and after select the two owning groups from the exact same frozen
five-file byte-I/O fixture. Each group still includes Wio native/VM/C++/C#,
257 reads, the full-byte FNV checksum, identical flags and qualification limits.
The selection utility changes only complete workload groups and labels; it
records the source manifest hash and does not overwrite/rebuild the old images.
The helper is named `select_workloads.py` to avoid shadowing Python's `select`
module on platforms where it is not built in. The before session used the
original `select.py` name; its unchanged contents are preserved as source evidence.
Comparisons/final Wio phases are collected by the Wio controller, not Python.
No concurrent local build/test, retry/trimming or machine power/priority change.

Before completes with 84/84 correct samples and timing unqualified. The scoped
local semantics gates pass 2/2 in 278.47s (29 existing I/O processes and 19 new
borrow processes, including two explicitly labeled KI-009 negatives). Final
plan/string-regression/selector gates pass 3/3 in 5.53s; the selector has six unit
cases. Its fixtures use ordinary workspace UUID directories because Python
mode-0700 temporary directories initially failed under the Windows sandbox ACL.
No machine permissions are changed.

## Before/after outcome

Both sessions finish with 84/84 correct samples, unchanged artifacts within each
session, qualified CPU affinity and unchanged power profile. Both remain timing
unqualified. The five Wio/C++/C#/header/project source hashes match exactly;
reference binaries were rebuilt from those same inputs and flags in the after
folder, not reused as byte-identical images. Runtime archive, Release VM and Wio
controller hashes stay unchanged. The candidate compiler is source `58c2bda`;
the subsequent diagnostic-reporting repair is not in the timed images.

Kernel medians, milliseconds (five comparison repetitions):

| Workload | Wio native before -> after | C++ before -> after | C# before -> after | Wio VM before -> after |
| --- | ---: | ---: | ---: | ---: |
| Exact 64 KiB x 257 | 34.0282 -> 27.8940 | 25.1953 -> 27.5185 | 28.1696 -> 20.7071 | 4109.7038 -> 2567.8386 |
| Some 64 KiB x 257 | 35.9776 -> 28.2631 | 24.7642 -> 26.7190 | 27.2410 -> 20.9464 | 4104.4936 -> 2577.2638 |

The after native/C++ ratios are 1.014x/1.058x, but these are **not accepted
parity**. Native comparison ranges are 37.86%/37.05%; C++ ranges are
63.17%/31.59%. The separate final Wio phase has native medians
33.1913 -> 31.7745ms and 36.0132 -> 28.0967ms, VM
5098.4323 -> 2616.3305ms and 4357.4471 -> 2627.5706ms. Its exact-native
A/A median difference is 14.89%. Native comparison medians fall 18.03%/21.44%,
but unchanged-language/control changes and failed timing qualification prevent
attributing those percentages to this optimization. In particular this is not
a VM optimization despite the lower observed VM times. No retries or trimmed
sessions replace these observations.

Actual generated-Wio global-new/delete diagnostic, for **each** owning workload:

| Counter | Before | After |
| --- | ---: | ---: |
| Allocations / frees | 514 / 514 | 257 / 257 |
| Requested / freed bytes | 16855088 / 16855088 | 16842752 / 16842752 |
| Peak live bytes / blocks in window | 65632 / 3 | 65536 / 1 |
| End live bytes / blocks; allocation failures | 0 / 0; 0 | 0 / 0; 0 |

This is a verified 50% reduction in allocation calls, but only 12336 fewer
requested bytes (the 65536-byte payload per iteration is deliberately retained).
It is not a 50% memory-footprint reduction. Reusable-buffer cases retain their
previous zero hot-window allocation counts, with the caller buffer outside that
window. Process peak RSS is essentially unchanged: owning native comparison
9288/9284 -> 9276/9276 KiB; after C++ 5420/5416 KiB, C# 37012/37004 KiB,
VM 24820/24812 KiB. RSS includes process/runtime state, not just the kernel.

Generated C++ size 3599292 -> 3599866 bytes; native image 5758419 -> 5758619
bytes; bridge 2459505 -> 2459069 bytes. Sidecar size 2724504 -> 2724514 bytes
includes different source paths; no bytecode representation improvement is
claimed. Single preparation observations: emit 4.453 -> 6.223s, native C++
compile 150.121 -> 152.749s, bridge/sidecar build 52.572 -> 25.329s. These are
not repeated/qualified build-time comparisons or compile-time gains.

## Platform qualification follow-up

Source `58c2bda` passes the Windows Debug CI job. Ubuntu Release times out while
building the new fixture's O0 native bridge; macOS 15 Release fails in the same
phase while reporting native output (`std::regex` complexity limit). This is
**not** three-platform qualification. The other new N04 plan/selection and
existing byte-I/O gates pass there; preserve the failed run
[39181485255](https://github.com/tynes0/wio/actions/runs/39181485255).

`c377e0d` bounds structured native diagnostic matching to 2048-byte physical
lines and catches regex complexity failures. Oversized/unmatched output remains
verbatim in the existing raw-output fallback; native errors are not suppressed
or accepted. A new real compiler regression checks short and 16-KiB `#error`
messages, failure status and intact markers (local 1/1, 9.37s). CI also preserves
the new borrow/diagnostic evidence. The native bridge root error and three-platform
closure still need qualification; N04/N14 stay open. Diagnostic changes are not
included in the measured candidate.

Follow-up: the revealed native error was a fixture executable named `memory`
inside an include directory, shadowing the standard `<memory>` header on Unix.
`eb3f811` renames it to `allocation-probe`. Source `b14ea3e` (also adding the
high-sample controller) passes all scoped Windows Debug, Ubuntu Release and
macOS 15 Release jobs in
[CI 39305065595](https://github.com/tynes0/wio/actions/runs/39305065595).
The byte-cell platform qualification gap is closed; broader N04/N14 work is not.

Raw evidence: `docs/evidence/n04-byte-cell-{before,after,build,memory,costs}-2026-10-19.json`;
the before allocation counters are in `n04-reuse-memory-2026-10-19.json`.

## Runtime cost attribution aid

`tests/performance/byte_borrow/costs.cpp` is a separate C++ runtime-only diagnostic
for hash-only, owning/reused read+hash and owning/reused read+sampled-probe costs.
It is not generated Wio, a four-language comparison, an additive cycle breakdown
or a qualified performance gate. File bytes and full/probe checksums are checked
independently. Keep it separate from the full-byte-FNV language comparison;
sampling only endpoints is a different validation contract, not a faster result
for the same workload. Neither existing full-FNV workload nor its oracle changes.

At 4097 iterations, five diagnostic rounds give medians: hash-only 350.1009ms,
owning read+hash 373.0134ms, reused read+hash 364.2173ms, owning read+probe
23.4259ms, reused read+probe 17.2320ms. All 25 outputs match independent full-FNV
or sampled-probe oracles. This points to full-byte hashing dominating this
particular C++ runtime probe; it does not establish an additive Wio cost breakdown
or justify weakening the full-FNV language acceptance workload.
