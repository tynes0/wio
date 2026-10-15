# Wio v1 Surface Consolidation Plan

Status: **accepted pre-v1 release blockers; scoped implementation in progress**.
Decision recorded on **2026-10-07**. This document changes the intended v1
contract, not the behavior of the current compiler or released std.
The std source-style and explicit-visibility requirements below were added on
**2026-10-13**; they are accepted cleanup requirements, not a completed migration.

On 2026-10-15 std consolidation was integrated module by module into native
optimization pass 1. The first `builders`/`binary` slice implements the accepted
source rules and Result-first ByteReader APIs, with native/VM correctness gates.
See [scope, migration and measurements](WIO_STD_BUILDERS_CONSOLIDATION.md).
Other std/intrinsic APIs and language-wide bracket-only retirement remain open;
this is not a completed v1 or SDK migration.

These requirements refine the existing language-coherence, std-correctness,
migration, tooling, SDK-parity and performance gates. They do not renumber the
backend closure list or silently replace any existing v1 gate. Complete this
pass before beta and qualify its performance again in the final integrated
optimization stage.

## 1. Result-first standard APIs

### Accepted contract

- `std::Result<T>` is the single default recoverable-failure model for standard
  APIs and compiler-provided member methods. Success returns a value; failure
  returns a standardized `ResultError` with a stable domain/code and useful
  context.
- `Foo(...)` exposes the result, `Foo!(...)` explicitly unwraps or panics, and
  `Foo?(...)` unwraps or propagates the error from a Result-returning caller.
  Member and generic calls must behave consistently with free functions.
- Missing values, missing keys and invalid indices in checked retrieval use
  Result errors rather than an automatically selected `Option` return.
  Invalid values, parse failures, range failures, timeouts and cancellation
  retain distinguishable error contracts rather than collapsing into one
  generic failure.
- This does **not** wrap every infallible operation in Result. Methods such as
  `Count()` and `Empty()` retain their direct return types when their semantic
  operation cannot fail.
- `std::Option<T>` remains an explicitly chosen user-facing optional-value
  utility, analogous in purpose to C++ `std::optional`. It is not removed from
  the language/library, and user-authored Option fields, generic composition,
  matching and SDK round trips must remain supported.
- Standard operations must not force users through `Option.Value()` or
  `IsSome()` merely to retrieve a checked value. Do not replace this with an
  implicit Option-to-Result conversion or new Option-specific `?`/`!` sugar.
  Explicit Option construction and conversion helpers may remain for users
  who deliberately choose that type.

Proposed call-site contract after implementation, **not current behavior**:

```wio
items.Get(index)  // Result<T>: value or a standardized index error
items.Get!(index) // T: explicit unwrap; panic on error
items.Get?(index) // T: propagate the error from a Result-returning caller
```

### Required audit and implementation

- Inventory Option-returning std and intrinsic operations across arrays,
  strings, dictionaries, spans/views, algorithms, iterators, JSON, numeric
  helpers, reflection and async helpers. Include nested Option results and
  cross-module generic signatures, not only intrinsic `Get`.
- Inventory fallible operations currently returning raw values or panicking,
  including parse and strict retrieval, and give them the same checked Result
  contract. Resolve redundant `At`/`Get`/`First`/`Last` spellings in the std API
  audit rather than preserving conflicting failure models by accident.
- Standardize missing-index, missing-key, missing-value and invalid-value
  error categories and their source/context details. Reuse existing error
  infrastructure where appropriate; freeze the domain/code mapping with tests.
- Keep async pending/readiness status distinct from a completed failure.
  Migrate timeout/completion payloads and checked task helpers without treating
  ordinary pending work as an error or weakening cancellation semantics.
- Update semantic intrinsic typing, overload resolution, Typed/Lowered WIR,
  cleanup and Result propagation, C++ lowering, VM execution, native bridges,
  reflection descriptors and SDK signatures together. Remove automatic Option
  selection for standard operations, not support for explicit user Option
  values.
- Update CLI consumers, templates, examples, tests, documentation, SDK samples
  and editor signatures. Publish return-type/name migration guidance and
  actionable diagnostics before freezing the replacement surface.

Acceptance: canonical checked retrieval and fallible APIs work with ordinary
calls, `!()` and `?()` on both backends; errors preserve their identity and
cleanup is correct on propagation. The audited standard-operation inventory
has no remaining implicit Option returns, except explicitly opt-in Option
construction/conversion utilities. User-chosen Option remains usable and
host-visible. No silent compatibility coercion masks an old return type.

