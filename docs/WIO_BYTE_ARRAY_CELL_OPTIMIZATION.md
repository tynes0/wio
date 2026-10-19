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
No machine permissions are changed. After measurements, full Wio heap and
platform CI evidence remain pending; no accepted speedup is claimed yet.

## Runtime cost attribution aid

`tests/performance/byte_borrow/costs.cpp` is a separate C++ runtime-only diagnostic
for hash-only, owning/reused read+hash and owning/reused read+sampled-probe costs.
It is not generated Wio, a four-language comparison, an additive cycle breakdown
or a qualified performance gate. File bytes and full/probe checksums are checked
independently. Keep it separate from the full-byte-FNV language comparison;
sampling only endpoints is a different validation contract, not a faster result
for the same workload. Neither existing full-FNV workload nor its oracle changes.
