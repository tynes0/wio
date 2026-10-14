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
SDK session owns an independent VM instance, even when two opens load the same
DLL. It does not depend on a separate
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

Calls/registration belong to the session's opening thread (or the constructing
thread for a direct ModuleAdapter). Foreign calls return WRONG_THREAD;
reentrant mutations reject. Owner-thread completion callbacks may query/read a
task result. VM execution failure returns PANIC, conversion errors
return TYPE_MISMATCH, and exceptions are contained at the wire boundary.
Direct embedders can inspect `ModuleAdapter::lastError()` on the owning thread.
Raw result slots must be empty; arguments are copied, never consumed. String
and text results own their storage. Session SDK values pin producer code, not
the closed VM instance, through close and cloning. Raw ABI callers must retain producer **code** until
the final owner release, even when result storage outlives the adapter.

This is deliberately a bounded module-hosting surface. Ref/view, nullable,
callback, object/component and container/Any/Box exports reject during
adapter initialization. Async exports use the bounded task surface below;
application hosting and complex/nested task payloads remain unqualified. Workers
are joined during explicit session close, never under a Windows DLL loader
lock. Generated wrappers do not yet discover/deploy native
dependencies automatically. A malformed/unsupported embedded module returns
a null contract getter and is rejected by NativeModule::open; embedding alone
is not a build-time module acceptance check or a sandbox.

Windows Debug qualification on 2026-10-14 covers runtime-only embedding,
installed-prefix relocation, shared and multiple static modules, stable generic
IDs, Unicode/NUL-bearing results, panic recovery, wrong-thread calls, result
ownership after close and invalid embedded bytecode. Linux/macOS and the full
existing SDK/runtime corpus are not freshly qualified by these focused gates.

## Instance sessions and lifecycle

Generated modules now expose additive `WioGetSdkModuleSessionApi` v1 alongside
the canonical getter, without changing contract v1, value ABI v2 or bytecode.
`NativeModule::open` prefers this sidecar and opens a fresh VM with separate
globals/call contexts on each call. A present but invalid session sidecar is an
error, not a reason to silently fall back. Opening does not execute user Load.

```cpp
auto first = wio::sdk::NativeModule::open("game.dll");
auto second = wio::sdk::NativeModule::open("game.dll");
first.start();              // Load once; repeated start is idempotent
second.start();             // independent VM/global state
first.update(1.0 / 60.0);   // finite, nonnegative f32-range delta
auto state = first.saveState();
second.restoreState(state); // copies the concrete state payload
first.stop();               // explicit close, reports unload failure
first.close();              // releases this SDK view, noexcept
```

Lifecycle-bearing exports require start; lifecycle function bodies cannot be
invoked through the ordinary export table to bypass these state transitions.
API version can be queried before start and is a module-supplied u32, not the
session descriptor or product version. Missing callbacks reject their respective
operations. Supported signatures are `api_version: () -> u32`,
`load: () -> i32`, `update: (f32) -> void`, `unload: () -> void`, and paired
`save_state: () -> T`, `restore_state: (T) -> i32`. T must be a qualified
non-void scalar/enum/flagset/string/text value; restore uses the exact saved
type declaration. The host remains responsible for comparing state schema
versions across different module revisions before restoring them.

Load/restore return zero for success. A failed Load closes the instance and
attempts Unload once to roll back partial initialization; retry does not rerun
Load. Unload failure still destroys the session, with no destructor retry.
Closing an unopened/unstarted instance does not run Unload. Closing the last
SDK view shuts down its session, even if owning results/snapshots still pin the
DLL. Copied views share an instance; stop invalidates their common contract.
Active host calls pin the session until they return, so closing a view from a
reentrant host callback cannot delete the currently executing VM.

Static wrappers also publish `${GETTER}SessionApi` by default; the optional
`SESSION_GETTER` CMake argument chooses a distinct C identifier. Use
`NativeModule::attachSession(api, owner)` for these or for a scoped
`vm::ModuleSessionFactory`, passing a factory/code lifetime owner. A static
process-lifetime token is suitable only for genuinely statically linked code.
Lifecycle-bearing and async modules reject the legacy default contract getter; pure
legacy canonical getter/attach remains available with its older shared-instance
and caller-managed affinity semantics. Do not mix that compatibility instance
with session-based resource/affinity guarantees.

