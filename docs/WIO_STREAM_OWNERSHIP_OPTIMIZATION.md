# N03 legacy stream-result ownership

2026-10-18, based on `970c03b`. This is a bounded compatibility-helper change,
not a switch from direct numeric formatting to streams. N03 remains open.

## Why not streams everywhere?

Normal finite/classic/nearest display formatting already uses direct `to_chars`
and appends to the destination. Host streams remain necessary for supported
custom locale/fenv behavior, nonfinite spellings, native operators and the
legacy compatibility API. A stream's existence is not itself a speed advantage.
The older float benchmark's C++ standard-stream reference is slower than its
Wio direct path, but is not an optimized C++ lower bound.

There are distinct APIs: ordinary Wio interpolation, indexed
`std::console::Format("{0}", ...)`, and native compatibility
`wio::runtime::Format("{}", ...)`. This slice changes only the last one.
Current WIR C++ generation does not route ordinary interpolation through it.
The fixture reaches it through an explicitly declared test-only native wrapper;
the VM lane calls that wrapper through its native bridge. No interpreter-core
optimization or new VM user-hook support is claimed.

## Production change and memory tradeoff

Once all formatting has completed, return `std::move(stream).str()` instead of
`stream.str()`. C++20 extraction returns an owning string, never a view into a
destroyed local stream. Formatting order, live locale/rounding, counted bytes,
partial-output state and native hook exceptions are unchanged. The stream is
not reused after extraction. No ABI, WIR, bytecode, public signature or std API
changes are introduced.

The transferred buffer can retain spare capacity. This is not a universal
retained-memory improvement or permission to steal buffers from live/reused
streams. In the GCC O2 standalone diagnostic, a 216-byte result has capacity
216 before and 512 after. MSVC Debug has capacity 223 before and 255 after.
Long-lived collections of results can retain more memory even when transient
allocation/peak traffic falls. No shrink-to-fit loop is inserted to conceal that
tradeoff by adding allocations back.

## Direct allocation diagnostic

128 calls with a 192-byte input payload, in an isolated replacement-new/delete
window. Each completed result is destroyed inside the window. The after image
also runs the owning-copy control in the same process; the original before
image was run with `--observe` before the production edit.

| Metric | GCC O2 copy | GCC O2 extraction | MSVC Debug copy | MSVC Debug extraction |
| --- | ---: | ---: | ---: | ---: |
| Allocations | 256 | 128 | 1152 | 1024 |
| Requested bytes | 93586 | 65664 | 98304 | 69632 |
| Peak live bytes | 732 | 513 | 496 | 384 |
| Final live bytes | 0 | 0 | 0 | 0 |

The platform-specific absolute counts are not interchangeable. These are not
RSS, all malloc traffic, copy-byte counts, production timing or VM heap metrics.
The MinGW unit statically links libstdc++/libgcc so streams and replacement
allocation operators share one runtime image.

## Validation and retained setup failures

Only the two new local gates ran: **2/2 passed in 13.65 seconds**. The standalone
unit covers SSO/growth boundaries, counted NUL, seek/high-water output, badbit
partial output, containers, stateful ToString ordering, native operator
exceptions, custom grouping and rounding modes against the existing copy
implementation. Wio tests run generated native O0/O2 and the VM native bridge,
covering NUL/Unicode, integer boundaries and independent owning snapshots.
The GCC O2 standalone unit also passes with strict warnings.

Two setup failures are retained rather than substituted with performance data:

1. The first C++ reference included the native wrapper's runtime header without
   an include path. The fixed reference excludes that wrapper and has no Wio
   runtime formatting dependency. It retains the explicit stream-copy reference.
2. The first allocation diagnostic mixed a dynamic MinGW C++ runtime's stream
   allocation with the executable's replacement delete, causing fast-fail
   `3221226505`. Ordinary production binaries worked. The diagnostic is now
   explicitly static-libstdc++/libgcc on Windows. Its rebuild command is appended
   to the original build record; ordinary native/C++/C#/bridge artifacts were
   not changed by that repair. No timing session existed before either repair.

The shared Python fixture supports explicit `--reuse-build
--rebuild-diagnostics` for that separate executable, retaining/appending its build
commands. Default builds remain unchanged; this flag cannot be used as evidence
that changed production artifacts were rebuilt. The final candidate uses a fresh
ordinary build. This is not retry-to-pass timing, trimming or environment tuning.

## Measurement

Two workloads use 192/8192-byte counted ASCII/NUL payloads with runtime decimal
indices and lowercase boolean suffixes. Each owning result is fully FNV-hashed;
input construction, formatting, hash and local cleanup are included in the
kernel. Wio native/VM, C++ and C# use 2001 operations and two in-process warmups.
Python prepares builds and computes independent checksums. Wio `wio-bench`
collects rotating/reversing comparison rounds, exact-command native A/A and a
separate final native/VM phase. C++ keeps a frozen owning-copy stream reference;
it is not an optimized rvalue-extraction lower bound. C# uses UTF-16 builders and
an allocating UTF-8 hash; these costs are not identical native storage contracts.

