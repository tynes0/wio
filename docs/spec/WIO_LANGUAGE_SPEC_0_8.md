# Wio Language Specification 0.8 — Nullability and Lifetime

Status: normative for Wio 0.8 nullability, initialization, ownership,
destruction, panic, and native exception behavior. Language areas not defined
here remain governed by the implementation and `WIO_LANGUAGE_DRAFT.md` until
they enter a later versioned specification.

## 1. Type categories

An `object` value is an owning, reference-counted handle. Copying or assigning
the value copies the handle and preserves object identity. A `component` is an
inline value; copying or assigning it copies the value. Arrays, dictionaries,
strings, and components have value/container semantics. `ref T` and `view T`
are non-owning borrows and never extend the owner's lifetime.

Object/interface handles, native `opaque` handles, function values, and
`ref`/`view` values are non-null unless their type is explicitly nullable.
`any` remains a tagged dynamic value and may contain the null tag. `Option<T>`
represents optional domain data and is not interchangeable with `T?`.

## 2. Nullable types

The grammar adds a postfix nullable type constructor:

```text
nullable-type := primary-type [ "?" ]
grouped-type  := "(" type ")"
```

Examples:

```wio
mut service: Service? = null;
let handle: opaque? = null;
let callback: (fn(i32) -> i32)? = null;
let observer: (view Service)? = null;
```

`T?` is valid only when `T` is an object/interface handle, `opaque`, a function
type, or a `ref`/`view` type. It is invalid for primitives, components, strings,
arrays, dictionaries, and an already-nullable type.

Parentheses are semantically important around prefix borrow types:

- `ref T?` is a mutable borrow of a nullable `T` storage location.
- `(ref T)?` is a nullable mutable borrow of non-null `T`.
- `view T?` and `(view T)?` follow the same rule.

## 3. Compatibility and initialization

`null` converts only to an explicit nullable type or to `any`. A non-null `T`
implicitly promotes to `T?`. A `T?` never implicitly converts to `T`. Two
nullable types are compatible exactly when their value types are compatible.

A non-null local/global handle or function declaration requires an initializer.
Nullable storage may be initialized with `null`. Component and managed value
types retain their defined default initialization behavior. Fields are
initialized before constructor-body execution and constructors are responsible
for establishing their declared invariants.

## 4. Flow-sensitive narrowing

A direct nullable variable is narrowed from `T?` to `T` on paths proven
non-null by:

- the true branch of `value != null`;
- the false branch of `value == null`;
- the right operand of short-circuit `and`/`&&` or `or`/`||` when execution of
  that operand proves the variable non-null;
- a `while (value != null)` body;
- code following an `if (value == null) { return ...; }` guard.

Assignment to the variable invalidates its narrowing. Member access, calls,
and conversion from `T?` to `T` outside a proven path are compile-time errors.

## 5. Copy, assignment, and destruction

The observable lifecycle matrix is:

| Value | Copy/assignment | Destruction |
| --- | --- | --- |
| component | independent value copy | `OnDestruct` once per value |
| object | shared owning handle | `OnDestruct` once, after the last strong handle |
| ref/view | borrow copy | no owner destruction |
| managed container | value/container copy | contained values are released normally |
| `resource::Owned<T>` | shared owner handle | live resource closed once |

`OnDestruct` takes no parameters and returns `void`. It runs on normal scope
exit, early return, and panic unwinding. Source-level move syntax is not part of
0.8; backend move elision/optimization must not change the observable matrix.

## 6. Recoverable errors and panic

Recoverable application/library failures use `std::Result<T>`. `Option<T>` is
for presence/absence without error detail. `std::Panic` represents an
unrecoverable programming/runtime failure and unwinds Wio stack frames so RAII
cleanup and `OnDestruct` run before the executable boundary reports the error
and returns a non-zero exit code.

Destructors and native close callbacks must not throw or panic. A failure from
explicit `Dispose()` may propagate as a panic, but the owner is marked disposed
before invoking the closer so cleanup is not repeated during unwinding.

## 7. Native boundary

Native code is privileged. Every generated `@Native` wrapper translates
`std::exception` into `wio::runtime::RuntimeException` with the native symbol
and original message, preserves an existing `RuntimeException`, and translates
unknown C++ exceptions into a stable Wio runtime error. The executable boundary
also contains otherwise-untranslated native exceptions.

Native functions may use nullable pointer/handle declarations explicitly.
`ref T?` borrows nullable storage; `(ref T)?` is a nullable borrow. Native code
must honor non-null declarations. Native `ref`/`view` returns remain rejected
because their lifetime cannot be proven.

`std::resource::Owned<T>` provides idempotent deterministic close, final-owner
cleanup, release, borrowed wrapper creation, and a live-resource diagnostic
counter. `Borrowed<T>` never closes its value; the caller must keep the native
owner alive.

## 8. SDK metadata

Module type descriptors preserve nullability with
`WIO_MODULE_TYPE_DESC_NULLABLE`; `elementType` points to the non-null value
descriptor. `TypeDescriptorView::is_nullable()` exposes the distinction to
hosts. Appending this descriptor kind preserves the layout of the module ABI
structures.
