# Native-pass high-sample measurement policy

2026-10-19. User-approved sampling update, not a new runtime optimization or
closure of N04/N14. Controller source `b14ea3e`, `wio-bench/1.1`; schema 1 remains
compatible with manifests without the new optional fields.

## Budget and collection

- Wio native/C++/C#: 1000 independent measured process runs per case; 10000 can
  be requested. Warmup processes and the native identical-command A/A lane are
  additional, never counted as primary samples.
- VM: 20 measured runs per case/phase by default; 10 is also a supported policy
  choice. VM runs in a separate block after the non-VM block in each phase.
- The final phase reruns Wio native/VM at their declared budgets. Core four-way
  workload coverage is still mandatory, and operation counts/checksums/flags and
  acceptance thresholds are not changed to accommodate a slow implementation.
- `samples.jsonl` appends every completed raw row. Full atomic `results.json`
  checkpoints default to every row, or every 100 in the high-sample profile;
  incorrect samples, phase ends and completion force full checkpoints. The
  journal avoids repeatedly serializing an ever-growing report after each run.
  It is not an automatic resume or power-loss durability guarantee.

`tools/wio-bench/sampling_profile.py` derives a new manifest without rebuilding
workload/reference binaries, records its source-manifest hash and preserves
relative path meanings. Wio performs production sample collection/statistics;
Python is preparation and independent contract validation only. Existing output
directories and old evidence are never overwritten.

## Local validation

The new focused contract checks legacy 32-row and custom 41-row schedules,
per-implementation comparison/final counts, full native A/A counts, VM isolation,
journal/report equality, rejection of ten malformed configurations, 1000/10000
profile validation, no overwrite and retention of all wrong-checksum samples.
Synthetic child durations are protocol fixtures, **not** performance data.
The final scoped gate passes 1/1 in 7.08 seconds (test body 6.96s).

The Unix borrow-fixture executable is renamed from `memory` to
`allocation-probe`: its include directory previously caused `<memory>` to read
the executable as a header on macOS. This explains the revealed native fixture
failure rather than a byte-cell optimization defect. Source `b14ea3e` passes
Windows Debug, Ubuntu Release and macOS 15 Release, including the new sampling
gate, in [CI 39305065595](https://github.com/tynes0/wio/actions/runs/39305065595).
Updated native O0/O2, supported
VM O0/O2 bridges, heap/alias/error/function-value controls pass locally in
113.13s. KI-009 indirect-native VM calls remain explicit known-gap negatives.

## First real session

The frozen two owning byte-I/O groups are selected for the first 1000/20 session:
64 KiB x 257 reads plus a full-byte FNV checksum per process. Expected total is
12096 process samples, including warmups, controls and final Wio reruns. It uses
the same native/VM/C++/C# workload images as the previous five-sample candidate;
only controller/protocol identities change. The native runtime/VM have not been
optimized in this update.

## Completed session

All **12096/12096** rows are correct, with exact per-lane budgets, the independent
full-byte FNV oracle, unchanged artifact hashes, applied affinity and unchanged
power profile. Collection takes approximately **61.18 minutes** including process
startup/setup, in-process warmups, native A/A controls and final reruns. These
costs are not part of the reported kernel medians. No trimming/retries occur.

Comparison-phase kernel medians, milliseconds:

| Implementation | Measured samples per case | Exact | Some |
| --- | ---: | ---: | ---: |
| Wio native | 1000 | 45.16115 | 44.34620 |
| C++ | 1000 | 46.11340 | 45.74710 |
| C# | 1000 | 33.70265 | 34.00725 |
| Wio VM | 20 | 2506.97750 | 2520.99595 |

Separate final phase: native medians **27.50345/27.53785ms** (1000 each), VM
**2512.78120/2548.52865ms** (20 each). Do **not** compare final native times
against comparison-phase C++ times to claim a speedup: the same unchanged native
image shifts substantially between phases. That demonstrates nonstationarity or
phase effects, not an optimization during the run. Exact causes such as clock,
scheduling, cache or other host load were not traced and remain hypotheses.

The comparison native/C++ median ratios are **0.9793x/0.9694x**, descriptive only.
Overall **timingQualified=false**. Comparison SDs are native
18.7386/14.7725ms, C++ 16.4722/16.5275ms and C# 11.8483/14.5368ms. Native
comparison p95 values are 81.3729/73.7673ms. Reference implementations also have
wide distributions: more repetitions do not remove environmental variability.

Native A/A median differences are **0.894%/1.049%** in comparison and
**0.225%/0.237%** in final. These central-value checks pass, but unchanged min/max
range limits still fail. The final exact VM case passes its own statistical
checks; overall session qualification remains false. Do not describe this as
every correctness test failing or as accepted 5-10% native overhead/parity.

The final native distributions are narrower but not flat: SD 3.6725/3.8631ms,
MAD 0.8914/0.8541ms, p95 35.7764/34.9503ms and p99 40.9756/42.2338ms. The
exact final sample minimum is 19.1222ms and maximum 64.8135ms; the reported
27.50345ms is the **median**, not the best case. Comparison peak process RSS
medians are native 9292KiB, C++ 5420KiB, C# 36996KiB, VM 24824/24812KiB.
This protocol update makes no new memory optimization claim; the previous owning
257-allocation and reusable zero-hot-window-allocation diagnostics remain separate.

## Evidence and independent verification

- [Compact verified summary](evidence/n04-high-sampling-summary-2026-10-19.json).
- [Full raw archive](evidence/n04-high-sampling-raw-2026-10-19.zip): complete
  results.json, samples.jsonl, generated Markdown, manifest, the frozen five
  fixture sources/input payload and controller/profile sources. No raw row is
  discarded. This is measurement evidence, not a hermetic toolchain package;
  executable/runtime identities are recorded, not embedded as binaries.
- [Archive entry hashes](evidence/n04-high-sampling-archive-2026-10-19.json),
  [independent verifier](evidence/n04-high-sampling-verify-2026-10-19.py),
  [platform CI metadata](evidence/n04-high-sampling-ci-2026-10-19.json).

The independent Python verifier agrees with all twelve Wio-produced summaries:
counts, min/max, mean/median, SD/SE, MAD, p90/p95/p99, drift, ranges and native A/A
differences. It also checks full journal/report equality, process status, phase
ordering, exact counts and independently recomputed checksum. Python does not
collect the production samples. To verify after extracting into a fresh folder:

```sh
python -m zipfile -e docs/evidence/n04-high-sampling-raw-2026-10-19.zip build/n04-sampling-extracted
python docs/evidence/n04-high-sampling-verify-2026-10-19.py build/n04-sampling-extracted
```

Archive size 677744 bytes; SHA256
`8af3bcadb80cdcbda99c67ea9d2ff71698e4fb436445f544643063ad7c5cb534`.

Higher sample counts reveal distributions; they do not remove scheduler, clock,
cache or correlated drift. Reports now distinguish each case's statistical gate
from whole-session qualification. Keep medians, MAD, SD, p95/p99, min/max and all
raw outliers; do not retry or relax limits until a session turns green. Ratios
against the old five-sample protocol are descriptive history, not a controlled
before/after optimization result.

Next N04 implementation scope remains bounded write prefixes/ranges and
bulk-buffer/stream operations. This measurement closure does not close N04,
start N05, optimize the VM or accept N14 performance budgets.
