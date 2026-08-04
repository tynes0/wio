# Wio Language Specification 0.9 — Generics and Constraints

Status: normative for the Wio 0.9 generic type-parameter, default,
specialization, constraint, and compatibility surface. Const/non-type generic
parameters, associated types, generic interface methods, and generic component
methods are not defined by this version.

## 1. Generic declarations

Functions, type aliases, interfaces, components, and objects may declare type
parameters between `<` and `>`. A declaration may have at most one type-parameter
pack, written `Ts...`, and it must be last.

```text
generic-parameter-list := "<" generic-parameter { "," generic-parameter } ">"
generic-parameter      := identifier [ "=" type ] | identifier "..."
```

The parameter names enter scope from left to right. A default type may refer to
an earlier parameter but not to itself or a later parameter.

## 2. Default type arguments

Once a fixed parameter has a default, every later fixed parameter must also
have a default. A pack cannot have a default.

```wio
component Pair<T = i32, U = T> {
    public first: T;
    public second: U;
}
```

Explicit arguments bind the leftmost parameters. For callable inference, type
deduction binds every parameter it can observe in the callable signature;
defaults then fill only unresolved fixed parameters, in declaration order.
Substitution is applied to dependent defaults before the next parameter is
filled. A missing parameter without a default is an arity or inference error.

The canonical instantiated type contains the completed argument list. Thus
`Pair`, `Pair<i32>`, and `Pair<i32, i32>` denote the same type in the example.

## 3. Specialization declarations

Only generic object and component primaries may be specialized. The primary
must already be visible in the same merged realm scope.

A full specialization has no parameter list and supplies a concrete argument
for every primary parameter:

```wio
@Specialize(i32, string)
component Pair {
    public first: i32;
    public second: string;
}
```

A partial specialization declares its own pattern variables. Every declared
pattern variable must occur in the specialization pattern.

```wio
@Specialize(T, string)
component Pair<T> {
    public first: T;
    public second: string;
}
```

Specialization selection uses this strict ordering:

1. A matching full specialization wins.
2. Otherwise, matching partial specializations are ranked by the number and
   structure of their concrete pattern positions.
3. A unique most-specific partial specialization wins.
4. Equal best matches are an ambiguity error at the instantiation site.
5. If no specialization matches, the primary wins.

Declaration order never breaks a tie. Duplicate full patterns are invalid.
Normal module merge and import visibility determine whether the primary and
specializations participate; there is no separate global specialization
registry.

## 4. Constraints

`@Apply(...)` is the canonical constraint representation. Arguments in one
attribute are positional and conjunctive across parameters. Multiple
`@Apply(...)` attributes are alternative allowed rows. A row slot accepts a
concrete type, a supported `std::traits` predicate, a user-defined nominal
trait interface, `true`, or `false`.

`where` is readable syntax for one constraint row:

```text
where-clause := "where" where-entry { "," where-entry }
where-entry  := generic-parameter-name ":" trait-type-name
                { "+" trait-type-name }
```

```wio
fn Twice<T>(value: T) -> T where T: std::traits::IsInteger {
    return value + value;
}
```

The compiler supplies the named generic operand, so `T: Trait` is equivalent
to the `T` slot containing `Trait<T>`. Predicates joined by `+` are
conjunctive; the concrete binding must satisfy all of them. Unmentioned fixed
slots are `true`. Unknown or repeated parameter names are errors. A pack
parameter supplies its entire pack operand to a pack-capable predicate.

Constraints are checked after explicit arguments, deduction, and defaults have
formed a candidate binding. A rejected candidate does not participate in
overload ranking. If all otherwise viable candidates are rejected, the
diagnostic names the callable/type and completed concrete argument list.

## 5. Generic compatibility and variance

Generic instantiations are invariant in every argument. `Container<Derived>`
does not implicitly convert to `Container<Base>`, even if `Derived` converts to
`Base`. Exact canonical argument equality is required, except for compatibility
that is explicitly declared by nominal object/interface inheritance on the
fully instantiated types.

No covariance or contravariance annotation exists in 0.9. `ref`, `view`, and
nullable layers follow their own compatibility rules but do not change the
invariance of the generic type beneath them.

## 6. Native and export instantiation

Open generic ABI surfaces are not emitted. Generic `@Native` and `@Export`
functions use `@Instantiate(...)` to declare concrete bindings. Trailing fixed
arguments may be omitted when their parameters have defaults; defaults are
substituted before code generation, symbol mangling, and whitelist checks.

For a trailing pack, additional `@Instantiate(...)` operands are its concrete
element types. Predicate expansion applies only to fixed parameters. Exported
pack functions require at least one concrete instantiation row.

Native component specialization and generic object/component export remain
outside the 0.9 contract.

## 7. Variadic meta surface

Type and value packs expose compile-time `.size` and indexing within the
documented pack-expression subset. `std::meta` defines `First`, `Second`,
`Last`, `Penultimate`, `TypeCount`, `ContainsType`, `AllSame`, `IndexOf`, and
`UniqueCount`, together with `Types<Ts...>` and `Values<Args...>` helpers.

This is not an ordinary non-type generic system. Declarations such as
`Vector<T, N>`, general value substitution, associated types, generic default
implementations, and arbitrary compile-time transforms are reserved for a
later specification.
