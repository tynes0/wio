# Lowered WIR C++ Backend

Wio has two C++ generation paths during the backend migration:

- `legacy` is the default production generator and consumes the analyzed AST;
- `wir` is the canonical experimental generator and consumes only verified,
  optimized Lowered WIR.

Select the new path explicitly:

```powershell
wio file run .\main.wio --cpp-backend wir
wio project build --cpp-backend wir
wio project build --cpp-backend wir --emit-cpp
```

An unknown backend name is rejected. Selecting `wir` never falls back to the
legacy generator: if a construct does not yet have a canonical implementation,
generation stops with a stable `WCPPxxxx` diagnostic and its Wio source span.
This makes differential testing trustworthy and prevents a partially migrated
program from silently using two semantic implementations.

## Backend contract

The backend accepts a `lowered::Module` only after the Lowered WIR verifier and
canonical optimization pipeline have succeeded. It does not perform overload
resolution, infer ownership, rediscover field layout, or inspect AST nodes.

Generated identities derive from stable WIR IDs:

```text
TypeId(12)     -> _wio_t12
FunctionId(7)  -> _wio_f7
GlobalId(3)    -> _wio_g3
ValueId(19)    -> _v19
```

Arbitrary WIR control flow is emitted as a structured block state machine.
Block parameters are assigned on their incoming edges, owned values move on
those edges, and source spans become C++ `#line` directives. This supports
loops, branches, early exits, and future bytecode differential traces without
depending on legal C++ `goto` placement.

WIR places use a generated `Place<T>` adapter. A local place owns optional
storage, while global, field, array, and dictionary projections borrow existing
storage. `place-init`, `load`, `store`, `replace`, and cleanup opcodes therefore
retain their explicit Lowered WIR meaning instead of collapsing back into AST
assignment rules.

## Current executable slice

The opt-in backend currently emits:

- primitive, string, text, any, opaque, nullable, fixed/dynamic array,
  ordered/unordered dictionary, function, async-task, reference, tuple/pack,
  native POD, component, object, interface, enum, and flagset C++ type shapes;
- concrete enum/flagset declarations with their exact canonical underlying
  type and bit pattern, strong constants, flag operators, validity/name
  reflection, and all enum/flagset intrinsics;
- canonical `Option<T>`/`Result<T>` construction, pattern tests and payloads,
  checked `!()` unwrap, `?()` error propagation, and concrete generic value
  layouts while open template declarations remain non-emitted metadata;
- concrete non-variadic generic function bodies, nested and recursive calls,
  generic extensions, specialized closure bodies, and pinned function references;
- component field layouts and intrusive-reference-counted object storage;
- constants, unary/binary/range operations, conversions, direct/extension/
  resolved non-virtual method calls, function references, closures, and
  indirect calls;
- arrays, dictionaries, interpolation, `any`, nullable values, globals,
  locals, projections, borrows, construction, copy/move/retain/release/drop,
  return, branch, conditional branch, and unreachable traps;
- pinned array/dictionary/string/text intrinsics through shared runtime helpers,
  including mutators, numeric string conversions, Unicode operations, and
  canonical `Option` lookup results;
- range and fixed/dynamic array/ordered/unordered dictionary iteration with
  steps, index/key/value projections, reference bindings, and structured exits;
- native headers, native POD spellings, resolved native symbol invocation, and
  wrappers for addressable native functions;
- executable `Entry` adapters and normal Wio backend compilation/linking.

The first parity gate compiles generated source with an independent C++
compiler. A second CLI gate compiles and executes a mutable-local loop through
`source -> Typed WIR -> Lowered WIR -> C++ -> executable`.
The container gate also links and runs generated C++ with array/reference
mutation, ordered/unordered dictionary traversal, successful/missing lookups,
string parsing, emoji/code-point operations, empty iteration, and integer-limit
ranges. Runtime checks reject zero range steps and non-positive container steps;
range advancement terminates before integer overflow. Borrowed loads retain
storage identity instead of copying containers. Iterators do not snapshot their
source: structural changes that invalidate native container iterators are not
supported during traversal.

The following operations remain deliberately rejected before code emission:

- inherited object/interface layout, virtual/interface dispatch, and checked
  hierarchy conversion until the WIR layout has a backend-complete cast table;
- unresolved generic calls and variadic parameter-pack expansion; generic native
  adapters and generic virtual/interface dispatch remain in their respective
  native/hierarchy milestones;
- coroutine suspend/resume/completion and cancellation runtime emission.

Those are parity work inside the C++ backend milestone, not permissions for an
AST fallback. SDK call-table/sidecar bodies and generalized exception adapters
also remain part of the same migration before `wir` can become the default.

## Sprint 17.1: concrete generic bodies

`GenericSpecializer` runs between initial Typed WIR verification and canonical
control-flow/ownership lowering. It consumes pinned function/type identities,
never AST overload lookup. Original declarations remain template metadata;
concrete bodies carry `genericOrigin`, `specializationArguments`, and a canonical
`specializationKey`. A worklist and cache close self/mutual recursion without
duplicating bodies. Invalid bindings produce `WIR3100`; an expansion body limit
produces `WIR3101` instead of unbounded materialization.

Substitution covers signatures, nested value types, captures, local storage,
instruction type metadata, and symbolic fixed-array extents. Const parameters
use explicit `generic-const` instructions until materialization. Integer, bool,
string, and Unicode text constants become ordinary constants; default generic
locals use `default-value`. Cleanup is recomputed for the concrete value:
trivial copies/releases disappear, moves become loads where appropriate, and
managed values retain their copy/move/drop contracts.

```wio
fn Identity<T>(value: T) -> T { return value; }
fn Capacity<const N: usize>() -> usize { return N; }
fn Forward<T>(value: T) -> T { return Identity<T>(value); }
```

Calling `Identity(42)` twice emits one `i32` body; `Forward("wio")` requests a
separate `string` body for both functions. `Capacity<7>()` materializes a body
returning the constant `7`. The `wio_wir_cpp_generics` gate tests generated-code
execution, shared identities, deterministic/idempotent materialization, bounded
failure, recursion, ref mutation, closures, const arrays, generic component
returns, and intrusive object identity. Contextual generic function-reference
source syntax is not expanded by this sprint; the gate also constructs a pinned
function-reference WIR probe directly.

## Cutover policy

`wir` becomes the default only when all release-gate programs pass both
generators, output behavior matches, native/SDK and async surfaces are complete,
and compile-time/runtime benchmarks are recorded. The legacy generator is
removed only after at least one release line with the WIR backend as default;
until then it remains the compatibility oracle for differential tests.
