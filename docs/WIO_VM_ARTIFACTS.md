# Wio VM Executable Artifacts

Status: executable slice implemented; project-command integration and VM-backed
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

- The output must not exist. There is no `--force` yet; select a fresh output
  path. This prevents overwriting arbitrary files or a deployed application.
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

## Installed embedding surface and remaining work

Installation includes `wio-vm`, the `wio_vm` and `wio_bytecode_core` archives,
and their runtime-only headers. C++ embedders must use a matching compiler,
architecture and C++ runtime, and keep the immutable decoded module alive for
the Machine lifetime. The archives do not yet provide a packaged Wio module
with the SDK lifecycle/export tables.

The `WioVmRuntime` install component installs the runner, archives, runtime
headers and required native wire header without the compiler:

```powershell
cmake --install build --config Release --component WioVmRuntime --prefix vm-runtime
```

Use `include/` and `sdk/include/` when compiling an embedder and link both VM
archives plus platform thread/dynamic-loader libraries as appropriate. This
slice does not yet ship an exported CMake package for embedding.

Still required for packaging closure:

1. Project CLI build/run/package integration, including safe repeated builds.
2. VM-backed shared-library and static-library Wio module wrappers with SDK
   exports, lifecycle, async results and module ownership.
3. Installed SDK host/deployment and clean Linux/macOS qualification gates.
4. Artifact/runtime/build-cost optimization and release budgets.

Existing `wio project --backend vm` behavior is unchanged by this first slice;
it still builds `.wiob` plus a paired native producer and uses the existing
project runner. Existing C++ backend executable/shared/static targets remain.
