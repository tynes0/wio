# N01 live/peak memory attribution

The language workload is Wio; orchestration and independent checksums are Python.
This is a separate diagnostic executable, not a performance benchmark or a
production allocator. Do not link `probe.h` into a shared library, production
program, multiple translation units, or the older allocation/timing probes.
Compiler/runtime source and the common timing controller are unchanged.

```sh
python tests/performance/native_costs/memory/run.py --output build/memory-fresh \
  --wio build/app/wio --runtime build/runtime/backend/libwio_runtime.a
```

For Windows multi-config builds, `--wio build/app/Debug/wio.exe` is the default.
Select `--cxx` for the configured backend toolchain. GCC/Clang C++20 is required;
`-fsized-deallocation` explicitly enables the sized-delete control surface on
Clang. The output directory must be new. Failed cases, raw stdout/stderr and
source/runtime/toolchain/artifact hashes are retained. No timing thresholds,
trimming, retry-until-pass, or user-system setting changes.

## Two different memory measurements

1. The diagnostic replaces ordinary/aligned, scalar/array, throwing/nothrow
   global C++ new/delete. Each successful allocation records its original
   requested size and whether it belongs to the active window. This provides
   cumulative requested/freed bytes, live/peak requested bytes and block counts.
   A free of pre-window storage is not subtracted from the window. Zero-byte
   allocations are still blocks; failed allocations are not successful blocks.
2. Fixed checkpoints sample the process's current resident memory and its
   process-lifetime peak using platform APIs. Windows additionally reports
   private commit. These include startup, runtime, library mappings, allocator
   metadata/padding/caching and diagnostic overhead, not just Wio values.

Neither is a measurement of copied bytes. Requested bytes exclude the probe's
prefix/alignment padding and allocator metadata. C malloc/realloc/free,
custom allocators, mmap/VirtualAlloc and allocations performed wholly inside
external libraries are not intercepted. Such memory may still affect OS metrics.
The probe changes layout/allocation behavior and serializes bookkeeping; **do
not use its elapsed times or RSS as uninstrumented production results**.

The process metrics have explicit sources:

- Windows: current/peak working set and current private commit from
  [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).
- Linux: `/proc/self/status` VmRSS/VmHWM in KiB converted to bytes. The kernel's
  [documented approximation](https://man7.org/linux/man-pages/man5/proc_pid_status.5.html)
  means these are observations, not precise allocation-accounting invariants.
- macOS: MACH_TASK_BASIC_INFO resident_size/resident_size_max, whose
  [XNU header declares byte units](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/task_info.h).
  Private commit is not measured on Linux/macOS and is null, not zero.
- Unavailable/unsupported OS observations are explicitly null. Resident memory
  is sampled at three points, not continuously. Lifetime peak is **not** a peak
  reset at Begin. Releasing blocks need not return pages to the operating system.

## Cases and checks

Each case runs in a fresh process at counts 1, 17 and 2000:

- Retained Wio object array, owning long-string array and byte array. A live
  checkpoint occurs while the array is retained, then Wio returns and performs
  scope cleanup before the released checkpoint.
- Native weak control called from Wio: allocation remains after the final
  strong handle is released and is physically freed after final weak release.
- Native ordinary-allocation control: two touched allocations, exactly
  `192 * count + 4` requested/live/peak bytes and two blocks at the live point.
- Native aligned/edge control: six live allocations, three aligned, exactly
  `count + 8499` requested bytes; checks 64/128/4096-byte alignment, zero-byte
  allocation, throwing/nothrow overflow failure and several delete forms. Its
  two SIZE_MAX requests are rejected by checked size arithmetic before malloc;
  this is not an attempt to exhaust system memory.

Begin creates an untracked allocation; the live checkpoint frees it to verify
cross-window accounting. All checkpoints enforce `requested - freed == live`
and `allocations - frees == live blocks`; the final checkpoint requires zero
window-owned blocks/bytes. Peak counters remain after release. Known controls
validate exact counts/peaks independently; Wio checksums are computed in Python.
This is evidence for these fixtures, not a whole-program leak detector or proof
about worker-thread races. There is no malloc-failure/new-handler fault injection.

`wio_native_memory_attribution` is a separate CTest: five checks including
18 real Wio executions. It is registered for the Windows/Linux/macOS CI matrix,
without machine-speed gates. Local evidence must not be presented as confirmed
cross-platform CI until those jobs finish.
