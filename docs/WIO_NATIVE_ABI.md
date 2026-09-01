# Wio Native ABI Contract

This document freezes the boundary shared by generated C++, the future
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

The production AST-to-C++ generator remains active during WIR migration. The
new C++ backend and VM bridge will consume this contract after their respective
sprints; they must not create a second ABI model.
