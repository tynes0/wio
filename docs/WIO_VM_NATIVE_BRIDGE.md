# Wio VM Native Bridge

Status: Sprint 20 implementation in progress.

The VM consumes the same wire-v2 `WioNativeAbiRegistry` and concrete native
thunks as the Lowered-WIR C++ backend and host SDK. It does not use a second FFI
contract and does not attempt to reproduce C++ overload, template, or layout
rules at runtime. The compiler resolves those rules and emits one stable native
key and ABI plan before bytecode generation.

## Sprint 20.1 registry and synchronous values

`Machine::registerNativeRegistry(...)` validates and copies every descriptor.
The descriptor array may be released after registration, while the containing
native library and thunk code must remain loaded until the machine can no
longer execute those functions. Registration is idempotent for the same stable
key/thunk pair and rejects rebinding a key to different code.

`NativeInvoke` resolves its external bytecode declaration by stable key and
executes the registered wire-v2 thunk. This slice supports:

- bool, signed/unsigned integer, and floating-point scalars;
- byte-preserving UTF-8 `string` values;
- scalar-valid UTF-32 `text` values;
- nullable wire void only where the Wio declaration is nullable;
- immutable `view` and mutable `ref` tokens over VM places;
- alias-preserving reference tokens and mutable copy-back, including copy-back
  when a native thunk reports a translated failure;
- native failure category, message, and native-code capture;
- deterministic rejection of missing registrations, ABI-version mismatches,
  conflicting stable keys, unsupported marshalling, and incorrect result kinds.

The bytecode verifier requires every `NativeInvoke` to match one external
function declaration, parameter list, result type, and encoded ABI plan.

Current stable diagnostics are:

- `WVM1220`: invalid or unregistered native target;
- `WVM1221`: invocation shape does not match the ABI plan;
- `WVM1222`: native thunk reported failure;
- `WVM1223`: argument conversion or reference validation failed;
- `WVM1224`: result or mutable-reference copy-back conversion failed;
- `WVM1225`: the requested marshalling/passing mode belongs to a later bridge
  slice.

## Sprint 20.2 foreign storage and ownership

The VM now has explicit value kinds for native POD bytes, opaque pointers,
foreign objects, and foreign runtime values. Native POD results are copied into
VM-owned byte storage. The other kinds retain the producer-supplied wire-v2
handle operations and release them when the last VM `Value` storage reference
dies. The VM never directly deletes memory created by a native library.

POD, object, and runtime-value ABI plans require reflection metadata with a
non-zero stable type id. The bytecode verifier rejects a missing identity as
`WBC1074`; the bridge checks both the wire value's id and the producer handle's
`typeId` operation before admitting it. Opaque pointers remain pointer-shaped
and may optionally carry a producer owner handle.

Copying a VM `Value` only increments the VM storage reference count. One
producer retain is held by that shared storage, so the final VM copy performs
exactly one matching producer release. Borrowed arguments expose the stored
handle for the duration of the synchronous thunk without transferring
ownership. Owned results are retained before the native frame releases its
wire value.

## Sprint 20.3 callback entry

Scalar Wio function values can now cross a native parameter as wire-v2
callbacks. A callback invocation decodes its scalar arguments, re-enters the
owning machine through the callable's function and captures, and converts the
scalar/void result back to the wire contract. VM errors and conversion failures
are contained at the callback boundary instead of unwinding through native
code.

The bytecode verifier pins callback parameters to function types and validates
the `call`/`retained` lifetime and `caller`/`any` thread enums. Call-scoped
callbacks expire immediately after the native thunk returns, even if native
code incorrectly retained the descriptor. Retained callbacks keep their VM
callable alive, but return `WIO_NATIVE_ABI_STALE_HANDLE` after the owning
machine is gone. Caller-thread callbacks reject foreign entry before trying to
lock or execute the VM; any-thread callbacks serialize entry through the
machine execution lock. The callback ops flags carry this affinity into the
shared C++ wire codec as well.

## Sprint 20.4 native async adaptation

A native function may now return the canonical wire representation of
`coroutine<T>`. Registering its registry together with `WioNativeTaskApi` and
the additive `WioNativeTaskCompletionApi` makes `NativeInvoke` create a normal
VM task rather than exposing a foreign runtime handle. The VM subscribes once,
reads and converts `T` only after terminal completion, and releases the
producer-owned task handle exactly once.

Completion callbacks may arrive synchronously or on any foreign thread. The
callback completes the external VM task; existing VM await machinery then
schedules the suspended Wio coroutine on the executor recorded in its
coroutine state. Native code therefore never resumes a Wio frame directly on
its callback thread. Faults cross as `WVM1226`, and native cancellation maps to
the existing `WVM1165` VM cancellation contract.

Cancellation is bidirectional at the ownership boundary: cancelling an
adapted VM task invokes the producer's task API, while machine teardown
cancels all outstanding foreign tasks before dropping their handles. The
exactly-once settlement guard resolves completion/cancellation races without
double reads or releases. `bindMainExecutor()`, `drainMainExecutor()`, and
`wait()` also bind and pump every registered native task runtime, so tasks
whose producer targets its main executor make progress without a separate host
loop.

`WioNativeTaskCompletionApi` is a separate wire-v2 sidecar instead of an
in-place extension of `WioNativeTaskApi`; modules built before this slice keep
their original binary layout. Such modules can still be loaded and polled by
the SDK, but the VM deterministically rejects async adaptation without the
completion sidecar as `WVM1225`.

## Sprint 20.5 discovery, lifetime, and project execution