```cpp
auto factory = std::make_shared<wio::vm::ModuleSessionFactory>(std::move(decoded.module));
auto game = wio::sdk::NativeModule::attachSession(factory->api(), factory);
game.start();
// End affine sessions before their owner thread exits.
game.stop();
```

Wrong-thread raw close preserves the live token. If the final SDK view is
destroyed off-thread, SDK retirement pins the instance and producer code until
`NativeModule::collectSessions()` runs on its opening thread. This safe point
does not steal ownership or run user unload on another thread; pending instances
are deliberately not destroyed during static/DLL teardown. Hosts must close or
collect before the owner thread exits, otherwise the retired instance/code stay
pinned until process termination. Session/value view mutation is not a promise
of concurrent access to the same C++ SDK object. Separate VM instances can
belong to separate owner threads.

Raw sessions are affine noncopyable ownership tokens by convention. Do not
retain contract/context pointers after close, copy raw ownership claims, or
unload producer code while an owning wire still has release callbacks. Use SDK
owners unless implementing these lifetime rules explicitly.

Two new Windows Debug gates on 2026-10-14 qualify lifecycle signature checks,
exactly-once start/unload, rollback/unload failure, independent shared/static
globals, state copying, owner-thread retirement, reentrant view closure and
escaped snapshots/results. This is synchronous lifecycle/session closure within
the qualified value surface, not application/async/complex SDK parity.

## Async session tasks

VM sessions publish a separate `WioGetSdkModuleTaskApi` v1 sidecar. Session v1,
native value ABI v2, task-source wire and bytecode versions do not change.
`NativeModule::open` validates the optional sidecar against the exact session
descriptor; a present invalid sidecar never silently falls back. Static modules
default to `${GETTER}TaskApi`; `TASK_GETTER` selects a unique C identifier.
In-process/static attachment passes `&factory->taskApi()` as the third argument
to `NativeModule::attachSession(factory->api(), factory, ...)`.

Concrete async functions and generic specializations may return owning tasks
with void, scalar, enum/flagset, string or text payloads. Arguments still use the
qualified ordinary value surface. Ordinary functions returning a qualified
`coroutine<T>` (for example a timer) use the same task sidecar. Owning task
arguments use the same-session capability described below. Async ref/view and
complex or nested task payloads are not enabled. Invocation returns the task without waiting
for its completion. A task can be pending, ready, faulted or cancelled; there is
no temporary/default payload. Read before completion returns NOT_READY; reading
a fault/cancellation throws through the C++ wrapper (raw APIs return statuses).
Fault text is preserved in the wrapper's exception.

```cpp
auto module = wio::sdk::NativeModule::open("jobs.dll");
module.start(); // required only when the library declares a lifecycle
std::array args{wio::sdk::NativeValue::integer(41)};
auto task = module.invoke("Work", args); // no implicit wait
if (module.waitTaskReady(task, std::chrono::milliseconds{10}))
{
    auto result = module.readTask(task);
    std::cout << result.asInteger();
}
else
{
    module.cancelTask(task); // timeout itself does not cancel
}
module.stop();
```

`taskReady`, `cancelTask`, `readTask`, `onTaskComplete` and `pumpMain` are scoped
to the task's original session/generation and opening thread. Cloning/moving a
task preserves provenance; a different session rejects it, even for the same
DLL. `pumpMain` drives owner-thread continuations. `waitTaskReady(timeout)` and
`waitTask()` are explicit blocking convenience operations that pump main work;
the latter has no timeout. Timeout is a progress-check budget, not preemption of
a running body/native call. A host can instead poll/pump in its application loop.

Completion subscriptions own their task wire until exactly-once delivery and
contain C++ callback exceptions. Delivery may be inline or on the completing
worker/timer thread. Owner-thread callbacks may query/read the settled task;
worker callbacks must marshal SDK operations back to the owner. Mutating reentry
(invoke/cancel/stop/pump/subscribe) remains rejected. A callback may release its
SDK view/task; active calls and subscriptions hold the necessary leases.

Without a registered native producer, **exact** `wio::runtime::AsyncSleep(u64)`
and `wio::runtime::RunAsync(fn() -> T)` bindings with canonical ABI plans use the
VM's timer/worker services. A registered producer retains precedence. The worker
fallback currently qualifies non-async Wio callables with scalar/string/text
captures, an explicit retained/any-thread callback ABI and the payload surface
above; other native helpers/captures still
require their producer. A foreign function merely named Sleep/Spawn, or a
signature mismatch, does not acquire builtin behavior.

