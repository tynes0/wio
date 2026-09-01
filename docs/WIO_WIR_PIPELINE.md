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
destructor capabilities. Object and interface records additionally retain
method names, parameter/return signatures, abstractness, implementation IDs,
and deterministic dispatch slots. These properties are copied unchanged into
Lowered WIR so each backend receives the same ownership, layout, and dispatch
contract.

The initial builder supports top-level functions plus component/object/
interface methods, explicit receiver parameters, parameters, primitive and
reference-family types, contextually typed integer and floating-point constants,
Unicode `text`, `string`, `char`, and nullable `null` constants, direct calls, unary/binary expressions,
direct/virtual/interface method calls, object/interface `fit` and `is`, identity
equality, explicit clamping numeric `fit` conversions, safe implicit numeric
widening, pure conditional values, local declarations and default initialization,
place-based local/field/index assignment, compound assignment, `ref`/`view`
borrows, explicit `deref`, contextually typed and inferred array literals,
dictionary literals and keyed reads/writes, array/string/text index reads,
string/text interpolation, enum/flagset values, `any` boxing/testing/casting,
nullable wrapping, backend-neutral container/string/text intrinsics, `if`/`else` control flow, returns,
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

Every WIR type publishes an ownership model and cleanup kind. Objects,
interfaces, function values, and async tasks are `reference-counted` with
`release-reference`; components and managed values such as string, text, any,
arrays, and dictionaries are `owned-value` with `destroy-value`; `ref` and
`view` values are borrowed and never own their referent. Borrowed parameters
and results also carry caller or lexical lifetime metadata.

A managed `load` is a lexical borrow. When that value crosses an owning
boundary, `copy` creates a new ownership claim. `move` transfers a local claim
without retaining or copying it, and `replace` drops the previous destination
before consuming its replacement. Discarded owned temporaries use `release`;
`drop` closes an initialized local place. Lowered WIR specializes these
operations into intrusive `retain`/`release`/`release-place` for shared objects
and `copy-value`/`drop-value`/`drop-place` glue for owned values.

The builder emits cleanup in reverse lexical order on ordinary scope exit,
`return`, `break`, and `continue` edges. Direct local returns use `move`, while
other returns create an independent owned claim before local cleanup. The
Typed WIR verifier propagates live cleanup-bearing places over the CFG, rejects
double cleanup, replacement of dead storage, inconsistent merge states, and
any function exit that leaves a managed local live.

Field projections are checked against the nominal layout rather than trusted as
free-form strings. Missing fields, type mismatches, writes through read-only
fields, incorrect construction kind, malformed constructor signatures,
ownership/cleanup mismatches, and cleanup operations on trivial values are
rejected in both Typed and Lowered WIR.

## Object and Interface Model

Every method is represented as an ordinary WIR function with a synthetic,
leading `self` reference. Consequently `self`, `deref self`, mutable field
access, and `ref`/`view` self returns follow the same place and borrow rules as
other expressions; a backend does not need a special AST-only interpretation
of the receiver.

Nominal method tables are deterministic. An override keeps the inherited slot,
while a new signature receives the next slot. Interface declarations retain
abstract entries and object implementations replace matching entries without
changing their identity. `method-call`, `virtual-call`, and `interface-call`
remain distinct operations and carry the static owner, selector, slot,
implementation function, receiver, and typed argument signature.

Object conversion is explicit in the IR:

- `upcast` is a statically proven object/interface base conversion;
- `checked-cast` is the runtime-checked object/interface form of `fit`;
- `type-test` is the runtime predicate behind `is`;
- `identity-equal` compares object/interface identity rather than payload.

Both verifiers reject missing method slots, selector/function mismatches,
invalid receiver ancestry, malformed call signatures, wrong dispatch kinds,
and invalid cast/test targets. This freezes one model for the C++ and bytecode
backends instead of allowing each backend to rediscover object semantics.

## Callable Model

Callable identity is resolved before a backend sees the program. A direct
`call` carries the exact declaration id selected by overload resolution, its
concrete argument signature, ordered generic arguments, and a deterministic
specialization key. Backends therefore never repeat overload selection or
invent independent generic-instantiation names.

Function values and closures are explicit:

- `function-ref` materializes one exact named function as a typed value;
- `closure-create` binds a synthetic closure-body function to an ordered
  environment layout;
- `indirect-call` invokes a function value using its verified visible
  signature;
- `extension-call` records the selected extension implementation, receiver
  target, receiver/argument signature, and specialization identity.

Closure layouts distinguish copied value captures, explicit reference
captures, and retained object `self` captures. Hidden environment parameters
are separate from the visible callable signature. Captured values are exposed
to the closure body through stable environment places, while retained `self`
keeps object identity alive for an escaping closure. Both IR levels verify
capture order/kind/type, indirect-call arity and result shape, extension
receiver compatibility, generic metadata, and exact callable targets.

## Value and Container Model

Container construction and access no longer depend on C++ spellings.
`dictionary-create` preserves ordered versus unordered identity plus alternating
concrete key/value types. `dictionary-get` and `dictionary-place` distinguish
keyed reads from mutable storage projection, just as `array-get` and
`array-place` do for positional containers. Wio source now accepts direct
`dictionary[key]` reads and writes; the production native path and WIR path
share the same missing-key failure rule.

`intrinsic-call` freezes the semantic intrinsic family (`array`, `dictionary`,
`string`, `text`, `enum`, or `flagset`), source selector, receiver target, and
complete concrete operand signature. Backends implement that contract instead
of rerunning member lookup. String/text interpolation uses `interpolate` with
ordered literal segments and typed value holes, so nested calls and Unicode
text remain structured until backend emission.

Enum and flagset members use `enum-constant`; enum/flagset operations remain
typed intrinsics. Dynamic values use distinct `any-box`, `any-type-test`, and
`any-checked-cast` operations, while implicit non-null-to-nullable conversion is
`nullable-wrap`. Null remains a valid `any` payload. `Option<T>` and `Result<T>`
retain nominal value-model markers and continue to use verified
`variant-test`/`variant-payload` operations. Standard-library `Tuple` and `Span`
types likewise retain `tuple` and `span` markers even though their ordinary
construction and methods remain nominal component/object operations.

Typed and Lowered verifiers check dictionary key/value pairing and places,
interpolation segment/hole shape, intrinsic signatures, enum/flagset targets,
dynamic source identities, nullable payload types, and nominal value-model
placement. Canonical lowering copies every field unchanged.

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
