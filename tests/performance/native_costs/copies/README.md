# N01 observed bulk-copy attribution

Wio behavior fixtures, Python orchestration, GCC/GNU-compatible linker diagnostic
only. Not a production profiler or a throughput benchmark. Never link this probe
into production, shared libraries or the allocation/RC/memory probes.

```sh
python tests/performance/native_costs/copies/run.py --output build/copies-fresh \
  --wio build/app/wio --runtime build/runtime/backend/libwio_runtime.a
```

Windows multi-config default: `--wio build/app/Debug/wio.exe`. The output folder
must be new. Compiler/runtime/generated/binary/fixture hashes must not change.
Failed output, raw samples and both compilation results remain in the folder.
Windows/Linux GCC CI runs `wio_native_copy_attribution`; the macOS Clang linker
is not claimed supported. No new PowerShell or production-code edits.
The current feature-branch push runs only this new test on supported runners;
main/release/PR functional and earlier diagnostic gates remain in place. The
macOS feature job still builds, but does not pretend to run GCC copy attribution.

## Precisely what is measured

The diagnostic links `--wrap=memcpy` and `--wrap=memmove`. Wrappers record the
call count and sum of the byte-length arguments, then forward to the actual C
implementation. The counters are active only around the Wio kernel, its normal
scope cleanup and final source-integrity checks. Source payload construction,
argument parsing and case selection happen before Begin; case selection uses
an integer inside the measured kernel to avoid copying comparison literals.

Common flags are C++20 `-O2 -static-libstdc++ -static-libgcc`. Only the diagnostic
adds `-fno-inline -fno-builtin -fno-ipa-cp -DWIO_COPY_DIAGNOSTIC` and linker wraps.
Static C++ libraries expose more link-time references to the wrapper, but **not
all possible copies**. The normal variant keeps inlining/builtins enabled and
does not wrap or count anything. Its metrics are null, not fabricated zeros.

The GNU linker's [documented --wrap boundary](https://sourceware.org/binutils/docs/ld/Options.html)
is unresolved symbol references: internal/resolved references are not necessarily
wrapped. These counters therefore do **not** include all:

- scalar/register/SIMD copies, compiler-inlined copies or trivial component fields;
- shared-library-internal copies, custom copy loops, every compiler builtin,
  allocator relocation, or memory traffic implemented without these C calls;
- physical memory-bus/cache-line/store bytes. These are logical byte ranges of
  observed copy requests, not measured hardware traffic;
- language-level payload alone: requests may include string terminators and
  other runtime representation bytes. Do not silently subtract bytes to fit a
  desired count or use allocation-request sizes as copied bytes.

Zero observed bytes does not mean no copying occurred. This is especially
important for scalar-only components. Neither diagnostic counts nor durations
are claimed to match the fully optimized shipping executable. Ordinary O2
checksum parity proves output equivalence for these fixtures, not count parity.
The linked runtime archive is not rebuilt under the diagnostic flags.

## Workloads and validation

Thirteen cases at counts 1, 17, 2000; both diagnostic and ordinary O2 binaries:

- Owning argument / borrowed view / owning return for byte string, Unicode text
  and a component containing a long string plus a numeric field.
- Repeated string and text `+=`, plus scalar-only component construction/return.
- Native calibration invoked by Wio, using volatile function-pointer calls to
  guarantee two memcpy and two memmove calls per iteration, including a zero-
  length copy, both overlap directions and returned-pointer validation. Known
  byte totals are 7 memcpy + 16 memmove bytes per iteration. Independent Python
  modeling verifies the resulting buffer contents.

The string payload includes an embedded NUL and is 45 bytes; the Unicode text
contains emoji, a combining mark and Japanese characters and is 42 UTF-8 bytes.
Each source is hashed before the window and verified unchanged afterward,
including the component field. Python independently computes wrapping-u64
fingerprints; both binaries must pass. The test does not require workload copy
counts to remain high or freeze them as a language contract. Only the explicit
calibration's known C-call counts are exact gates.

`wio_native_copy_attribution`: five Python checks including **78 real Wio
executions**. Protocol tests reject incorrect calibration, wrong fingerprints,
mode changes, missing/null/negative/overflow/non-integer counters, duplicate
fields and non-finite JSON. No machine-speed thresholds or retry-until-pass.
