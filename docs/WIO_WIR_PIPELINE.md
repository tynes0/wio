# Wio Intermediate Representation Pipeline

Wio is moving from a direct analyzed-AST-to-C++ path to a backend-neutral
compiler pipeline:

```text
source -> AST -> semantic analysis -> Typed WIR -> Lowered WIR
                                                |-> C++ backend
                                                `-> bytecode backend
```

The WIR path is experimental and opt-in. Ordinary builds still use the proven
AST-to-C++ generator while WIR coverage grows. This keeps current native output
stable and lets the new representation acquire executable parity in measured
slices.

## Typed WIR

Typed WIR is the last language-shaped representation. Every value and
instruction has a stable strong ID, an interned type, and source provenance.
It retains operations such as typed `select` that are useful to optimization
and diagnostics before control flow is made backend-canonical.

The initial builder supports top-level functions, parameters, primitive and
reference-family types, contextually typed integer and floating-point constants,
Unicode `text`, `string`, `char`, and nullable `null` constants, direct calls, unary/binary expressions,
explicit clamping numeric `fit` conversions, safe implicit numeric widening, pure conditional values, local declarations and default initialization,
identifier assignment, compound assignment, `if`/`else` control flow, returns,
`while` loops, C-style `for` loops, `break`, `continue`, short-circuit `and`/`or`,
and expression statements. Logical expressions use explicit right-hand, short,
and merge blocks, so calls in the right operand are only evaluated on the
required edge. Mutable values crossing branches, logical expressions, and loop iterations are represented
as deterministic SSA merge-block parameters rather than hidden memory slots. Unsupported
language constructs fail with stable `WIR2xxx` diagnostics; they never silently
fall back or guess semantics.

Conditional expressions retain `select` only when both alternatives are free
of side effects. Calls and other effectful alternatives are placed in explicit
`conditional.true`, `conditional.false`, and `conditional.merge` regions so an
unselected alternative cannot execute.

Value-producing primitive `match` expressions lower to ordered test and body
blocks. Literal alternatives, multi-value alternatives, inclusive/exclusive
ranges, guarded cases, and the final Wio `assumed` fallback are supported. The
matched subject is evaluated once. Statement-form matches use the same control
flow, merge mutated locals through block parameters, and preserve normal
fallthrough when no `assumed` case is present. Option/Result, array, and
payload-carrying patterns use explicit backend-neutral data-model operations:
`variant-test`, `variant-payload`, `array-length`, and `array-element`. Pattern
bindings are projected only on matching control-flow paths and dominate both
their guards and bodies. This keeps C++ and future VM backends from recreating
Option/Result or array pattern semantics independently.

## Lowered WIR

Lowered WIR is the shared backend contract. Its first canonicalization pass
turns a typed conditional value into explicit blocks, conditional jumps, jumps
with block arguments, and a merge-block parameter. The verifier checks target
existence, argument arity and types, terminators, value definitions, and use
sites before any backend consumes the module.

The deterministic pass order is currently:

1. `verify-typed-wir`
2. `lower-canonical-control-flow`
3. `verify-lowered-wir`

Future lowering stages will own object/component layout, async state machines,
cleanup and lifetime edges, native ABI adaptation, and other semantics that
must be identical for C++ and bytecode.

## Inspecting WIR

For a source file:

```powershell
wio file typed-wir .\main.wio
wio file lowered-wir .\main.wio
wio file lowered-wir .\main.wio --ir-output .\artifacts\main.lowered.wir
```

The raw compiler accepts the equivalent `--emit-typed-wir` and
`--emit-lowered-wir` flags. Without `--ir-output`, outputs are named
`<source>.typed.wir` or `<source>.lowered.wir`; `--intermediate-dir` moves the
default output into that directory.

For a manifest project:

```powershell
wio project build --emit-typed-wir
wio project build --emit-lowered-wir --ir-output .\.wio-build\module.lowered.wir
```

During the early coverage phase, a minimal project can explicitly pass
`--no-builtin` to inspect only its own program. This is not an implicit compiler
behavior: projects that depend on standard-library bodies should keep builtin
merging enabled and will receive precise unsupported-WIR diagnostics until the
needed constructs land.

Only one emission mode may be active. WIR emission cannot be combined with
`--emit-cpp`, `--dry-run`, or `--run`, and `--ir-output` requires a WIR mode.

## Backend Cutover Rule

The C++ backend will move from the AST to Lowered WIR only after the following
are true for a language slice:

- semantic information is preserved without recovery guesses
- Typed and Lowered WIR verifiers cover its invariants
- WIR printer output is deterministic
- existing native behavior has parity tests
- failures retain source locations and stable diagnostic codes

Bytecode work starts from the same verified Lowered WIR contract. It does not
introduce a second language semantic implementation.
