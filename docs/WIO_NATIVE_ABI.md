# Wio Native ABI Contract

This document describes the boundary shared by generated C++, the future
bytecode VM, native libraries, and the host SDK. It describes the canonical
contract, not a particular platform's C++ object layout.

## Boundary rule

Wio may bind a normal C++ API, including overloads, references, templates, and
POD structs. The compiler resolves that rich surface once and emits a concrete,
C-shaped thunk. Backends invoke the thunk; they do not repeat C++ overload or
template resolution.

```wio
[Native, CppHeader("metrics.h"), CppName(metrics::Sample)]
component Sample {
    value: f64;
    timestamp: u64;
}

realm metrics {
    [Native, CppHeader("metrics.h"), CppName(metrics::Normalize)]
    fn Normalize(sample: ref Sample, label: view string);

    [Native, CppHeader("metrics.h"), CppName(metrics::Map), Instantiate(i32)]
    fn Map<T>(value: T, callback: fn(T) -> T) -> T;
}
```

The first function records mutable borrow + native POD and immutable borrow +
UTF-8 marshalling. The second produces a concrete template-specialization
thunk and a call-scoped callback record.

## Ownership

- Scalars, POD values, and opaque pointer identities are passed by value.
- `view T` is an immutable borrow valid for the call unless declared retained.
- `ref T` is a mutable borrow valid for the call.
- A consumed parameter transfers exactly one ownership claim.
- A managed return is an owned claim.
- Object/runtime handles always carry owner-provided `retain`, `release`, and
  type identity operations.
- Foreign code never invokes `delete` or a VM heap operation directly.

The same intrusive strong-count semantics therefore apply when both sides are
generated C++, when a host loads a Wio DLL, and when a VM calls native code.

## Failure boundary

Every generated C++ thunk catches native exceptions and translates them into
`WIO_NATIVE_ABI_EXCEPTION` with a `WioNativeAbiFailure`. A panic or VM failure
uses the same status channel. No exception may unwind across a DLL, callback,
or VM boundary.

## Callbacks and foreign threads

The default callback is borrowed for the duration of one native call and may
be entered only from the caller thread. Native code that stores it must retain
its userdata and later release it. A future explicit any-thread contract will
enter the runtime through the registered executor/foreign-thread gate; native
code may not call VM frames directly from an arbitrary thread.

## Generic C++ APIs

A C++ template is never exported as a template through the ABI. Semantic
analysis selects allowed concrete Wio arguments. `NativeAbiPlanner` combines
the declaration's stable signature with each `native-invoke` specialization
key and emits one deterministic thunk descriptor per concrete specialization.

## SDK surface

`wio_native_abi.h` is deliberately C-shaped and fixed-width. C++ convenience
wrappers may be added around it, but the wire contract consists of:

- `WioNativeAbiValue` and its explicit ownership flags;
- byte/POD/text slices;
- `WioNativeAbiHandle` plus owner operations and generation;
- `WioNativeAbiCallback` plus retain/release/invoke operations;
- `WioNativeAbiFailure` and status codes;
- `WioNativeAbiFunctionDescriptor` and thunk pointer.

The production AST-to-C++ generator remains the default during WIR migration.
Sprint 17.4 implements checked thunks in the opt-in Lowered-WIR C++ backend;
the VM bridge is still future work, consuming the same contract.

## Experimental wire ABI v2 (Sprint 17.4)

`WIO_NATIVE_ABI_VERSION` is 2. Rebuild experimental wire-v1 producers and hosts
together: adding `WioNativeAbiValue.owner` changes its binary layout. This does
not change the independent `WioModuleApi` v11 descriptor. Sidecar call entries
use the explicit `WIO_SDK_CALL_NATIVE_ABI_V2` marker so the new SDK cannot mistake
legacy payloads for canonical values.

String output uses an owned byte slice; Unicode `text` uses a validated UTF-32
slice with a **byte count**, not a `wchar_t` count. Surrogates and values above
U+10FFFF are rejected. POD output has an exact stable type ID and byte size;
platform padding/layout is not a cross-platform serialization format.
`WioNativeAbiReleaseValue` releases the producing owner's storage and clears
the value. Objects delegate retain/release to their existing intrusive runtime.
Runtime containers/tasks are boxed module-owned values with owner-op checks.

A `REFERENCE` argument points to another `WioNativeAbiValue`, with BORROWED and
optionally MUTABLE flags. A call frame decodes repeated tokens to shared local
storage and copies mutations back on success or caught failure. This preserves
aliasing, but is not transactional rollback. Returned borrows and asynchronous
borrow parameters are rejected because that frame is call-local. Raw tokens
must remain valid/aligned; arbitrary host pointers cannot be validated safely.

Native call-scoped callback wrappers reject use after the native call ends and
reject wrong-thread entry. Copying the `std::function` alone does not grant an
extended lifetime. Canonical callback invoke returns a status across the foreign
boundary; the current SDK callback path is caller-thread-only. Foreign retained
callbacks must keep their owner module loaded until their final release.

`WioGetNativeAbiRegistry` publishes concrete thunk descriptors. Thunks validate
argument count, required pointers, tags and integer narrowing before invocation;
they translate C++ exceptions to statuses. Failure strings are thread-local and
valid until the next failing boundary call on that thread. Output slots must be
empty on entry; release previous owned contents before reusing a slot.
The ABI trusts in-process handle pointers and is not a memory-safety sandbox.
