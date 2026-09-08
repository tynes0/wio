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
- component field layouts, declaring-subobject field projections, and
  intrusive-reference-counted object storage with a shared hierarchy root;
- constants, unary/binary/range operations, conversions, direct/extension/
  resolved non-virtual, virtual and interface method calls, function references, closures, and
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
- async bodies, task payloads, canonical suspension/resumption/completion,
  cooperative cancellation checkpoints and executor transitions;
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

- unresolved generic calls and variadic parameter-pack expansion; generic native
  adapters remain in the native milestone; open method-level generic contracts
  are not emitted as runtime virtual slots.

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

## Sprint 17.2: object and interface execution

`lower-object-hierarchy` pins each nominal type's transitive `castTypes`,
per-contract `dispatchEntries`, default constructor and destructor. A dispatch
key is `(contract TypeId, local slot)`; different interfaces may use the same
slot number without colliding. Generic owner methods and lifecycle bodies are
materialized before this pass. Field places carry their declaring type and
local storage index, including fields inherited from a base object. The
Lowered verifier rejects modified cast/dispatch/lifecycle tables (`LIR1530`),
unknown implementations (`LIR1531`), and mismatched field projections
(`LIR1532`). Invalid hierarchies fail lowering with `WIR3200`.

The C++ backend emits base classes before derived classes and one virtual
intrusive runtime root across object/interface views. Contract-specific virtual
thunks call the already-selected WIR function; `super` remains a direct call.
`is` inspects dynamic type, `fit` checks and rejects a mismatched target, and
identity comparisons normalize interface pointers to the shared runtime object.
Objects boxed in `any` retain dynamic hierarchy casts, not only the boxed static
type. Generated type IDs here are module-local; stable cross-module SDK dispatch
and adapters remain Sprint 17.4 work.

```wio
interface IRead { fn Read() -> i32; }
[From(IRead)] object Counter {
    public value: i32;
    OnConstruct(value: i32) { self.value = value + 1; }
    public fn Read() -> i32 { return self.value; }
    public fn Own() -> Counter { return deref self; }
}
fn ReadCounter(counter: view IRead) -> i32 { return counter.Read(); }
fn Entry() -> i32 {
    let counter = Counter(6);
    let reader = counter fit IRead;
    if (ReadCounter(counter) == 7 and reader.Read() == 7) { return 0; }
    return 1;
}
```

Constructor calls now carry the semantic analyzer's selected callable identity,
including overloads: bodies execute instead of being approximated by field-wise
initialization. Base default constructors precede derived constructor bodies;
destructors run derived-to-base. `self`, object borrows and borrowed object loads
do not increment the strong count. Owning `deref self` results do. Ordinary
same-type handle places preserve write-through storage; raw self borrows do not
represent rebindable handle storage. Physical destruction follows the existing
`Ref`/`WeakRef` runtime contract: a surviving weak reference delays physical
destruction, while locking a zero-strong object already fails. This sprint does
not change production lifetime semantics.

`wio_wir_cpp_objects` compiles and runs the new source fixture plus C++ boundary
probes: overload/default constructors, multi-field and inherited field access,
three-level virtual dispatch, shared interface paths, `super`, generic
object/interface methods, generic destruction, owning/borrowed self returns,
`any`, scope cleanup, failed casts, shared strong counts and weak lifetime.
It also corrupts dispatch/field metadata and requires rejection before emission.
The source-language restrictions on nullable `is` operands and interface
inheritance declarations are unchanged; this is backend parity, not new syntax.

## Sprint 17.3: async and coroutine execution

The C++ backend consumes the existing canonical coroutine states, without
consulting the AST. `coroutine-suspend` emits a C++20 `co_await`, then transfers
to its declared resume block; `coroutine-resume` moves the payload from a
state-specific optional and clears it. `coroutine-complete` emits `co_return`.
The host C++ compiler supplies physical coroutine-frame storage for the WIR
values and places. This is not a second AST lowering and not a VM implementation.

```wio
use std::async;
async fn Produce(value: i32) -> i32 {
    await std::async::Sleep(10);
    return value + 1;
}
async fn Entry() -> i32 {
    let pending = Produce(41);
    // pending is a coroutine<i32>, not an uninitialized i32.
    let answer = await pending;
    await main;
    return answer == 42 ? 0 : 1;
}
```

Functions keep the runtime's eager-start behavior: they begin on the calling
thread and can suspend at await. An already-ready task need not suspend. The
awaited value is only read in the resume block; ordinary await never emits
`BlockOn`. The synchronous executable entry adapter waits for completion while
pumping the bound main executor, so `await main` does not deadlock async entry.
Worker, blocking, IO and main destinations have explicit scheduling adapters.
These execute existing WIR contracts; no new source executor syntax is added.

Cancellation checkpoints inspect the current promise before suspension and
after delivery, including ready-task fast paths. Child cancellation/fault
propagation uses the shared `AsyncTask<T>` runtime. Cancellation is cooperative,
not thread preemption. Queue exhaustion and runtime shutdown become task
failures. Existing copyable task result semantics remain unchanged: repeated
reads of an object result retain the same intrusive object identity.

An async object/interface method's `CoroutineLayout.retainedReceiver` identifies
the self parameter that must remain alive for the frame. Lowering pins this
contract, the verifier checks it (`LIR1533`), and C++ emission creates one owning
self guard with a stable raw receiver snapshot. Normal returns, cancellation
and exceptions unwind frame storage with RAII; explicit scope cleanup opcodes
remain authoritative. A caller dropping its last handle does not invalidate a
suspended method. Arbitrary component/ref borrows do not acquire new ownership.
As in the existing runtime, task completion notification can precede the final
physical destruction of the frame; it is not a scheduler-join barrier.

The new `wio_wir_cpp_async` gate compiles and executes generic/void/immediate
tasks, loop awaits, closure calls, worker/blocking/IO jobs, async interface
dispatch, owning results, self lifetime, parent-child cancellation, queued-main
cancellation, fault delivery and shutdown rejection. Canonical probes exercise
all four executor destinations and reject corrupt state, frame, executor and
receiver metadata. The standalone fixture supplies a minimal runtime surface;
full project/std/SDK adapter parity remains in 17.4 and the final cutover gate.
Runtime-native forwarding here is limited to compatible in-process
`wio::runtime` task/scalar/string/function signatures, not a foreign coroutine ABI.

## Cutover policy

`wir` becomes the default only when all release-gate programs pass both
generators, output behavior matches, native/SDK and async surfaces are complete,
and compile-time/runtime benchmarks are recorded. The legacy generator is
removed only after at least one release line with the WIR backend as default;
until then it remains the compatibility oracle for differential tests.
