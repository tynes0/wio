# N03: preserve the host floating display contract

2026-10-17. N03 remains in progress; this is a correctness follow-up, not a new
speedup claim, N03 closure, or a transition to N04.

## Observed failure and fix

[CI run 38974779551](https://github.com/tynes0/wio/actions/runs/38974779551)
passed Windows Debug and Ubuntu Release. macOS Release compiled successfully,
but its host floating oracle and Wio formatting gate failed:

```text
value=-0x1.a349dp+20
host stream=-1.71740e+06
optimized output=-1.7174e+06
```

The numeric value is equivalent, but Wio must not change its existing display
text silently as an optimization. `AppendFloating` now conservatively retains
the stream fallback when compiled with libc++. The finite/classic/nearest
`to_chars(general, 6)` path remains enabled on the locally qualified MSVC and
libstdc++ hosts. This is a standard-library gate, not an OS-name check: Clang
with libstdc++ and Clang with libc++ are different qualification targets.

The counterexample is explicit in the host oracle as both f32 and f64, before
the 30,000-bit-pattern test for each type. Expected text comes from the actual
host stream, not a spelling hard-coded from Windows. The same shared helper
serves generated native formatting and VM builtin formatting. No numeric value,
precision, rounding mode, locale, bytecode opcode or public API changes.

The reserved builder allocation assertion now covers integer/bool values on
every host. Floating values still have correctness checks, but their stream
fallback is not advertised as allocation-free. This narrows an unsupported
cross-platform performance promise; it does not waive the float output oracle.

## Validation

The six new N03 tests passed locally in 131.76 seconds: the primitive fusion
proof, numeric allocation/locale unit, 60,000-value float oracle, VM formatter
unit, and both Wio native O0/O2 + VM gates. No old functional corpus was run.
The macOS failure evidence is retained separately from the passing local run:
[CI probe failure](evidence/n03-libcpp-failure-2026-10-17.json),
[CI test log](evidence/n03-libcpp-failure-2026-10-17.log),
[local six-test result](evidence/n03-portability-tests-2026-10-17.xml).

[Correction CI 38994486788](https://github.com/tynes0/wio/actions/runs/38994486788)
at source commit `7bebac4`: Ubuntu Release and macOS Release are successful;
Windows Debug is still building at this report checkpoint. Do not infer its
result from the passing local Windows gate.

## Fresh same-source Windows measurement

The five workload/project source files are byte-identical to the preceding
numeric-builder after snapshot. Current native/diagnostic/C++/C# programs and
bridge were rebuilt serially; the Release VM host was rebuilt first. The
Wio-written controller is unchanged (SHA256
`d281d70686e6585d4382770c283acbc3c54580ed81c00dafacfa93726ebe6362`).
Inputs remain 10,001 iterations, O2 native/C++, .NET 9 Release, five rotating
comparison repetitions plus A/A controls, and a distinct three-repetition final
native/VM phase. No concurrent heavy local build/measurement or system setting
changes were introduced. Declared artifacts are hashed; this is not a pristine
full-transitive-source build. Existing dirty std/playground files are preserved.

Comparison medians, milliseconds, **initial -> preceding -> current**:

| Workload | Wio native | Release VM |
| --- | ---: | ---: |
| Growing rows | 8.7500 -> 7.9471 -> 7.9140 | 123.7713 -> 115.0297 -> 102.3193 |
| Clear/reuse integers | 8.0708 -> 7.3130 -> 7.2742 | 157.2808 -> 147.8166 -> 131.9380 |
| Reserved floats | 2.3373 -> 2.2435 -> 2.2247 | 46.3059 -> 40.9132 -> 35.6171 |

The initial state explicitly includes the new API's interpolation body; it is
not an unmodified release. The earlier report retains the exact baseline scope.

Current independent references (same sources, freshly built), milliseconds:

| Workload | C++ direct to_chars | C# span TryFormat |
| --- | ---: | ---: |
| Growing rows | 1.2136 | 3.1764 |
| Clear/reuse integers | 1.1360 | 5.7721 |
| Reserved floats | 0.4925 | 2.5335 |

Separate final phase, **preceding -> current**, milliseconds:

| Workload | Native | VM |
| --- | ---: | ---: |
| Growing rows | 8.4193 -> 8.4368 | 115.1800 -> 105.0403 |
| Clear/reuse integers | 7.8544 -> 7.3012 | 145.2788 -> 130.2230 |
| Reserved floats | 2.1646 -> 2.1980 | 40.7193 -> 36.6369 |

All **126/126** warmup/control/sample process outputs are correct; affinity,
endpoint artifacts and power profile checks pass. Overall timing qualification
is **false** (for example C++ growing-row range 0.35 and current native reserved-
float range 0.25). The controller therefore exits 1 while preserving the complete
session. This is not a compiler/runtime correctness failure. No samples were
trimmed, replaced or retried. Current Windows production behavior did not change
in this libc++-gated correction, so the lower VM observations are **not attributed
to a new optimization**. Native rows/floats have slightly higher final medians
despite lower comparison medians. No speedup factor or N14 budget is accepted.

Native diagnostic memory is unchanged from the preceding snapshot:

| Workload | Allocations = frees | Requested = freed bytes | Peak live bytes | Final live blocks/bytes |
| --- | ---: | ---: | ---: | ---: |
| Growing rows | 18 | 1327595 | 803358 | 0 / 0 |
| Clear/reuse integers | 20005 | 1011570 | 311 | 0 / 0 |
| Reserved floats | 5 | 215173 | 215173 | 0 / 0 |

These are separate global-new/delete diagnostics, not production timings,
all malloc traffic, copied bytes or VM heap counters. Current comparison median
root-process peak RSS (KiB):

| Workload | Native | VM | C++ | C# |
| --- | ---: | ---: | ---: | ---: |
| Growing rows | 10980 | 22220 | 6436 | 29652 |
| Clear/reuse integers | 10364 | 22532 | 5900 | 30876 |
| Reserved floats | 10456 | 20656 | 5932 | 26408 |

RSS rose across all four implementations compared with the preceding session;
there is no isolated resident-memory reduction claim. C# current-thread managed
allocation counts retain their separate scope; their workload bytes remain
1400232 / 1840384 / 485192, not native requested-byte or RSS equivalents.

Raw evidence: [full controller session](evidence/n03-portability-benchmark-2026-10-17.json),
[serial build commands](evidence/n03-portability-build-2026-10-17.json),
[native allocation diagnostics](evidence/n03-portability-memory-2026-10-17.json).

## Remaining work

libc++ floating performance needs its own measured, output-compatible fast path.
The fallback is deliberately not reported as equally fast to the qualified
`to_chars` path. Failed CI stays visible; local Windows results do not prove
macOS success. Shared user ToString lowering/VM invocation (KI-008), richer
formatting, and the legacy Format inventory remain open under N03. N01/N14
measurement qualification and the fixed N01-N14 numbering are unchanged.

### Legacy surface inventory

A source search across compiler/runtime/sdk/std/tests/app found generated calls
to `wio::runtime::Format` only in the legacy generator's
`codegen/detail/expression_visitors.inl`; its header is emitted by
`detail/generator_core.inl`. The runtime header is still in the packaged header
inventory, so external native users cannot be ruled out by that search. The
active `wir_cpp_backend.cpp` uses `append_formatted` / `AppendBasicFormatted`,
not the legacy Format entry point. Keep compatibility review visible, but do
not attribute active WIR benchmark costs to the dormant generator call path.
