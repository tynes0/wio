# Wio VM Executable Artifacts

Status: executable and project-command slices implemented; VM-backed
shared/static **Wio module** artifacts remain open. Locally qualified on Windows;
Linux/macOS deployment is not claimed by the Windows gates.

## Runtime-only runner

`wio-vm` depends on `wio_vm`, `wio_bytecode_core`, the SDK wire headers and host
argument/process primitives. It does not link the compiler, analyzer, parser,
WIR lowering, C++ backend or self-hosted CLI. CMake checks that dependency closure.
The bytecode core owns decoding, verification, wire metadata and disassembly;
the compiler keeps WIR conversion and semantic enum encoders.

Build the runner without building the compiler target:

```powershell
cmake --build build --target wio_vm_runner --config Release
```

Run an existing module, forwarding application arguments after `--`:

```powershell
build/app/Release/wio-vm.exe program.wiob -- hello "two words"
```

Native registry producers can be supplied with repeated `--native-library`:

```powershell
build/app/Release/wio-vm.exe program.wiob --native-library bridge.dll -- hello
```

Decoding and Machine load verify the bytecode before execution. `Entry`, `main`
and recorded application entry points use the existing VM runner rules. In raw
runner mode `args[0]` is the module path; in an embedded application it is the
actual executable path. Integer Wio entry results become process exit codes.

## Embedded executable

For a pure Wio program:

```powershell
wio program.wio --emit-bytecode --bytecode-output program.wiob
wio-vm --package program.wiob --output dist/program.exe
dist/program.exe hello
```

Use the built runner path above if `wio-vm` is not on PATH. On Linux/macOS omit
the `.exe` suffix and use the platform's runner. Packages use the runner's OS,
architecture, VM implementation and host runtime; this is not cross-compilation.

The executable contains the runner, bytecode and deployment metadata. A pure
package needs no external `.wiob`, Wio source, std sources, compiler executable,
CLI companion or `WIO_ROOT` at runtime. Platform runtime dependencies still
follow how the runner was built; this is not a universal dependency-free binary.
Packaging preserves executable permissions.

Every argument to the embedded application is a Wio argument, including
`--help`, `--package`, `--` and option-shaped values. Embedded programs do not
fall through to the runner's tool commands. Windows arguments and executable
paths use UTF-8 rather than the local ANSI code page.

## Native code deployment

Build a qualified native registry producer and bytecode sidecar:

```powershell
wio program.wio --target shared --output build/bridge.dll `
    --native-bridge-only --bytecode-sidecar build/program.wiob
wio-vm --package build/program.wiob --output dist/program.exe `
    --native-library build/bridge.dll
```

`dist/` contains `program.exe` and `bridge.dll`. The bytecode is embedded; the
producer stays a real native library. Library basenames are recorded in UTF-8
and resolved relative to the executable, **not the current working directory**.
Spaces and Unicode filenames/arguments are covered by the Windows gate.

Only explicitly listed producer libraries are copied. Their transitive DLL/SO
dependencies, assets, platform runtimes, signing and installers are not discovered
or bundled automatically. Deploy them according to the platform loader's rules.
A missing library fails before Wio entry execution. Native code is privileged;
packaging does not sandbox it or relax the native ABI qualification boundaries.

## Publication and corruption guards

- By default the output must not exist. There is no arbitrary `--force`.
  `--replace` only replaces an existing decoded/verified VM executable artifact,
  never an ordinary executable, unknown file or corrupt recognized artifact.
- Existing adjacent libraries are reused only if byte-identical. Conflicting
  libraries, duplicate basenames and executable/library name collisions reject.
- Cooperating packagers serialize publication/rollback through a deployment
  directory lock. An active or interrupted invocation fails explicitly instead
  of removing a library another package may use. After a crash, inspect the
  directory and remove the empty `.wio-vm-package.lock` only when no packager is
  running. That name is reserved for the lock, not executable/library output.
- Library metadata permits only bounded, valid UTF-8 basenames without path
  traversal or separators. There are at most 64 names, each at most 1,024 bytes.
- The module must decode and pass the bytecode verifier before publication.
  Executable packaging requires a concrete runnable entry with zero parameters
  or one `string[]` parameter; library-only modules are not executable packages.
- A complete temporary executable is published without replacement: an exclusive
  Windows move or a POSIX hard link. POSIX output filesystems must support hard
  links. These operations never expose a partially copied executable target.
