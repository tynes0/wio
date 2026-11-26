# Measurement qualification correction — 2026-10-15

This N01 change fixes measurement acceptance, not production performance. There
is no compiler, generated-code, runtime, std or VM optimization in this update.
N02 has not started. The prior 2-3x swings must not be described as normal noise
or attributed to the instrumentation update.

## What changed

- The Mandelbrot build runner delegates to one controlled measurement runner.
- Existing native/bytecode/bridge/VM binaries can be remeasured without rebuilding.
- The dedicated controller is pinned before child creation; Windows children
  inherit affinity. The original controller mask is restored on exit/failure.
- All cases, including the VM, receive an explicit excluded warm-up. Measured
  case order rotates and reverses instead of always placing the VM/native first.
- An A/A lane executes the identical compute executable twice per round.
- Every output is checked; artifacts, runner and input provenance are hashed
  before/after. Failed and timed-out samples survive in the evidence file.
- Coarse range/drift/A/A acceptance gates return nonzero on instability. There
  is no sample trimming or rerun-until-pass policy.
- Endpoint power/OS/CPU information, child CPU/wall time and aggregate system
  busy fractions are retained. Missing CIM data remains null with its error.

The controller changes no system power scheme, priority, other application,
security or production VM instruction budget. It rejects unsupported processor
groups. It does not assume logical CPU 0 is the fastest core. The policy and
Windows affinity inheritance source are documented in the
[runner guide](../tests/performance/mandelbrot_compute/README.md).

The default 15% relative range and 10% chronological/A/A median gates detect
coarse instability; they are **not** confidence intervals or proof of 5% accuracy.
Passing one run does not establish comparability with historical sessions.
Pinning does not eliminate other applications, SMT sharing or frequency changes.

## Verification and remaining limitation

The new `wio_measurement_contract` passes locally: **1/1, 6.02 seconds**, with
38 assertions. It tests medians, completeness, invalid/NaN/infinite values,
range/drift rejection, balanced scheduling, artifact mutation, actual child
affinity inheritance/restoration and the real failed-process evidence path.
No old correctness corpus or production build was run locally.