All five frozen workload source files are byte-identical before/after. Generated
C++ is identical after normalizing only the output-folder name. The Debug
compiler, Release VM host, Wio controller and native runtime archive stay
unchanged; the generated native image and VM native bridge are rebuilt with the
changed compatibility header. Declared hashes are not a clean/full-transitive
repository claim: existing user std/playground edits remain untouched.

Comparison medians, before -> after, milliseconds:

| Payload bytes | Wio native | Wio VM + bridge | C++ copy stream | C# builder |
| --- | ---: | ---: | ---: | ---: |
| 192 | 0.8997 -> 0.8428 | 15.4058 -> 16.0595 | 0.8065 -> 0.8122 | 2.2169 -> 2.3934 |
| 8192 | 20.0640 -> 20.1076 | 45.3303 -> 46.5114 | 20.2906 -> 20.3552 | 24.4756 -> 24.7931 |

Separate final native/VM phase, milliseconds:

| Payload bytes | Native before -> after | VM before -> after |
| --- | ---: | ---: |
| 192 | 0.8994 -> 0.8420 | 15.2746 -> 15.9061 |
| 8192 | 20.1668 -> 19.3448 | 46.8945 -> 46.3173 |

Both complete sessions have **84/84 correct outputs**, 168/168 together,
applied affinity, unchanged endpoint artifacts and unchanged endpoint power
profiles. Both have **timingQualified=false**. After comparison ranges include
0.794 for large native, 0.216 for small VM and 0.521/0.742 for C#. Native A/A
comparison differences are 0.017/0.033 and final differences 0.001/0.045. All raw
samples, including failures of timing qualification, remain; there is no trimming,
retry-to-pass, power/priority/security change or unrelated process termination.
Heavy candidate builds and timing collection are separate. An initial short
standalone diagnostic compile overlapped baseline build preparation, not sampling.

The small native observation falls in both phases; large native comparison rises
slightly while its final observation falls. VM comparison observations rise;
the small final VM observation rises too. These are retained observations, not
accepted speedup/regression percentages. Runner exit 1 rejects timing
qualification, not workload correctness. No VM-core gain or native budget closure
is inferred. This is a new legacy-helper workload; previous float/hook workloads
cannot be used as a historical paired denominator.

## Wio workload memory and artifact tradeoffs

Separate static-C++-runtime allocation diagnostics, 2001 operations, including
payload construction, formatted owning results and cleanup:

| Payload bytes | Allocations = frees, before -> after | Requested = freed bytes, before -> after | Peak live bytes, before -> after |
| --- | ---: | ---: | ---: |
| 192 | 4004 -> 2003 | 1466872 -> 1026762 | 983 -> 762 |
| 8192 | 14009 -> 12008 | 81012621 -> 64564511 | 32855 -> 32827 |

All end at zero tracked live blocks/bytes. The large case still pays stream growth
and holds the payload; eliminating its final result copy does not eliminate
those costs. Ordinary timing executables do not use replacement allocation
operators. VM native bridge/session allocations and C# managed allocation bytes
are not these counters. The returned-capacity tradeoff above remains real even
when this immediate-consumption workload's transient peak falls.

Native process peak RSS comparison medians are 9276/9568 -> 9264/9488 KiB. These
small whole-process shifts do not isolate a retained-memory win. After VM/C++/C#
RSS medians are 17876/5252/34264 KiB for the small case and
18180/5564/36864 KiB for the large case.

The production native executable grows **6377353 -> 6378649 bytes (+1296)**;
the separate diagnostic stays 9523398 bytes. Generated C++ size
3255947 -> 3255943 differs only by path provenance; normalized content is equal.
This slice is not a generated-code-size or whole-process-memory optimization.

## Evidence

- [Before samples](evidence/n03-stream-own-before-2026-10-18.json),
  [after samples](evidence/n03-stream-own-after-2026-10-18.json).
- [Before build and appended diagnostic repair](evidence/n03-stream-own-build-before-2026-10-18.json),
  [fresh after build](evidence/n03-stream-own-build-after-2026-10-18.json).
- [Before heap](evidence/n03-stream-own-memory-before-2026-10-18.json),
  [after heap](evidence/n03-stream-own-memory-after-2026-10-18.json).
- [Original standalone probe](evidence/n03-stream-own-probe-before-2026-10-18.jsonl),
  [after same-image control/probe](evidence/n03-stream-own-probe-after-2026-10-18.jsonl).
- [CTest/MSVC diagnostic](evidence/n03-stream-own-tests-2026-10-18.xml),
  [Wio native/VM processes](evidence/n03-stream-own-validation-2026-10-18.json).
- [Frozen-source equality and artifact hashes](evidence/n03-stream-own-artifacts-2026-10-18.json).
- [Initial reference include failure](evidence/n03-stream-own-initial-build-failure-2026-10-18.json),
  [initial mixed-runtime diagnostic failure](evidence/n03-stream-own-initial-memory-failure-2026-10-18.json).

The preceding `970c03b` commit's Windows Debug, Ubuntu Release and macOS Release
CI passed. This change needs its own platform qualification.

## Scope remaining

The native generated generic stream fallback is unchanged. Richer formatting,
remaining KI-008 callable/continuation surfaces, KI-007/KI-009, and the fixed later
N04-N14 items remain explicit. N04 has not started. This slice cannot close the
general 1.05-1.10x native budget or establish a universal memory improvement.