## 2. Bracket-only attribute application

### Accepted contract

- Declaration-leading `[Attribute]` is the only supported attribute attachment
  spelling in the final v1 compiler. Preserve its qualified names, arguments,
  grouped/stacked applications and existing typed validation/processor model.
- Remove compatibility parsing of `@Attribute(...)` and postfix attribute
  `with ...` applications before v1, including maintained native/export and
  attribute-policy/composition declarations that still use these spellings.
- Preserve scoped `using` activation, including
  `using cpp::header("header.h");`. It expresses lexical activation rather
  than a competing attachment spelling and was explicitly retained by the
  design decision.
- Do not remove unrelated language uses of a token merely because it also
  appeared in an old attribute grammar.
- Built-in canonical names and their existing aliases remain accepted inside
  bracket attributes: `[Native, CppName("engine::Tick")]` and
  `[native, cpp::name("engine::Tick")]` resolve to the same contracts. The
  2026-10-09 decision retains these names; retiring attachment syntax does not
  retire built-in name aliases or make user attributes case-insensitive.
- A dedicated migration tool may still recognize legacy source as input and
  rewrite it; this is not permission for the v1 compiler to accept it.

### Required migration and acceptance

Inventory compiler/runtime/std/CLI/SDK declarations, tests, examples, generated
Wio, templates and all maintained editor/documentation repositories. Convert
maintained input first, retain historical release notes as history, then retire
legacy parser branches and semantic normalization paths. Diagnose obsolete
spellings with a clear bracket-syntax replacement rather than silently accepting
them. Update lexer/parser tests, formatter, migration tooling, IDE grammars,
completion/snippets and source examples in the same delivery window.

Acceptance: bracket attributes and scoped `using` preserve metadata, native
mapping, export, reflection and behavioral semantics on both backends. Legacy
attachment syntax is rejected with tested migration diagnostics; maintained
current source does not depend on it. No capability is lost merely because its
attachment spelling changed.

## 3. Standard library standardization, cleanup and performance pass

### API and implementation cleanup

- Produce a module-by-module inventory of canonical operations, names,
  overloads, parameter conventions, return types, mutability/ownership,
  bounds behavior, errors, Unicode units and native requirements.
- Apply the Result-first contract throughout that inventory. Remove `Try...`
  function/method spellings from the canonical std surface and migrate their
  consumers to ordinary names with the fixed Result-based failure contract.
  Remove redundant `...Result`, `...Raw` and other duplicate compatibility
  surfaces when they only expose a second failure/unwrap convention.
- Preserve distinct nonblocking, probing or low-level capabilities, but do not
  keep `Try...` as an alternative public failure model. Give necessary operations
  meaningful canonical names or consolidate their implementation internally.
  Pending/readiness is not itself an error; keep its semantics separate from
  completed failure. Infallible operations still return their values directly.
- Remove unused modules/helpers, duplicated wrappers and obsolete convenience
  APIs only after checking consumers and publishing a replacement/migration
  path. Consolidate shared implementations and avoid layering overhead.
- Treat compiled-in methods as part of the same std-facing contract. Keep the
  builtin-method reference synchronized with implementation; document each
  surviving operation once with its complete signature and failure behavior.

### Std source-style and readability requirements

These are mandatory acceptance criteria for the std cleanup, including native
declarations. They are std source conventions, not new syntax restrictions on
user Wio projects. Source migration, formatter support, generated std-facing
declarations and maintained std API examples must agree on the rules.

1. **No one-line function or method bodies.** Expand every function/method body
   over multiple lines, even if it only returns a value or is empty. The early
   return exception in rule 8 applies to an `if` statement, not a function body.
2. **One blank line after closing a block.** Follow every function, method and
   control-flow block's closing brace with exactly one empty line before the
   next source line. This also applies between nested closing braces and before
   an `else` continuation; do not write `} else {` on one line. A required
   semicolon remains attached to its closing declaration. Literal/initializer
   delimiters are not control-flow blocks and must not be reformatted as such.
3. **Private object defaults and explicit access.** Std objects keep the private
   member default; do not override it to public/protected. Write `private` or
   `public` explicitly on every function/method signature and object member,
   including native signatures, so access is visible at the declaration. Keep
   intended public APIs explicitly public and helpers explicitly private; this
   rule is not blanket privatization or an excuse to widen access silently.
