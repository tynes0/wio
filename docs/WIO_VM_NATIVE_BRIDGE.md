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

## Remaining Sprint 20 surface

The next slices add library loading/registry discovery, CLI/project
integration, and the full C++-backend-versus-VM native differential gate.
Non-scalar callback signatures remain intentionally rejected until owned
callback-result storage is part of the wire contract.
