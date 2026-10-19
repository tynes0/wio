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
optimized in this update. Results are pending, not an accepted speedup.

Higher sample counts reveal distributions; they do not remove scheduler, clock,
cache or correlated drift. Reports now distinguish each case's statistical gate
from whole-session qualification. Keep medians, MAD, SD, p95/p99, min/max and all
raw outliers; do not retry or relax limits until a session turns green. Ratios
against the old five-sample protocol are descriptive history, not a controlled
before/after optimization result.