4. **Bracket-only attributes on separate lines.** Use only `[Attribute]`
   attachment, never legacy `@...` or postfix attribute `with ...`. When short
   enough, group the declaration's attributes in one bracket list; split long
   groups across multiple bracket lists. Attribute lists precede the declaration
   on their own lines, never share the signature's line, and remain attached to
   that declaration. Scoped `using cpp::header(...)` is unchanged.
5. **Width-based parameter wrapping.** Keep parameters on the signature line
   when they fit the configured line-width limit. Otherwise wrap deterministically
   between parameters into indented continuation lines, continuing to respect
   the limit. Never split tokens or damage generic/type/attribute expressions.
6. **One Result-based fallible API, no Try family.** Remove `Try...` std API
   spellings and parallel failure conventions. Canonical fallible operations
   return `Result<T>` and support ordinary, `!()` and `?()` calls. Keep infallible
   operations direct and Option as an explicit user utility, as in section 1.
7. **Separate all declarations.** Put exactly one blank line between definitions
   and declarations, even when they are bodyless native functions. Treat an
   attribute group and its following declaration as one declaration unit; the
   separating blank line goes before the next unit, not inside the group.
8. **Braced blocks with one narrow exception.** Every control-flow body uses
   braces, including `if`, `else`, loops and block-valued match branches. Only a
   single-statement early-return `if` is written without braces, on one line:
   `if (condition) return value;`. Multi-statement branches, `else`, loop bodies,
   and break/continue/throw cases do not inherit this exception. Function/method
   bodies still follow rule 1.
9. **Space before control-flow parentheses.** Where parentheses are used after
   a control-flow keyword, use `if (...)`, `match (...)`, `for (...)`,
   `while (...)`, etc., never `if(...)`. This is not a request to add parentheses
   where the Wio grammar does not require them, or to write `Function (...)`.

The numeric line-width limit is not frozen yet. **120 columns** is the initial
proposal, consistent with the repository's C++ `.clang-format` setting, for both
parameter wrapping and attribute grouping. The Wio formatter needs its own std
profile: the C++ formatter does not implement these Wio-specific blank-line,
visibility, attribute and early-return rules. Additional style decisions can be
recorded later; this note does not quietly introduce more mandatory conventions.

Acceptance: audit all std modules and native declaration groups against all nine
rules; add focused syntax-aware style/formatter fixtures for nested blocks,
`else`, early returns, grouped/stacked attributes, generic/multiline signatures
and explicit visibility. Formatting must be deterministic and idempotent, while
the API/access/Result migration must preserve intended semantics, produce useful
migration diagnostics and pass both-backend/SDK acceptance. Do not use a blind
brace or declaration regex to rewrite strings, interpolation, comments or literals.

### Performance requirements

- Profile representative algorithms, collections, builders, string/text,
  byte/buffer/pool, parsing/formatting, serialization, hashing/random and async
  std operations. Measure native and VM paths separately.
- Optimize measured complexity, allocation, copying, ownership traffic,
  iteration, bounds checks, conversions and native-boundary overhead while
  preserving correctness and lifetimes. In particular, prove amortized builder
  append behavior and efficient byte/buffer output rather than hiding repeated
  whole-value copying behind a builder API.
- Measure Result success and failure paths, explicit user Option storage and
  generated code/build costs; do not introduce avoidable heap wrappers or
  dynamic machinery merely to express the new API.
- Record reproducible Debug/Release baselines, workloads, toolchains,
  correctness checks, memory/allocation data and regression budgets. Compare
  equivalent native algorithms when useful; do not claim "fastest" or a fixed
  performance ratio without evidence.

Acceptance: the canonical std inventory, migration guide, docs and editor/SDK
surface agree, and the std source-style requirements above are satisfied;
focused positive/negative, lifetime and cross-platform
conformance tests pass; both backends agree on errors and results; measured
hotspots are addressed and reviewed budgets pass. The final pre-beta heavy
optimization stage requalifies the combined compiler/runtime/std product.

## Delivery boundaries

This is a planning decision, not a completed migration. Existing reference
signatures continue to describe current implementation until changed by tested
implementation slices. Complete source migration and both-backend/SDK parity
before removing compatibility paths, and close all three workstreams before
the v1 feature freeze. No release tag or version bump is implied by this note.