The implementation is committed/pushed as `4718f82`. The three-platform
[CI run](https://github.com/tynes0/wio/actions/runs/38759859521) is fully green:
Windows Debug, Ubuntu Release and macOS Release all pass the new measurement
contract (3/3 executions). These are functional harness tests, not speed gates.

During the manual diagnostic session the observed aggregate CPU busy fraction
was around 65-70%. A separate brief read-only process sample (a one-second wait
plus process enumeration, not a precisely calibrated rate interval) showed an
active game process accumulating approximately 7.69 CPU-seconds across its
threads, with additional CPU used by its clients. No application was
terminated or modified. VM CPU/wall ratios were approximately 0.62-0.68.
These observations show competing activity; they do not isolate all timing loss,
prove thermal throttling or establish that the same load existed historically.
CPU accounting is especially coarse for the short native cases.

The harness repair is complete, but a quiet, stable baseline is **not** available
from this session. Do not use its timings to accept an optimization/regression,
close N01, or claim a current C++ performance ratio. Repeat a predeclared paired
experiment once normal quiet conditions are available, not until numbers look
attractive. Historical results remain intact.

The follow-up [native-cost controller pass](WIO_NATIVE_COST_CONTROLLER.md) migrates
the microbenchmark runner to the shared control helpers. Earlier diagnostic
timings remain explicitly unqualified for precise performance claims; their
allocation/fingerprint evidence is separate. This is still N01, not N02.

## Raw runs

The CPU 0 native smoke and CPU 2 development diagnostic are distinct experiments,
not a fastest-core selection. Both were rejected. The development run includes
VM observations while the controller was being hardened, so it is quarantined as
development evidence, not final-version qualification. The final native-only
run uses the committed controller and the same frozen binary artifacts.

| Run | Raw executions | Outcome / evidence |
| --- | ---: | --- |
| CPU 0 native smoke, development controller | 12 | Rejected; [raw](evidence/measurement-native-smoke-2026-10-15.json) |
| CPU 2 native + Release VM, development controller | 38 | Rejected; [quarantined raw](evidence/measurement-development-cpu2-2026-10-15.json) |
| CPU 2 final native controller | 40 | Rejected; [raw](evidence/measurement-final-native-cpu2-2026-10-15.json) |
| Final controller, intentional one-second VM timeout | 3 | Expected failure; [raw](evidence/measurement-vm-timeout-2026-10-15.json) |

All 90 executions in the first three experiments pass their fingerprints, but
**none of the experiments qualifies for performance comparison**. The timeout
exercise passes its two native warm-up fingerprints, preserves the VM timeout,
returns nonzero, verifies unchanged artifacts and leaves no VM child alive. Its
1.2931-second observed wall includes polling/termination and is not VM throughput.
All 93 raw rows are retained; no unfavorable run was replaced or trimmed.

In the final native experiment both binary and runner/provenance hashes are
unchanged, endpoint power scheme is unchanged, and A/A median difference is
4.34%. Nevertheless, within-case ranges correctly fail the experiment:

| Final native case | Median ms | Min-max ms / measured count |
| --- | ---: | --- |
| Compute | 59.7378 | 44.8046-76.3957 / 9 |
| Same-executable A/A compute | 62.3895 | 50.1038-87.7621 / 9 |
| Full current PPM | 411.0403 | 348.7278-714.8075 / 9 |
| Full previous saved C++ | 433.8804 | 348.1597-554.3797 / 9 |

Do not report the lower current/full median as a speedup: the noise gates reject
both variants. Native compute's relative range is 52.88%, full current's 89.06%.
No duplicate compile is involved in the A/A lanes.

Development VM compute median is 59,212.9412 ms (56,169.0438-59,772.6026, 3 measured
plus warm-up); full PPM is 60,977.8988 ms (57,683.3224-66,785.4161, 3 plus warm-up).
Individual coarse VM range gates passing does not qualify the overall session
or excuse competing system activity. These remain quarantined observations,
not a final-controller acceptance run. Another expensive full VM timing run was
not started under the already-observed competing load.

## Required dated-reference table

Milliseconds; different columns have different measurement boundaries. Current
native/VM rows are **rejected diagnostics**, not a controlled comparison against
the historical language references.

| Implementation | Compute-only kernel | Complete PPM wall |
| --- | ---: | ---: |
| C++ fixed 2026-10-04 | 28.4052 | 76.02 |
| Wio native final controller, 2026-10-15, rejected | 59.7378 | 411.0403 |
| Wio Release non-JIT VM development diagnostic, 2026-10-15, quarantined | 59,212.9412 | 60,977.8988 |
| C# fixed 2026-10-04 | 29.5856 | 149.13 |
| Python fixed 2026-10-04 | 1,348.0412 | 1,613.71 |

C++/C#/Python were not rerun. Same 800x800/100-iteration f64 fingerprints; GCC
15.2.0 `-O2 -ffp-contract=off`, .NET 9 Release, plain CPython 3.12.14, with
storage/formatting differences described in the
[original audit](WIO_MANDELBROT_PERFORMANCE_AUDIT.md). Compute excludes startup,
bridge/VM loading and output; PPM wall includes startup/loading, formatting and
file write/close, not compilation, validation hashing or durable disk sync.
PPM stays 5,156,528 bytes with SHA-256
`1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733`.

The preceding unstable N01 update had native compute/full medians
43.9924/187.3799 ms and VM 49,681.1540/61,027.9853 ms. The earlier stable-looking
session had 23.3227/133.6333 and 19,044.3468/20,595.9902 ms. These are retained as
historical observations, not valid before/after improvement percentages. The
current direct same-binary test strengthens the evidence that cross-session
timing variation cannot be attributed to a compiler change here.
