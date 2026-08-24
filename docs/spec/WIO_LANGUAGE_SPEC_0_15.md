# Wio 0.15 Typed Attribute Specification

Status: normative delta for Wio 0.15.0.

This document freezes the attribute behavior added after Wio 0.14. Rules not
changed here inherit the 0.13 language-coherence and 0.14 SDK contracts.

## 1. Source form and identity

The canonical application form is declaration-leading `[Name]` or
`[Name(arguments)]`. Lists may be stacked or grouped. Qualified names and named
arguments use ordinary Wio lookup and parameter ordering. Legacy `@Name`
remains accepted migration input, but generated examples use brackets.

Built-in and user attributes share canonical declaration identities. Target,
retention, repeatability, composition, requirements, conflicts, allow-lists,
exclusivity, cardinality, and before/after ordering are validated over the
expanded effective set. Composition cycles and ordering cycles are errors.

## 2. Processor boundaries

- `Validator<any>` is target-independent. `Validator<T>` requires an exported
  component/object target compatible with `T`. `Validate() -> bool` must fold
  at compile time; an optional constant `Diagnostic() -> string` supplies the
  rejection text.
- `PreProcessor.Before()` runs before the body. A method hook may receive
  `receiver: any` or `receiver: view T`; the latter requires target
  compatibility. A boolean pre hook may skip only a unit-returning target.
- `PostProcessor.After()` runs after successful result evaluation and may
  observe `After(result: T)` with the exact target result contract.
- `FinallyProcessor.Finally()` runs exactly once on normal or exceptional exit
  and may observe `Finally(succeeded: bool)`.
- synchronous `AroundProcessor.Around(proceed: fn() -> T) -> T` may invoke
  `proceed` zero or one time. The capability cannot escape and duplicate calls
  fail deterministically. Async around is a compile-time error in 0.15.
- async functions execute pre/post/finally inside the coroutine on its executor;
  no hook causes an implicit caller-thread wait.

Behavioral order is deterministic. Entry phases follow the resolved
before/after order; exit phases unwind in reverse. Unsupported targets and
malformed hooks are errors, never accepted no-ops.

## 3. Checked derive

`DeriveProcessor<TTarget>` may expose public methods marked
`[std::attribute::DeriveMember]`. The first explicit parameter is a hidden
target receiver (`any` or immutable `view TTarget`); remaining parameters and
the result become the generated member signature. Each call owns an isolated,
default-constructed processor instance.

Wio 0.15 derive is deliberately method-only. It cannot change layout, add
fields/properties/constructors, mutate arbitrary AST, perform I/O, or inject
unchecked code.

## 4. Reflection and host ABI

Runtime reflection preserves canonical identity, normalized arguments, default
provenance, origin, retention, and method behavioral pipelines. Module ABI v9
adds `WIO_MODULE_CAP_ATTRIBUTE_METADATA_V1` and exposes retained attributes on
exports, types, fields, and methods, including ordered processor phase records.

## 5. Migration

`wio migrate attributes PATH --check` reports `.wio` files containing legacy
applications. `--write` converts them. The scanner does not rewrite strings or
comments. Postfix `with` remains compatibility input and requires contextual
migration where it overlaps non-attribute clauses.

