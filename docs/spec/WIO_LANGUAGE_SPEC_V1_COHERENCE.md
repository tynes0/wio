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

## 5. Conformance boundary

The positive and negative tests linked from `WIO_TRACEABILITY.md` are part of
this candidate contract. A frontend acceptance followed by generated-C++
failure for these rules is a compiler defect; the analyzer and backend must
agree on visible signatures, selected specializations, and match coverage.

