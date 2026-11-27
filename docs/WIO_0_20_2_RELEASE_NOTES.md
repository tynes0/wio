# Wio 0.20.2 Release Notes

Wio 0.20.2 builds VM executable projects from bytecode first and reuses a
precompiled standard-library native adapter when it covers the program's native
declarations. Ordinary Wio/std applications no longer need a project-specific
C++ translation unit, compiler invocation or native bridge library.

```sh
wio project build --backend vm
wio project run --backend vm
wio project package --backend vm
```

The default backend remains native C++; VM execution is still explicitly selected.

## Executable build selection

For executable projects without explicit native sources, link libraries, backend
arguments or extra native producers, the CLI first emits a verified `.wiob`
module. It then checks whether the toolchain's standard adapter covers the
concrete native declarations and required layouts. Open generic declarations
are not confused with missing callable specializations.

The check validates both the adapter registry and its companion bytecode ABI
catalog. Matching a C++ symbol/signature alone is insufficient: a declaration
from a different header must not silently bind to the standard implementation.
Uncatalogued native header/library imports also select a project adapter, even
when they declare no callable function (a header can contribute initialization).
Preflight does not invoke Wio entry functions, globals or native thunks. The
standard adapter itself is trusted executable toolchain code, not a sandbox.

When supported, the project packages bytecode with the existing VM runner and
the standard producer. Native-backed projects keep the existing project bridge
path. A new concrete native specialization, native component/object layout or
an uncatalogued structured native Any payload can require that project adapter,
including some generic std/native combinations. The CLI reports which path it
selects instead of reusing an incompatible producer.

## Reusable standard support

The toolchain builds `wio-std-native` once from the shipped std modules and
publishes it beside `wio-vm`, together with `wio-std-native.wiob` as the build-time
ABI catalog. Standard borrowed console scalar/string/text specializations are
seeded explicitly. Windows standard adapters and MinGW VM runners link their
compiler runtimes statically so deployed applications do not need a compiler's
DLL search path merely to start.

Console integral forwarding also handles platforms where `usize/isize` have
different C++ identities from fixed-width 64-bit types, including Darwin.

The shared adapter is deliberately conservative and monolithic. Deployment
still copies the selected native producer into each self-contained bundle;
adapter splitting and footprint optimization are future work, not claimed here.
The catalog is needed to build/select support, not to run an already-packaged
application.

Source-tree toolchain builds can refresh both files with:

```sh
cmake --build build --target wio_vm_std_native
```

Runtime-only builds keep their existing compiler-independent link graph. Building
the standard adapter from Wio sources requires the compiler at toolchain-build
time; an application consuming the resulting adapter does not regenerate it.

## Cache and deployment behavior

Bytecode emission has its own argument/input-content signature. Module roots are tracked
as Wio source files rather than recursively including the compiler's own
bytecode, executable and package outputs in directory timestamps. Imported Wio
source edits and input-list changes invalidate the bytecode cache, including
edits that preserve file timestamps. Cached bytecode is bound to its content
digest. Each invocation validates a private working snapshot and removes it on
completion; an unsupported preflight does not replace the previous public
bytecode before the native build succeeds. Package reuse compares embedded
bytecode and deployed producer bytes against the selected build inputs, not only
their timestamps. Native build
inputs retain their established validation and compilation behavior.

The executable records its actual producer, so switching back from a native
script to a standard-only script does not load a stale project bridge. Existing
instruction budgets, no-build verification, dependency checks, atomic publication
and deployment copying remain in force. Deployed images can run/package without
their original bytecode/build intermediates or the standard ABI catalog.

`project describe` reports `vm.standardNativeLibrary` and
`vm.standardNativeCatalog`. The lower-level tooling check is:

```sh
wio-vm --check-native program.wiob --standard-library path/to/wio-std-native.dll
```

Use the platform's `.so` or `.dylib` suffix outside Windows. Exit code 0 means the
shared producer covers the module, 3 means a project adapter is needed, and 1
reports invalid/missing module, catalog or producer data. This command is a build
preflight, not an application run.

## Compatibility and remaining work

Bytecode format and module ABI descriptor 11 are unchanged. Compiler, runtime,
CLI, SDK product metadata, VS Code package and web documentation align at 0.20.2;
editor behavior and the separate v1 SDK feature gate are unchanged.

This patch closes the standard-covered executable path, not the entire native
adapter-cache blocker. A native-backed project's Wio-only body edit can still
recompile its project bridge. ABI/dependency-closure cache separation, transitive
native-input invalidation and preservation of unchanged adapters belong to the
next narrow build-path patch. Shared/static VM library wrappers still use their
existing C++ host/ABI build path. Heavy VM/compiler optimization remains in its
approved later release families.

See [CLI reference](WIO_CLI_REFERENCE.md), [VM artifacts](WIO_VM_ARTIFACTS.md),
[release plan](WIO_V1_RELEASE_PLAN.md) and [execution policy](WIO_0_20_1_RELEASE_NOTES.md).