- Publication failure makes a best-effort cleanup of the known temporary file
  and libraries created by this invocation. Existing files are not removed.
- A recognized but malformed footer fails; it cannot fall back to tooling.
  Payload size/version/reserved bits, metadata lengths, bytecode length and
  checksum are checked before execution. Bytecode verification follows decoding.

The version-1 footer is 32 bytes: `WIOVMPKG` magic, little-endian u64 payload
length, u64 FNV-1a checksum, u32 version and u32 reserved zero. The payload stores
u64 bytecode length, u32 library count, length-prefixed UTF-8 library basenames,
then raw `.wiob` bytes. Payloads are bounded to 512 MiB + 64 KiB; the bytecode
decoder independently enforces its 512 MiB file limit.

FNV-1a detects accidental corruption; it is **not** a cryptographic signature,
authentication or protection against a malicious producer. A modified bytecode
program with a recalculated checksum is still untrusted executable input.

## Safe rebuilds and project commands

```powershell
wio project build --backend vm
wio project run --backend vm --arg "two words" -- --help
wio project package --backend vm --output-dir dist
```

Build/run/package keep manifest resolution in the Wio CLI. The default deployed
executable is `<outputDirectory>/vm/<wioName>.exe` (no suffix on Unix); override
it with `vm.executableOutput`. The C++ backend's normal executable is untouched.
See [project configuration](WIO_PROJECT_SYSTEM.md#49-output-paths) for details.

The CLI packages with `--replace --version-libraries`. The latter inserts a
content identity before each native library extension, for example
`bridge.wio-0123456789abcdef.dll`. Changed libraries therefore cannot overwrite
a previous image's dependencies. Existing versioned files are reused only after
byte comparison. File changes during copy reject publication. Old generations
remain on disk to protect running/retained artifacts; they are not automatically
garbage-collected. The content identity is FNV-1a, not a cryptographic trust claim.
Missing deployed libraries fail `--no-build` verification. A normal build/run
can repair them from current producer inputs without altering an existing
library's bytes; corrupt or non-VM images still reject replacement.

`--replace` publishes the new image atomically under the deployment lock. On
Windows a running/locked executable can prevent replacement; failure preserves
the old image. The caller must stop it and retry, not delete it as a workaround.
Cooperating packagers are serialized; arbitrary external filesystem writers are
not an authenticated transaction boundary.

`wio-vm --verify APP` checks the embedded module and required library files.
It does not execute the program or establish native-library ABI/authenticity;
registry/adapter validation still happens when the VM loads each library.

`wio-vm --deploy APP --output NEW_APP` reads verified embedded bytecode and copies
only its recorded libraries, using the current runner template. No source or
external `.wiob` is required. The destination must be fresh; replacement flags
and arbitrary additional libraries are not accepted by this deployment command.
Project package uses it so old generation files do not leak into the distribution.
Native transitive dependency discovery/signing remains outside its scope.

## Installed embedding surface and remaining work

Installation includes `wio-vm`, the `wio_vm` and `wio_bytecode_core` archives,
and their runtime-only headers. C++ embedders must use a matching compiler,
architecture, configuration and C++ runtime. Direct Machine users keep the
immutable decoded module alive; ModuleAdapter owns its module and machine.

The `WioVmRuntime` install component installs the runner, archives, runtime
headers, SDK headers and a relocatable CMake package without the compiler:

```powershell
cmake --install build --config Release --component WioVmRuntime --prefix vm-runtime
```

Use `find_package(WioVmRuntime CONFIG REQUIRED)` with the installation prefix
in `CMAKE_PREFIX_PATH`. Imported targets `Wio::wio_vm`,
`Wio::wio_bytecode_core` and `Wio::wio_sdk` carry the required include and
thread/dynamic-loader dependencies. MSVC consumers must match the archive's
static/dynamic CRT and Debug/Release configuration; an MSVC runtime archive
cannot be linked into a MinGW wrapper merely because both use a C-shaped ABI.

## VM-backed SDK shared and static modules

The initial module adapter publishes the existing stable SDK contract and
canonical native value ABI v2, without linking the Wio compiler or changing
bytecode/SDK descriptor versions. Emit **library** bytecode first:

```powershell
wio script.wio --target shared --emit-bytecode --bytecode-output script.wiob
```

Package it with the installed runtime's matching C++ toolchain:

```cmake
find_package(WioVmRuntime CONFIG REQUIRED)
wio_add_vm_module(game SHARED BYTECODE "${CMAKE_CURRENT_SOURCE_DIR}/script.wiob")
wio_add_vm_module(game_static STATIC BYTECODE "${CMAKE_CURRENT_SOURCE_DIR}/script.wiob"
    GETTER WioGetStaticGameContract)
```

The helper embeds bytecode, producing a real `.dll/.so` (or platform shared
library suffix) or `.lib/.a`. It defaults to SHARED; static modules require
unique C getter names so multiple Wio libraries can coexist in one host. Each
wrapper owns an independent VM instance. It does not depend on a separate
`.wiob`, WIO_ROOT, sources or the compiler at runtime. CMake bytecode inputs
can also be generated by a dependency/custom command.

Load shared modules with the canonical SDK, **not** the legacy `sdk::Module`:

```cpp
#include <wio_native_sdk.h>
#include <array>

auto game = wio::sdk::NativeModule::open("game.dll");
std::array args{wio::sdk::NativeValue::integer(17),
                wio::sdk::NativeValue::integer(25)};
auto sum = game.invoke("Add", args).asInteger();
```

Static getters return the same contract. `NativeModule::attach(contract,
owner)` requires an explicit lifetime owner; for statically linked generated
wrappers, a host-owned process-lifetime token is sufficient because the
descriptor and code have static storage. Embedders needing scoped instances
can instead attach an owning adapter:

```cpp
#include <wio/vm/module_adapter.h>

auto adapter = std::make_shared<wio::vm::ModuleAdapter>(std::move(decoded.module));
auto game = wio::sdk::NativeModule::attach(adapter->contract(), adapter);
// adapter->loadNativeLibrary(path, &error) registers a qualified producer
// before invocation when embedding native calls manually.
```

The qualified export surface is synchronous concrete functions, concrete
generic function specializations, void, bool, numeric/byte/char values,
enum/flagset underlying scalars, byte-exact string and Unicode text. Signature,
range, stable IDs and call slots are checked. The contract also carries basic
reflection identities; full native reflection/attribute/accessor sidecars are
not published by this adapter yet. No legacy raw C++ function addresses or
`WioModuleGetApi` compatibility table are emitted by these wrappers.

Calls/registration belong to the constructing thread (the first successful
getter thread for generated wrappers). Foreign calls return WRONG_THREAD;
reentrant calls reject. VM execution failure returns PANIC, conversion errors
return TYPE_MISMATCH, and exceptions are contained at the wire boundary.
Direct embedders can inspect `ModuleAdapter::lastError()` on the owning thread.
Raw result slots must be empty; arguments are copied, never consumed. String
and text results own their storage. SDK values pin module/adapter lifetime
through close and cloning; raw ABI callers must retain producer **code** until
the final owner release, even when result storage outlives the adapter.

This is deliberately a bounded module-hosting surface. Ref/view, nullable,
callback, object/component, container/Any/Box and async exports reject during
adapter initialization. Lifecycle/save-state/application hosting also reject;
async helper work anywhere in the module is refused until explicit host
sessions/shutdown are available (workers cannot be joined under a Windows
DLL loader lock). Generated wrappers do not yet discover/deploy native
dependencies automatically. A malformed/unsupported embedded module returns
a null contract getter and is rejected by NativeModule::open; embedding alone
is not a build-time module acceptance check or a sandbox.

Windows Debug qualification on 2026-10-14 covers runtime-only embedding,
installed-prefix relocation, shared and multiple static modules, stable generic
IDs, Unicode/NUL-bearing results, panic recovery, wrong-thread calls, result
ownership after close and invalid embedded bytecode. Linux/macOS and the full
existing SDK/runtime corpus are not freshly qualified by these focused gates.

Still required for packaging closure:

1. Extend the shared/static adapters to lifecycle/async/complex values, scoped
   host sessions, native dependency deployment and project CLI library targets.
2. Installed SDK host/deployment and clean Linux/macOS qualification gates.
3. Artifact/runtime/build-cost optimization and release budgets.

Project builds still produce `.wiob` plus a paired native producer as build
intermediates, then package the executable. Existing C++ backend executable/
shared/static targets remain unchanged.
