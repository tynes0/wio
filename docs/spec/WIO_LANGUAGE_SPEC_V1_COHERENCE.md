# Wio v1 Language Coherence Candidate Specification

Status: normative release-candidate contract  
Target edition: v1  
Builds on: language specifications 0.8, 0.9, 0.10, and 0.11

This document freezes the coherence rules completed after Wio 0.12 without
rewriting the historical versioned specifications. If an older specification
and this document differ for one of the subjects below, this document is the
current v1-candidate rule.

## 1. Typed attribute declarations and applications

The compact declaration form is canonical:

```wio
attribute route(fn)(method: string, path: string = "/")
    with attribute::runtime, attribute::repeatable;
```

The parenthesized list after the attribute name is the target set. The second
list contains typed parameters. Declaration policies use the
`attribute::source`, `attribute::compile`, `attribute::runtime`,
`attribute::repeatable`, `attribute::inherited`, `attribute::scoped`, and
`attribute::conflict("group")` policy attributes. The older verbose
`for`/`retain` declaration form remains a compatibility spelling until an
edition boundary.

User-defined attribute applications accept positional or named arguments:

```wio
fn Health() -> string with route(path: "/health", method: "GET") {
    return "ok";
}
```

Named arguments are normalized to declaration order. An argument name must be
declared exactly once, positional arguments cannot follow named arguments, and
every required parameter must receive a value. Built-in compiler attributes
remain positional until their migration to typed declarations is complete.

Typed user-defined attribute arguments are compile-time metadata. A scalar,
`string`, or `text` argument may name an evaluable `const`; the metadata stores
the folded value rather than the identifier spelling. Missing trailing
arguments are materialized from their declared defaults before validation and
runtime reflection. A `string` parameter accepts a byte-string literal/value;
a `text` parameter accepts a Unicode `u"..."` literal/value. Neither spelling
implicitly crosses that boundary.

```wio
const Root: string = "/api";
const Title: text = u"İstanbul";
const Revision: i32 = 7;

attribute endpoint(component)(path: string, title: text, revision: i32 = Revision)
    with attribute::runtime;

component Dashboard with endpoint(title: Title, path: Root) {}
```

## 2. Generic closers and specialization ordering

In a generic type, parameter list, or explicit generic call, adjacent `>>`,
`>=`, and `>>=` tokens are split contextually into the required `>` closers and
remaining operator token. They retain their shift/comparison/assignment
meaning in expression grammar.

Specialization selection follows this order:

1. a matching exact specialization wins;
2. otherwise all matching partial specializations are compared structurally;
3. pattern A is strictly more specialized than pattern B when B accepts A and
   A does not accept B;
4. a unique strictly most-specialized pattern wins;
5. incomparable or structurally equivalent best matches are ambiguous;
6. the primary declaration is used when no specialization matches.

Structural comparison preserves repeated-parameter relationships. Therefore
`Pair<T, Box<T>>` is more specialized than `Pair<T, Box<U>>`; it is not merely
assigned the same numeric specificity score.

## 3. Component extension methods

An extension receiver is an implementation parameter and is absent from the
user-visible callable signature. Diagnostics, overload arity, default
parameter completion, and generated wrapper selection all use that visible
signature.

Extension methods support:

- trailing value defaults;
- generic type deduction;
- explicit type arguments;
- trailing generic defaults;
- `where` constraints and their compatibility attribute representation.

```wio
extension MarkerTools for Marker {
    public view fn Identity<T>(value: T) -> T { return value; }
    public view fn Zero<T = i32>() -> T { let value: T; return value; }
    public view fn Twice<T>(value: T) -> T where T: traits::IsInteger {
        return value + value;
    }
}
```

Generic extension targets such as an extension declaration parameterized over
`Box<T>` are not in this v1 slice. Generic methods on a concrete component
target are in the slice.

## 4. Match guards and exhaustiveness

Every non-`assumed` match arm may have a boolean `if` guard. A guard runs only
after its value, alternative, range, or destructuring pattern matches. A
guarded arm neither makes a later equivalent arm unreachable nor contributes
to exhaustiveness. `assumed` is final and cannot have a guard.

Value-producing matches are exhaustive without `assumed` when one of these is
true:

- both unguarded `Some` and `None` variants are present;
- both unguarded `Ok` and `Err` variants are present;
- every declared member of the matched enum has an unguarded arm.

Repeated unguarded enum members are unreachable. A non-exhaustive enum value
match must include a final `assumed` arm.

## 5. Textual const generics

Const generic parameters may use an integer type, `string`, or `text`.
Functions, aliases, interfaces, components, and objects share the same rules.
Textual arguments may be direct literals, earlier compatible const parameters,
or evaluable const declarations, including qualified constants from merged
modules.

```wio
const Product: string = "Wio";
const Greeting: text = u"Merhaba";

component Named<const Name: string = "unnamed"> { value: i32; }
fn Value<const Label: text>() -> text { return Label; }

let named: Named<Product> = Named<Product>(1);
let greeting = Value<Greeting>();
```

`string` and `text` values are invariant generic identities. The runtime-safe
conversion from `text` to UTF-8 `string` does not make `Named<u"x">` compatible
with a `const Name: string` slot. Textual values may select exact/partial
specializations and defaults. They cannot be static-array extents.

The backend represents textual const generics with Wio-owned structural C++20
values and converts them to ordinary `string`/`text` when referenced in a Wio
body. That representation is not a native ABI promise. Declaration-level
native components and native generic functions therefore continue to permit
only integer const parameters.

## 6. Conformance boundary

The positive and negative tests linked from `WIO_TRACEABILITY.md` are part of
this candidate contract. A frontend acceptance followed by generated-C++
failure for these rules is a compiler defect; the analyzer and backend must
agree on visible signatures, selected specializations, and match coverage.
