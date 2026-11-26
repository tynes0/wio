# N02: local string/text buffer reuse

2026-10-16; first production slice, not N02 completion. N01's remaining profiling
coverage is still open. No std/playground user edits are part of this change.

Subsequently superseded by the [N02 concat closure](WIO_CONCAT_OPTIMIZATION.md).
The proof boundary, measurements and remaining-work statements below describe
this first slice only; they are retained as dated evidence, not current status.

## Change and proof boundary

The C++ storage plan recognizes an adjacent borrowed `Load`, string/text `Add`,
and `Store`/`Replace` into the same mutable `LocalPlace`. Both intermediate results
have exactly one use across the function, including borrow metadata and CFG
arguments. The backend emits `destination.read() += rhs` instead of constructing
the sum and moving it back. There is no new ABI, WIR opcode or VM behavior.

The already evaluated RHS is not removed or reordered. In particular, self-append
retains its snapshot, and RHS calls run before the selected adjacent sequence.
The old load proves initialization; no intervening operation is allowed. The
underlying String/Text append preserves the destination on allocation failure;
the replaced initialized-cell move assignment was nonthrowing.

Fields, globals, parameter references, proxies, different destination cells,
multiple-result uses, intervening instructions and coroutine frames are excluded.
This first slice intentionally does not infer broad alias/escape facts. It does
not optimize StringBuilder's field buffer or promise every `a = a + b` is fused.

## Before -> optimization -> after

The baseline was generated before editing production code (compiler SHA256
`18B94A00178E09CB9BC282E52C87372E7A82E577919225BCC0B6A033E47F5F29`).
Both sides use identical Wio workloads/probes/runtime and GCC 15.2 C++20 O2,
`-ffp-contract=off -fsized-deallocation`. An initial Store-only candidate did not
match actual compound-assignment Replace instructions; it was rejected and kept
locally as `rejected-no-fusion`, not used as evidence of optimization. The accepted
generated benchmark has two append-reuse sites, one each for string and text.

2000 runtime-selected repetitions, independent full-byte FNV checks:

| Diagnostic metric | String before | String after | Text before | Text after |
| --- | ---: | ---: | ---: | ---: |
| Allocation requests | 2002 | 14 | 4001 | 14 |
| Cumulative requested bytes | 90,047,102 | 184,389 | 251,878,140 | 172,101 |
| Peak live requested bytes | 180,059 | 138,344 | 419,808 | 129,125 |
| Live requested bytes with result retained | 90,103 | 92,263 | 168,016 | 86,116 |
| Final tracked live bytes/blocks | 0/0 | 0/0 | 0/0 | 0/0 |

These are separate replacement-allocator diagnostics, not plain runtime timings,
all payload copies, total heap usage or shipping RSS. String retained capacity
increases live requested bytes by 2160 (~2.4%), while reducing peak by ~23.2%.
Text live/peak falls ~48.7%/~69.2%. Geometric capacity reuse is the explicit
speed/memory tradeoff; unconditional shrink would reintroduce allocation/copying.

| Plain native median, 15 measured samples | Before | After |
| --- | ---: | ---: |
| String | 26.9197 ms | 0.2823 ms |
| Text | 51.0483 ms | 0.2694 ms |

**Overall timing is unqualified.** Before-string and its exact-binary control
have ~96% relative ranges; all samples are retained. Other lanes pass their coarse
range/drift gates, but this is not grounds to declare an overall speedup factor.
Durations include the append workload, hash and cleanup, not process startup.
Deterministic allocation/memory improvements above are independently checked.

The earlier pilot also failed timing gates. It was kept separately; the final
collection adds endpoint power-state qualification, not a repeat-until-pass run.
See [final raw evidence](evidence/n02-append-reuse-2026-10-16.json) and
[pilot raw evidence](evidence/n02-append-reuse-pilot-2026-10-16.json).

## New tests and limits

Only the two new CTests ran locally: selection-proof plus Wio native O0/O2 and VM
semantics. Empty/SSO/growth, NUL, combining/emoji/Japanese text, self-append,
snapshots, RHS mutation, borrowed references and field fallback are covered.
The final new-only CTest run passed 2/2 in 13.93 seconds; its Wio subprocess
record is [saved here](evidence/n02-append-semantics-2026-10-16.json).
No old corpus was run. Multi-platform CI is configured, not locally proven.

Two fixture assumptions were corrected during setup: compound assignment is
void, not an expression value; a normal borrowed-LHS binary expression across a
mutating call is not an explicit snapshot. The latter path was not fused. The
test now uses an explicit snapshot to assert preservation without silently
changing language evaluation semantics. Failed local fixtures were retained.

## Remaining N02 work

Broader concat chains, proven last-use transport, field/proxy-safe reuse and
remaining temporary copies need separate proof and before/after evidence.
Builder/formatting work stays N03. No wholesale borrow/lifecycle rule weakening,
automatic shrink policy or VM speedup is claimed here.

## Mandelbrot tracking

Native compute/full were rebuilt with current Wio and GCC O2; the Release VM host,
bytecode and bridge were explicitly reused unchanged. This optimization has no
selected append site in either Mandelbrot output. Full generated C++ differs from
the previous snapshot only in source paths. Compute now includes current implicit
std/assert declarations, so it is not a byte-identical compiler differential.

| Implementation | Compute median | Full PPM median | Date / scope |
| --- | ---: | ---: | --- |
| Wio native current | 24.3941 ms | 128.0974 ms | 2026-10-16, fresh native build |
| Wio Release VM | 23.1147291 s | 22.1652684 s | 2026-10-16, unchanged artifacts |
| Wio native previous control | not measured separately | 131.1940 ms | same session, frozen binary |
| C++ GCC 15.2 O2 | 28.4052 ms | 76.02 ms | fixed 2026-10-04 reference |
| C# .NET 9 Release | 29.5856 ms | 149.13 ms | fixed 2026-10-04 reference |
| CPython 3.12.14 | 1348.0412 ms | 1613.71 ms | fixed 2026-10-04 reference |

32/32 outputs correct. Numeric fingerprint is 11613930 / 3721766287256 / 94440;
PPM is 5,156,528 bytes, SHA256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.
Overall timing fails native-full/control range gates and native-full drift.
A/A compute median difference is 1.04% and passes its coarse gate. No cross-date
speedup or regression claim: these rows have different measurement scopes/dates.
Previous N01 copy-attribution tracking medians were 51.7406/278.5126 ms native and
42.6891409/52.4043856 s VM, also unqualified, with unchanged VM artifacts. That
large cross-session change is not a VM optimization in this patch.

[Raw tracking](evidence/mandelbrot-n02-append-2026-10-16.json) and
[native rebuild provenance](evidence/mandelbrot-n02-append-build-2026-10-16.json)
keep all inputs and samples. The reused collector has historical `Stage=N01`
and `Rebuilt=false` fields; the companion build record specifies what was rebuilt.

Use the [Wio workloads and Python controllers](../tests/performance/append_reuse/README.md).
