# N01 bulk-copy attribution — 2026-10-15

Opening/closing stage **N01**; N02 has not started. This slice introduces Wio
fixtures and diagnostic tooling, not a production compiler/runtime/std change.

## Implementation and test evidence

[The Wio workload](../tests/performance/native_costs/copies/costs.wio) covers
owning/view/return transport for string, text and string-bearing components,
repeated string/text `+=`, scalar-only components and an explicit native copy/
overlap calibration called from Wio. Setup/case selection is outside the window.
Wio checks source immutability afterward; Python independently validates output
including embedded NUL and Unicode byte sequences. No new PowerShell logic.

GCC15.2 C++20 diagnostic: `-O2 -fno-inline -fno-builtin -fno-ipa-cp`, static C++
library linkage and GNU `--wrap=memcpy/memmove`. Wrappers record the byte-length
arguments and forward to the real implementation. Every case also runs as
ordinary O2 without wrappers, and its unmeasured counters are null.
[Full commands and limitations](../tests/performance/native_costs/copies/README.md).

Only the new `wio_native_copy_attribution` CTest ran locally: **1/1, 52.32 s**,
five Python checks and **78/78 actual Wio executions**, 13 cases at counts
1/17/2000 in two variants. All independent checksums, source-integrity checks,
native calibration and immutable-input/artifact hashes pass. The existing
correctness corpus was not rerun. Earlier local setup had an invalid double-ref
in the fixture (`ref` applied to an already borrowed parameter); only the new
fixture was corrected, with failed build evidence retained under
`build/native-copy-initial-2026-10-15`. No language semantics were changed.

The prior memory-attribution change is confirmed CI-green, including the new
Windows/Linux/macOS memory test:
[run 38776086732](https://github.com/tynes0/wio/actions/runs/38776086732).
This new copy diagnostic is wired for Windows/Linux GCC only; its new CI result
is not yet claimed. macOS Clang linking is outside this diagnostic's support.
This feature-branch push is narrowed to the new copy test, matching the user's
new-tests-only policy. Main/release/PR functional gates and earlier diagnostic
tests remain intact. The macOS feature job is build-only for this GCC-specific
slice, not a claimed copy-attribution execution.

## Observed diagnostic copy requests

Count 2000, sums of observed C-call byte lengths, not allocation bytes, total
language copies, hardware memory traffic or production-O2 counts:

| Wio path | memcpy calls | memcpy bytes | memmove bytes |
| --- | ---: | ---: | ---: |
| Explicit C-call calibration | 4000 | 14000 | 32000 |
| String owning argument | 2000 | 90001 | 0 |
| String view | 0 | 0 | 0 |
| String owning return | 2000 | 90001 | 0 |
| Text owning argument | 2000 | 84001 | 0 |
| Text view | 0 | 0 | 0 |
| Text owning return | 2000 | 84001 | 0 |
| String-bearing component owning argument | 2000 | 90001 | 0 |
| String-bearing component view | 0 | 0 | 0 |
| String-bearing component owning return | 2000 | 90001 | 0 |
| String repeated += | 6001 | 90135003 | 0 |
| Text repeated += | 8002 | 168086004 | 0 |
| Scalar-only component | 0 | 0 | 0 |

[Raw copy samples/provenance](evidence/copy-attribution-2026-10-15.json).
Native calibration's memmove call count is also exactly 4000. Zero-length copies
count as calls but add no bytes; both overlap directions preserve the independently
modeled buffer. Zero for the scalar component is not proof of zero field copies.

The final concatenated string is 90000 bytes and text 84000 UTF-8 bytes. At counts
1/17/2000, string observed bytes are 93 / 7653 / 90135003; text is 89 / 12873 /
168086004. This is strong **diagnostic evidence of growing-prefix copy costs**,
not an end-to-end throughput comparison. Non-inlining and static C++ linkage can
change the operation mix; some copies remain invisible, and terminators/runtime
representation bytes can be included. Do not relabel these as complete payload
copy totals or infer a universal ratio against C++.

The frozen generated C++ confirms that these `+=` paths materialize an owned RHS,
then evaluate binary `+` on the existing output and write back the result, roughly:

```cpp
rhs = source;
newValue = current + rhs;
output.write(std::move(newValue));
```

The runtime Text binary `+` takes its left operand by value, then appends. These
paths motivate N02's proven last-use buffer reuse/in-place append work. An eventual
fix must preserve source/alias snapshots, embedded NUL, UTF-8 validity, evaluation
order, exceptions and native/VM parity; this slice does not implement that fix.

## Fresh unchanged-artifact Mandelbrot tracking

[Raw tracking samples](evidence/mandelbrot-n01-copies-2026-10-15.json): **32/32**
output checks, unchanged artifact hashes and endpoint power state, inherited
CPU2 affinity, excluded warm-ups, five native/three VM samples per lane.
**Overall TimingQualified=false**. Same-binary native A/A medians differ by
12.18%, over the 10% gate; native range, full-native drift/spread and full-VM
drift also fail. All samples remain, without trimming or retry-until-pass.

| Lane | First fast N01 session | Previous memory session | Current copy session |
| --- | ---: | ---: | ---: |
| Wio native compute | 23.3227 ms | 24.0372 ms | 51.7406 ms |
| Wio native full PPM wall | 133.6333 ms | 137.4266 ms | 278.5126 ms |
| Release VM compute | 19.0443468 s | 20.6477775 s | 42.6891409 s |
| Release VM full PPM wall | 20.5959902 s | 22.3969702 s | 52.4043856 s |

Current native compute range 43.0200–54.0010 ms; same-executable A/A median
45.7986 ms. VM compute range 42.0778752–45.3573051 s. The previous full-native
control, also unchanged, has median 286.2155 ms and range 257.4373–808.8938 ms.
This is a much slower, unqualified measurement session, **not a new production
code regression**: the tracking inputs/executables were not rebuilt or changed.
Do not assign a single scheduling/thermal/hardware cause from these data alone.

First fast N01 PPM values used an older external-wall controller; later values
use Python child-wall timing excluding worker startup. Internal compute timing
is unchanged. The table is historical context, not controlled optimization deltas.
Even when one lane passes a coarse gate, this does not make cross-session ratios
or the full rejected session acceptable as a performance budget.

Fixed references, **2026-10-04**, not rerun this turn:

| Implementation | Compute | Full PPM wall |
| --- | ---: | ---: |
| C++ GCC15.2 O2 | 28.4052 ms | 76.02 ms |
| C# .NET9 Release | 29.5856 ms | 149.13 ms |
| CPython3.12.14 | 1348.0412 ms | 1613.71 ms |

Same 800x800/100-iteration algorithm/output fingerprints; dates/controllers and
whole-process output costs differ. Do not use these as precise current speed
ratios. Python3.13.14 runs the harness, not the dated Python reference workload.

## Remaining N01 boundaries

There is still no complete instrumentation of arbitrary scalar/inline/SIMD or
custom-loop payload copies, shared-library internals or exact shipping-O2 copy
counts. General direct malloc/custom-allocator traffic, broader subsystem coverage
and precise/noise-qualified microcase timing remain open. Missing measurements
stay explicit rather than being filled with zero. N01 remains open, not N02.
