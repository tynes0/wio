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

Named types retain their semantic category instead of collapsing to a backend
spelling: `component`, `object`, `interface`, `enum`, and `flagset` are distinct
nominal kinds. Components use value semantics while object/interface types are
identity-bearing handles. Native POD components additionally carry the
`native-pod` representation marker. Nominal records also retain ordered field
layouts, field types, mutability, visibility, base types, and constructor/
destructor capabilities. These properties are copied unchanged into Lowered
WIR so each backend receives the same ownership and layout contract.

The initial builder supports top-level functions, parameters, primitive and
reference-family types, contextually typed integer and floating-point constants,
Unicode `text`, `string`, `char`, and nullable `null` constants, direct calls, unary/binary expressions,
explicit clamping numeric `fit` conversions, safe implicit numeric widening, pure conditional values, local declarations and default initialization,
place-based local/field/index assignment, compound assignment, `ref`/`view`
borrows, explicit `deref`, contextually typed and inferred array literals, array
index reads, `if`/`else` control flow, returns,
`while` loops, C-style `for` loops, `break`, `continue`, short-circuit `and`/`or`,
and expression statements. Logical expressions use explicit right-hand, short,
and merge blocks, so calls in the right operand are only evaluated on the
required edge. Immutable temporary values crossing branches, logical expressions,
and loop iterations use deterministic SSA block parameters. Addressable source
variables instead keep a stable explicit place across those edges. Unsupported
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

Ordinary array expressions use `array-create` and `array-get`. Array literal
elements are converted against the semantic element type before construction,
and inferred literal arrays retain their fixed extent in the WIR type table.

## Place and Memory Model

Addressability is explicit and backend-neutral. `local-place` creates storage;
`place-init` performs its declaration-time initialization; `load` reads a value;
and `store` mutates only a mutable reference. `field-place` and `array-place`
project stable sub-places without copying their aggregate, while `borrow`
weakens a mutable `ref T` to a read-only `view T`. The verifier rejects stores
through views, reference-type mismatches, non-integer array indices, and any
projection that attempts to strengthen mutability.

Wio's source ergonomics remain unchanged: references to primitives, components,
and arrays auto-read in value contexts, while object/interface references retain
identity unless explicitly dereferenced. Typed WIR records every implicit read,
so C++ and bytecode backends do not independently reconstruct that rule. Local,
field, and indexed mutation—including compound assignment—now use the same place
operations and remain valid across structured control-flow edges.

## Construction and Lifecycle

Construction distinguishes stack/value components from identity-bearing object
allocations. `construct-component` creates an independent component value;
`construct-object` creates an owning object handle. Each instruction records the
selected constructor's stable name and typed argument signature, which both
verifiers check before a backend consumes it.

`drop` closes the lifetime of an addressable component value or releases one
owning object handle. Component loads therefore represent value copies, while
object loads represent strong-handle copies; dropping the last object handle is
the edge that invokes `OnDestruct`. `store` replaces the previous destination
value under the same ownership rules. The builder emits drops in reverse lexical
order on ordinary scope exit, `return`, `break`, and `continue` edges. A return
value is materialized before local cleanup, preserving the returned owner/value.

Field projections are checked against the nominal layout rather than trusted as
free-form strings. Missing fields, type mismatches, writes through read-only
fields, incorrect construction kind, malformed constructor signatures, and
non-nominal drops are rejected in both Typed and Lowered WIR.

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

Future lowering stages will own async state machines, exceptional cleanup edges,
native ABI adaptation, and other semantics that
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
