# Wio Language Specification 0.10 — Const Generics and Native Components

Status: normative for ordinary integer const generics and declaration-level
native component interop. This document extends the 0.9 generic contract.

## 1. Const generic declarations

Functions, aliases, interfaces, components, and objects may mix type parameters
with integer const parameters.

```text
generic-parameter := identifier [ "=" type ]
                   | "const" identifier ":" integer-type [ "=" const-argument ]
                   | identifier "..."
```

```wio
component Buffer<T, const N: usize = 4> {
    values: [T; N];
}

fn Capacity<const N: usize>() -> usize {
    return N;
}
```

The permitted value types are `i8`, `i16`, `i32`, `i64`, `isize`, `u8`,
`u16`, `u32`, `u64`, and `usize`. Const parameter packs are not supported.
Const parameters and type parameters share declaration order, trailing-default,
scope, duplicate-name, and earlier-parameter default rules.

## 2. Const arguments and evaluation

A const argument is a non-negative integer literal, an earlier const generic
parameter, or a top-level integer `const` whose initializer is accepted by the
compile-time integer evaluator. A referenced top-level const may use the
existing scalar integer operations and other acyclic integer consts.

```wio
const Base: usize = 2usize;
const Width: usize = Base + 3usize;

let value: Buffer<i32, Width>;
```

An argument in a type-parameter slot must be a type. An argument in a const
slot must be a compatible compile-time integer value. Mixing the two is a
semantic error before C++ generation. Deduction precedes defaults; defaults
fill only unresolved trailing slots.

Inline arbitrary expressions in angle brackets or array extents, floating
point/string/bool const parameters, and const parameter packs are outside this
version. Name a reusable expression with a top-level `const`.

## 3. Static arrays and substitution

`[T; N]` accepts an integer const parameter as its extent. Instantiation
substitutes both the element type and extent before constructor checking,
literal-size checking, overload resolution, reflection, and C++ emission.

```wio
type Fixed<T, const N: usize> = [T; N];

let values: Fixed<i32, 3> = [1, 2, 3];
```

Const generic values are also ordinary read-only expressions inside the body
of their declaration. They cannot be assigned to or borrowed as storage.

## 4. Identity, deduction, and specialization

Const arguments participate in generic identity and invariance. `Buffer<T, 3>`
and `Buffer<T, 4>` are distinct and neither converts to the other.

Static-array parameters may deduce a const extent from a concrete static-array
argument. Explicit and partial specialization patterns may contain concrete
values or declared const parameters.

```wio
@Specialize(T, 4)
component Buffer<T> {
    values: [T; 4];
    quartet: bool;
}
```

The 0.9 ordering remains unchanged: exact specialization outranks partial
specialization, the more specific partial pattern wins, equal best matches are
ambiguous, and the primary is the fallback. Concrete values add specificity;
a const parameter is a placeholder.

## 5. C++ representation and mangling

A Wio const parameter lowers to a C++ non-type template parameter using its
declared integer type. A concrete value is emitted as the corresponding
template argument. The ordered sequence of type and const arguments is part of
Wio specialization keys, generated C++ type identity, reflection identity,
and native binding selection.

```wio
component Buffer<T, const N: usize> { values: [T; N]; }
```

Conceptually lowers to:

```cpp
template <typename T, std::size_t N>
struct Buffer { wio::SArray<T, N> values; };
```

## 6. Declaration-level native components

A declaration-level native component is a zero-wrapper alias to a C++ POD-like
value type.

```wio
@Native
@CppHeader("native_records.h")
@CppName(native::record)
component Record<T, const N: usize> {
    values: [T; N];
}
```

Its normative rules are:

1. `@Native`, `@CppHeader`, and `@CppName` belong to the primary declaration.
2. The generated Wio type is an alias to the named C++ type/template; no Wio
   wrapper object or layout conversion is inserted.
3. Fields must be primitives, POD-compatible static arrays, generic
   placeholders that become such types, or other declaration-level native
   components.
4. Declaration-level native components cannot declare lifecycle functions or
   methods. Behavior belongs in native/free functions or Wio extensions.
5. Field order, field type, alignment, template argument order, and native C++
   layout are an ABI promise made by the binding author.
6. Dynamic arrays, strings, dictionaries, objects, interfaces, nullable
   handles, and other managed runtime values are not POD fields in this model.

## 7. Native component specialization

A Wio specialization may refine the semantic field surface of an `@Native`
generic component. It inherits native status, header, and C++ name from the
primary. Repeated mapping attributes, when present, must be identical.

```wio
@Native
@CppHeader("record.h")
@CppName(native::record)
component Record<T> { value: T; }

@Specialize(i32)
component Record {
    value: i32;
    bonus: i32;
}
```

The C++ primary alias already maps `Record<i32>` to `native::record<int32_t>`.
Consequently a Wio specialization emits no second alias-template
specialization. The corresponding C++ template specialization must exist and
must have the layout declared by Wio. A non-native primary cannot acquire a
new ABI identity by marking only a specialization `@Native`.

## 8. Native component extensions

A declaration-level native component may expose C++ free functions through
ordinary extension method syntax. The extension method itself is marked
`@Native` and has no Wio body.

```wio
extension RecordNative for Record {
    @Native
    @CppName(native::Inspect)
    public view fn Inspect() -> i32;

    @Native
    @CppName(native::Reset)
    public ref fn Reset();
}
```

The extension receiver is the implicit first native argument. A `view fn`
first attempts a `const T&` call and then a `const T*` call. A `ref fn` first
attempts a `T&` call and then a `T*` call. Reference form has deterministic
precedence when both native overloads are viable. The receiver is never passed
by value and no component copy or layout bridge is inserted.

`@CppName` selects a namespaced or differently named C++ free function. When it
is absent, the public extension method name is the native symbol name.
`@CppHeader` may be supplied on the method when the function is declared in a
different header; otherwise the native component's declaration-level header is
already part of generated C++.

Direct native extensions require a declaration-level `@Native` component
target. Ordinary components use Wio-bodied extensions that call native free
functions explicitly. Native extension return values obey the ordinary native
boundary: `ref`/`view` returns remain rejected because native borrow lifetimes
cannot currently be proven.

## 9. Native boundaries and exclusions

Native free functions may pass concrete native component instantiations by
value or supported reference forms. Open generic native functions still use
the 0.9 `@Instantiate(...)` contract where required; const arguments follow the
same concrete-whitelist principle.

Generic object/component SDK export, automatic C++ layout verification, const
generic packs, arbitrary inline const expressions, and general dependent-value
constraints remain outside the 0.10 contract.
