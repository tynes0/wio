# Wio 0.20.4 native dependency records

Wio 0.20.4 records the native inputs observed when building a project-specific
VM adapter. The record connects the requirements identity from 0.20.3 to file
contents, preprocessing, linker inputs and toolchain context. Adapter cache reuse
remains disabled; safe reuse is the separate 0.20.5 patch.

## Inspect a project build

```sh
wio project build --backend vm
wio project describe
```

The description exposes `vm.nativeDependencies`, beside `vm.nativeRequirements`.
A successful bridge compilation writes `<bridge>.dependencies.json`. A failed
native compilation preserves the previous report. Standard-covered executable
projects still use the precompiled standard producer without building a project
adapter or running these probes. Analysis-only checks do not invoke them either.

Direct bridge compilation accepts an explicit destination:

```sh
wio main.wio --target shared --output app-native.dll \
  --native-bridge-only --bytecode-sidecar app.wiob \
  --native-requirements-output app.requirements.json \
  --native-dependencies-output app.dependencies.json
```

Use the platform's shared-library suffix on Linux and macOS. Dependency capture
requires an actual canonical native bridge compilation, not bytecode-only or
analysis-only emission. Report destinations cannot overwrite observed native
inputs, the Wio entry, compiler, or selected backend outputs.

## Recorded inputs

The schema-version-1 report contains SHA-256 fingerprints for the generated
translation unit, extra native sources, transitively included user and system
headers, explicit native libraries, resolved linker inputs, the Wio generator,
compiler and discoverable toolchain executables. It records ordered compilation
and link options, target, product version, working directory and compiler query
hashes. The environment map stores hashes rather than raw values. Actual input
paths and explicitly supplied command-line options remain visible in the record.

Preprocessor-only probes include system headers and hash the preprocessed
translation units. They use compiler-produced Make dependencies rather than a
textual `#include` scanner. See the
[GCC preprocessing options](https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html).
Toolchain queries use the compiler's
[driver inspection options](https://gcc.gnu.org/onlinedocs/gcc/Developer-Options.html).

Where supported, the actual link writes a GNU
[linker dependency file](https://sourceware.org/binutils/docs/ld/Options.html).
On Apple toolchains the reader supports the input, missing-input and output
records in ld64's dependency-info format, defined by Apple's
[option declarations](https://github.com/apple-oss-distributions/ld64/blob/main/src/ld/Options.h)
and [writer](https://github.com/apple-oss-distributions/ld64/blob/main/src/ld/Options.cpp).
Unsupported capture formats remain explicit incomplete observations rather than
making an otherwise supported build depend on an unavailable diagnostic option.

## Validity boundary

Every report declares `cacheReuseEnabled: false`, `hermeticBuild: false`,
`requiresFreshPreprocessing: true` and `requiresFreshLinkResolution: true`.
The file list alone cannot establish cache validity: a previously absent
`__has_include` file, a higher-priority library or a changed search environment
can affect the next build without changing an old dependency's contents.

`knownInputsStable` compares files observed before compilation with their state
after compilation. Linker inputs discovered only after the link are observations,
not proof of an immutable filesystem snapshot. `artifactSha256` binds the record
to the produced library; `requirementsSha256` identifies its WIR bridge contract.

`observationComplete` is false when a probe fails, a format or option is not
understood, an input cannot be read, a bounded capture limit is exceeded, or a
known input changes during compilation. `incompleteReasons` explains why. GCC
may delete temporary object files before the linker record is inspected; those
unavailable inputs are also reported as incomplete, never silently trusted.
Unknown compile options are not forwarded into extra preprocessing probes.
Reports use bounded reads, not recursive hashing of toolchain directories.

This patch adds observation work to project-specific adapter rebuilds; it is
not a build-speed improvement. Patch 0.20.5 must validate fresh context, handle
incomplete records conservatively and publish/cache the adapter cohort safely.
Multi-file report/bytecode/library publication is not yet a cache transaction.

Product metadata aligns at 0.20.4. Module ABI descriptor 11, bytecode format 1.1,
default native execution and the separate SDK feature-parity gate are unchanged.
