# Wio Language Specification 0.13 - Coherence and Unicode Delta

Status: normative for Wio `0.13.x`

Builds on: specifications 0.8, 0.9, 0.10, and 0.11

Supersedes: `WIO_LANGUAGE_SPEC_V1_COHERENCE.md` for the subjects below

This release freezes the post-0.12 language-coherence work without rewriting
historical specifications. When an older specification, the broad language
reference, and this delta disagree on one of these subjects, this document
wins for Wio 0.13.

---

## 1. Unicode `text`

`text` is a first-class immutable Unicode-semantic primitive. Its source
literals are validated UTF-8:

```wio
let title: text = u"İstanbul 🚀";
let line: text = u$"Title: ${title}";
```

- `u"..."` produces `text`; ordinary `"..."` produces byte-oriented `string`.
- `u$"..."` is interpolated `text`; `$"..."` is interpolated `string`.
- interpolation uses the same nested-token rules as ordinary Wio expressions,
  including calls and nested string/text literals.
- `text` indexing and slicing are code-point based; grapheme and display-width
  operations are explicit library operations.
- invalid UTF-8, unpaired UTF-16 surrogates, and invalid UTF-32 scalars are
  rejected by fallible conversion APIs.
- `text` may convert safely to UTF-8 `string`. A potentially invalid `string`
  converts to `text` only through the fallible Unicode API.
- `text` participates in concatenation, comparison, hashing, matching,
  iteration, console output, generics, and runtime type reflection.

The language does not expose platform `wchar_t` width as part of this model.

---

## 2. Compile-time Textual Values

`const` declarations may contain scalar, enum/flagset, `string`, or `text`
values. Textual constants support literals, references, concatenation,
comparison, matching, and interpolation when every embedded expression is
constant-evaluable.

Functions, aliases, interfaces, components, and objects accept integer,
`string`, and `text` const generic parameters, including trailing defaults,
qualified module constants, exact specializations, and partial
specializations.

```wio
const Product: string = "Wio";
const Greeting: text = u"Merhaba";

component Named<const Name: string = Product> {
    value: i32;
}
```

Textual const values are invariant identities and cannot be fixed-array
extents. Native C++ template mapping remains limited to integer const
parameters; textual const generics use a Wio-owned backend representation and
are not a native ABI promise.

Constant evaluation is bounded to 128 dependency/expression levels, 16,384
visited nodes, and 1 MiB of folded textual data. A cycle or exceeded budget is
a semantic diagnostic, never a generated-C++ failure.

Runtime generic reflection preserves primary source parameter names and the
concrete type/const arguments selected through primary, exact, or partial
specialization.

---

## 3. Fixed-array Extent Inference

A variable with an explicit fixed-array element type may infer its extent from
its initializer using `[T; _]`:

```wio
let ports: [i32; _] = [80, 443];
```

The inferred extent is part of the resulting static type. Empty and nested
fixed-array initializers are supported when the nested shape is rectangular.
Inference is rejected for missing initializers, parameters, non-variable type
positions, dynamic-array sources, or ragged nested initializers.

---

## 4. Typed Attributes and Native Metadata

The compact user-attribute declaration and postfix/scoped application forms
are canonical:

```wio
attribute route(fn)(method: string, path: string = "/")
    with attribute::runtime, attribute::repeatable;

fn Health() -> string with route(path: "/health", method: "GET") {
    return "ok";
}
```

Named arguments are normalized to declaration order. Duplicate, unknown,
missing-required, and positional-after-named arguments are semantic errors.
Folded scalar, `string`, and `text` constants and trailing defaults are stored
in runtime metadata.

Canonical native metadata uses:

```wio
using cpp::header("native_api.h");

fn Call(value: i32) -> i32
    with native, cpp::name(native_api::Call);
```

Legacy `@Native`, `@CppHeader`, `@CppName`, `@Export`, and related built-in
spellings remain compatibility input in 0.13. They have the same semantic
meaning but are not the preferred source form.

---

## 5. Specialization and Extension Selection

After exact specializations, matching partial specializations are ordered
structurally. A candidate wins only when it is uniquely more specialized than
every other best candidate. Repeated-parameter relationships are preserved, so
`Pair<T, Box<T>>` is more specialized than `Pair<T, Box<U>>`. Equivalent or
incomparable best candidates are ambiguous.

Component extension receivers are implementation parameters and are excluded
from user-visible arity. Extension methods support trailing value defaults,
generic deduction, explicit type arguments, trailing generic defaults, and
`where` constraints on concrete component targets.

---

## 6. Match Guards and Exhaustiveness

Every non-`assumed` arm may have a boolean guard. A guard runs only after its
pattern matches, contributes nothing to exhaustiveness, and does not make a
later equivalent arm unreachable.

An Option, Result, or enum value match is exhaustive without `assumed` only
when every variant/member is covered by an unguarded arm. Duplicate unguarded
enum arms are unreachable. `assumed` must be final and cannot have a guard.

---

## 7. Backend Agreement

For all rules in this specification, semantic acceptance followed by a native
C++ compilation failure is a compiler defect. The analyzer and backend must
agree on literal kind, inferred extent, selected specialization, extension
signature, attribute values, reflected arguments, and match coverage.

Representative positive and negative tests are indexed in
[`../WIO_TRACEABILITY.md`](../WIO_TRACEABILITY.md). The focused
`wio_tests_language_coherence` target is the release conformance pack.