Close marks the session closed, quiesces async activity and cancels pending work
before synchronous Unload. Worker/timer queues are joined, then VM storage is
released. Arbitrary blocking native work cannot be forcibly preempted. Raw task
sources become STALE_HANDLE after close; decoded results still own their storage
and pin producer code. Escaped task wires retain immutable metadata, not a live
VM, and their last release may occur off-thread without joining worker threads.
Raw ABI callers still must pin producer code through their final wire release.

Focused Windows Debug gates `wio_vm_task_source_shutdown` and
`wio_vm_sdk_async_artifacts` cover 80 worker/main/timer shutdown repetitions and
installed, relocated shared/static hosts, pending/timeout/cancel/fault/void,
generic specialization, Unicode/NUL values, provenance/affinity, callback read
and reentrant task release. A controlled native-worker barrier also proves that
task publication/query/simple read/subscription do not wait behind unrelated
serialized VM execution. The two gates passed 2/2 in 36.67 seconds on 2026-10-14.
This is not full SDK/async parity or a fresh full
existing-corpus/platform qualification.

### Owning task parameters and relay

A synchronous or async export may accept `coroutine<T>` by value when T is in
the qualified void/scalar/enum/flagset/string/text surface. This copies a handle
to the **same task state**, not its payload, and never implicitly waits or
consumes the caller's value. Concrete generic exports use the same canonical
ABI rule as ordinary exports; legacy C ABI restrictions remain unchanged.
Structural task/payload identities are emitted even for parameter-only types.

```wio
[export]
async fn Number(value: i32) -> i32
{
    await main;
    return value;
}

[export]
async fn Add(task: coroutine<i32>, value: i32) -> i32
{
    return await task + value;
}

[export, instantiate(i32)]
fn Relay<T>(task: coroutine<T>) -> coroutine<T>
{
    return task;
}
```

```cpp
std::array input{wio::sdk::NativeValue::integer(41)};
auto source = module.invoke("Number", input);
std::array arguments{source.clone(), wio::sdk::NativeValue::integer(1)};
auto chained = module.invoke("Add", arguments);
source.reset();
arguments[0].reset(); // the suspended Wio frame owns its task handle
auto answer = module.waitTask(chained); // 42; explicit wait pumps main
```

`Relay<T>` returns an owning source for the same underlying task. Cancelling
that alias cancels the original state. Cancelling a parent awaiting a shared
input does not implicitly cancel the input; use explicit cancellation or the
language's structured-scope rules when group cancellation is intended. A source
fault/cancellation still propagates through Wio's existing await semantics.

Both SDK convenience calls and raw call-table entry enforce provenance. The
input must be an original owning TASK_SOURCE wire from the same live session,
with the exact declared task identity. Another session, even of the same DLL,
does not qualify. Borrowed/ref/view, consumed, copied/forged source descriptors,
producer-private runtime handles and wrong payload identities reject before the
function body. All arguments are decoded before execution; a later invalid
scalar does not execute a partially validated task call. Raw failure leaves an
empty output slot; no task claim is consumed. Host-provided/foreign-session task
creation and complex/nested payloads remain separate work, not enabled by this
capability. Sessions without the task sidecar cannot use task convenience calls.

New focused gates `wio_vm_task_source_import` and
`wio_vm_sdk_task_inputs_artifacts` cover 100 import/identity/foreign-release
repetitions plus installed shared/static chaining, generic relay, cancellation
direction, fault propagation, void and Unicode/NUL payloads, wrong-thread/raw
ABI rejection, parameter-only reflection, legacy ABI rejection and unsupported
complex-payload initialization. Both passed on Windows Debug on 2026-10-14:
2/2 in 23.17 seconds. Older gates and other platforms were not rerun.

Still required for packaging closure:

1. Extend the shared/static adapters to complex/async value parity and application
   hosting, native dependency deployment and project CLI library targets.
2. Installed SDK host/deployment and clean Linux/macOS qualification gates.
3. Artifact/runtime/build-cost optimization and release budgets.

Project builds still produce `.wiob` plus a paired native producer as build
intermediates, then package the executable. Existing C++ backend executable/
shared/static targets remain unchanged.