`Machine::loadNativeLibrary(path, &error)` loads a shared library and discovers
`WioGetNativeAbiRegistry` plus its optional task/completion sidecars. Paths are
resolved canonically; loading the same module into one machine is idempotent.
Machines share a process-wide library lease. Missing modules, missing registry
exports, incompatible wire versions, and invalid sidecars fail before execution.
Old wire-v2 modules without completion sidecars remain usable for synchronous
calls; async adaptation still requires both task APIs.

Foreign object, runtime-value, and owned opaque storage pins producer code
until its final foreign release. Pending native task adapters also pin their
producer. Destroying a machine therefore cannot unload a DLL underneath an
escaped native value or an outstanding completion callback. The final machine
registration shuts down the producer task runtime on the host teardown
thread while its code lease is still held. Escaped value leases may keep the
DLL loaded longer. Shutdown is not deferred to the final completion callback,
which could otherwise try to unload its own producer's return address.

Program projects can now select the VM explicitly:

```powershell
wio project build --backend vm
wio project run --backend vm -- "two words" --verbose
wio project run --backend vm --no-build
```

The default backend remains `native`. VM builds produce `<baseName>.wiob` and
`<baseName>.vm-native.dll` (`.so`/`.dylib` on other platforms). The bytecode and
native bridge are generated from the same analyzed and lowered program, using
the compiler's `--bytecode-sidecar` output. A failed native compilation does
not publish its staged bytecode. Both outputs participate in incremental
checks, including Wio modules, native sources/headers, the manifest, and the
compiler/runtime inputs. Manifest run arguments, repeated `--arg`, and values
after `--` keep their existing order and are delivered to `main(string[])` or
`Entry(string[])`; async entries are awaited with both main queues pumped.

Additional wire registries and output paths can be configured in the manifest:

```ini
[vm]
bytecodeOutput = ".wio-build/interop/demo.wiob"
bridgeOutput = ".wio-build/interop/demo.vm-native.dll"
nativeLibraries = ["native/lib/plugin.dll"]
```

Paths are relative to the project root. The runner loads the generated bridge
and then the additional libraries before binding the main executor. Native
archive dependencies continue to use `[wio].linkLibraries`; `nativeLibraries`
is only for dynamic modules exporting the Wio wire registry.

This slice deliberately requires `wio.target = "exe"` and
`host.enabled = false`. Hybrid SDK hosts and VM executable/shared/static
packaging are separate artifact work. The generated bridge currently uses the
canonical C++ backend for the full program with main emission disabled; it is
not yet a native-only pruned translation unit, so a cold VM project build still
incurs native C++ compilation. Incremental runs reuse both outputs.

The internal runner protocol is
`wio --vm-run-bytecode FILE [--native-library PATH]... -- [args...]`.
Normal users should use the project commands above.

## Sprint 20.6 native differential qualification

`wio_vm_native_differential` compiles one Wio program as a native executable
and as paired bytecode/shared-bridge artifacts. It executes both with the same
arguments and requires a successful, independently checked result: two equal
failures cannot pass. Successful stdout/stderr must match exactly. Native
exceptions must fail on both backends with the original message and matching
pre-failure effects; the VM additionally preserves `WVM1222`. Diagnostic
wrappers currently use different output channels, so those are not compared
as application stdout.

The qualified paths include integer limits and floating values, enum/flagset
values, templates, native-produced POD and generic POD round-trips, extension
`ref`/`view` dispatch, mutable and mixed mutable/read-only aliases, embedded-zero
strings, borrowed native string results, nullable opaque copy-back and null
tests, native-produced `any` handles and mutation, captured callbacks,
call-scoped expiration, caller-thread rejection, retained any-thread callback
entry from a later native call, and native async scalar/runtime-value results.

String/text callback parameters and results now use the existing wire-v2
slice contract. VM results transfer an owned storage handle, not a pointer
into the callback's temporary execution frame. Empty strings, embedded zeros,
supplementary Unicode scalars, retain/release, frame destruction, and repeated
release are covered by `wio_vm_native_value_contract`. Scalar callbacks keep
their allocation-free result path. Width checks reject malformed narrow
integer values; `f32` crossing rounds as the C++ codec does. Native enum and
flagset codecs use their underlying scalar rather than accidental POD encoding.

The bytecode compiler records structural ABI identities for non-nominal
runtime values, including async payloads, using exactly the C++ emitter's
identity rule. No ABI or bytecode format version change is required.

Both new tests carry `native-differential` and `release-gate` labels. The
Windows/Linux validation workflow selects the gate by label:

```powershell
ctest --test-dir build -C Debug -L "^native-differential$" --output-on-failure
```

Local qualification was performed on Windows. Linux execution is delegated
to the existing CI matrix; a local Windows pass is not a Linux pass claim.

## Remaining VM/native parity and artifact work

This gate qualifies the implemented bridge; it does **not** assert that every
native C++ operation is supported by the VM. Three known boundaries have
executable negative cases in the differential fixture:

- Locally constructed VM components cannot yet be marshalled as native POD.
  Native-produced POD storage can round-trip, but VM field access/construction
  needs producer layout/field adapters. The current boundary is `WVM1223`.
- VM-created `any`/containers/objects are not interchangeable with foreign
  runtime/object handles. Native factories and their foreign values work;
  structural VM-to-producer materialization still needs adapters (`WVM1223`).
- POD/object/container callback signatures, nullable callback signatures, and
  callback `ref`/`view` parameters need signature-specific type identities and
  borrow/copy-back plans. Scalar, enum/flagset, string, text, and void callbacks
  are implemented; the other shapes remain `WVM1225`.

Consumed native argument passing remains explicitly rejected as `WVM1225`.
Native-only bridge pruning, hybrid VM SDK hosts, and VM executable/shared/static
artifact packaging remain separate work. These boundaries must be removed or
explicitly qualified before advertising full native-language parity.
