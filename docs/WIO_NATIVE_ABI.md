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
be entered only from the caller thread. Native declarations opt into an
escaping callback explicitly on the callback parameter:

```wio
fn ThreadStart(
    [attribute::NativeCallback("retained", "any")] action: fn()
) -> opaque with native, cpp::name(runtime::ThreadStart);
```

`retained` permits the native function to store the callback after the call
returns; `any` permits entry from a foreign thread. The default spellings are
`call` and `caller`. Typed and Lowered WIR freeze both decisions in
`NativeAbiValue`, so the C++ backend, VM bridge, and SDK adapters consume the
same lifetime/thread contract instead of recognizing particular native symbol
names. Retaining a callback also retains the producing module until the final
callback claim is released.

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

The production Lowered-WIR C++ backend emits the Sprint 17.4 checked thunks.
The VM bridge consumes the same concrete binding and ownership contract.

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
aliasing, but is not transactional rollback. Qualified returned borrows map
back to explicit compatible caller cells; asynchronous borrow parameters are
rejected because that frame is call-local. Raw tokens
must remain valid/aligned; arbitrary host pointers cannot be validated safely.

Native call-scoped callback wrappers reject use after the native call ends and
reject wrong-thread entry. Copying the `std::function` alone does not grant an
extended lifetime. Retained/any-thread callbacks use their explicit WIR
contract; unannotated SDK callbacks remain caller-thread-only. Canonical
callback invoke returns a status across the foreign boundary. Foreign retained
callbacks must keep their owner module loaded until their final release.

`WioGetNativeAbiRegistry` publishes concrete thunk descriptors. Thunks validate
argument count, required pointers, tags and integer narrowing before invocation;
they translate C++ exceptions to statuses. Failure strings are thread-local and
valid until the next failing boundary call on that thread. Output slots must be
empty on entry; release previous owned contents before reusing a slot.
The ABI trusts in-process handle pointers and is not a memory-safety sandbox.

## Consumed argument claims

Use the standard parameter attribute on a native declaration:

```wio
use std::attribute as attribute;
use std::heap as heap;
using cpp::header("resources.h");

[Native, CppName(resources::Take)]
fn Take([attribute::NativeConsume] value: heap::Box<i32[]>) -> i32;
```

`NativeConsume` transfers one owning **invocation claim**, not unique ownership
of the caller variable. Normal Wio argument copy/value semantics remain intact:
an object variable can still be used after the call. C++ receives a moved typed
invocation value, which it may retain using the existing intrusive `Ref`.
This does not introduce a new `move` expression or a new reference counter.

Typed WIR freezes `Consume`; Lowered WIR, bytecode and specialization preserve
it. The signature identity includes consumed parameter positions. The producer
advertises `WIO_NATIVE_ABI_FUNCTION_CONSUMED_ARGUMENTS`; the VM rejects the call
before transferring claims if that capability is absent. Wire ABI version 2
and descriptor layouts are unchanged.

For a compatible, count-validated invocation, each consumed argument has
`CONSUMED | OWNED`, never `BORROWED`. The generated thunk adopts these claims
before argument decoding, without allocation. Its guard releases them on
successful return, decode failure or native exception, including validation
failure after adoption. The caller clears transferred wire slots after the
thunk returns and **does not release them again**. Before entering the thunk,
encoding/admission failures leave cleanup with the caller. Invalid argument
count, null argument array or missing result slot is rejected before adoption;
raw ABI hosts must retain ownership of their inputs on that pre-adoption path.

Strings/text, registered containers/Any, native POD values and qualified object
handles can carry invocation claims. Scalars/null have logical ownership flags
without an allocation to release. `ref`/`view`, raw `opaque`, callable and task
parameters cannot be consumed (`WIR2403`); ordinary non-native declarations
cannot use this attribute (`WIR2404`). Wrap an owning raw resource in an object
whose destructor releases it; an `opaque` pointer alone has no ownership policy.

## Borrow identity qualification

Native call frames canonicalize each `(live producer address, concrete C++
type)` to one stable ABI identity. Direct POD fields, sequence/POD projections
and mixed dictionary/sequence/POD paths share a root materialization and cell.
The concrete type is important: a struct and its first field can have the same
address without being the same typed cell. All POD paths use registered declared
field/layout accessors; hidden fields and padding are not a public projection.

Callback ref/view cells support repeated exact typed-cell aliases. Their tagged
snapshot protocol does **not** support simultaneous aggregate and descendant
borrows, such as `ref row` together with `ref row[0].x`. Generated native callback
wrappers and VM callback encoding reject that overlap before invocation; pass
one aggregate borrow and access its children inside the callback. This explicit
limit avoids silently splitting one live cell into independent snapshots.
