# Wio VM Runtime

Status: Sprint 19 implementation in progress.

## Architecture

`wio_vm` is a separately linkable runtime over the versioned `.wiob` module
model. It does not re-run parsing, semantic analysis, or WIR lowering. A
machine borrows one immutable bytecode module, verifies it once during load,
and builds reusable per-function register counts and block lookup tables.

During Sprint 19 the target links the compiler-owned bytecode model and codec.
Before the VM package is installed as a public runtime, that format/loader core
will be extracted into a small shared library so embedding the VM never pulls
in the parser, analyzer, or native backend.

Execution uses typed SSA value registers and an explicit call-frame stack.
Branch arguments are copied to the target block parameters before control is
transferred. This preserves canonical Lowered WIR phi semantics without a
second language-specific control-flow model.

## Implemented in Sprint 19.1-19.6

- compact tagged values for null, bool, signed/unsigned integer, float, and
  UTF-8 string values;
- constants and scalar default values;
- unary, binary, numeric conversion, and range-containment operations;
- direct Wio-to-Wio calls, returns, jumps, conditional jumps, and block
  arguments;
- stable local and persistent module-global places with init/load/store,
  replace, borrow, move, copy, and value/place drop operations;
- strict UTF-8 decoding and UTF-32-backed runtime `text` values, keeping byte
  `string` and Unicode-semantic `text` distinct inside the VM;
- Unicode scalar `Count`, UTF-8 `ByteCount`, scalar-safe indexing/slicing,
  UTF-8 conversion, concatenation, comparison, and interpolated `text`;
- intrusive aggregate storage plus array construction, length, indexed reads,
  copy/return survival, and deterministic bounds diagnostics;
- complete array intrinsic execution for capacity/search/endpoints, safe
  fallback access, clone/slice/take/skip/concat/reverse/sort/join queries, and
  place-checked push/pop/insert/remove/extend/reserve/fill/clear mutations;
- complete byte-string intrinsic execution for byte indexing/search/slicing,
  trim/case/replace/repeat/split/lines/padding transformations, checked numeric
  and boolean parsing, and place-checked in-place mutations;
- complete Unicode `text` intrinsic execution for scalar slicing and search,
  UTF-8 conversion, code-point projection, grapheme segmentation/slicing,
  display width, case folding, and grapheme arrays;
- owned ordered and unordered dictionaries with strict lookup and mutable
  places, deterministic duplicate-key construction, deep copy, merge/extend,
  key/value projection, query/mutation intrinsics, and ordered endpoint and
  floor/ceiling operations;
- result-less bytecode calls and intrinsics, allowing verified `void` container
  mutations such as `Set`, `Extend`, and `Clear` without synthetic values;
- stack-value components with explicit deep `CopyValue`, heap objects with
  intrusive strong references, and borrowed constructor/destructor receivers;
- recursive default field construction, field-initializer/constructor chaining,
  nested component field places, mutable array-element places, and direct,
  extension, and non-virtual method calls;
- concrete-type virtual and interface dispatch through verified
  `(contract type, method slot) -> implementation` tables, including inherited
  overrides whose implementation slot differs from the interface slot;
- object/interface upcasts, checked casts, runtime type tests, and allocation
  identity equality over owned handles, borrowed handles, and places;
- object receivers can be borrowed inside compiler-produced method bodies,
  keeping real Wio source behavior aligned with hand-authored bytecode;
- bytecode-load validation for nominal cast tables, dispatch contracts, method
  slots, dynamic-call declarations, and object cast/test instruction shapes;
- `Retain`, `Release`, and `ReleasePlace` object ownership operations with the
  Wio destructor scheduled exactly once before the final strong reference is
  cleared;
- named function values, closure creation, and indirect calls with verified
  callable signatures;
- deterministic dense function/global identity remapping at the bytecode
  boundary, so optimizer-pruned WIR modules remain loadable and dispatch every
  metadata and instruction reference to the intended target;
- intrusive closure environments whose value captures are snapshots, reference
  captures preserve their borrow, and retained `self` captures keep an object
  alive after its creator frame or original handle is gone;
- intrusive async-task handles with explicit pending, running, ready,
  cancelled, and faulted states;
- native-compatible eager async start, synchronous `Machine::wait(...)`, and
  stable cancellation/fault observation through shared task handles;
- canonical coroutine `CancellationCheck`, `CoroutineSuspend`,
  `CoroutineResume`, and `CoroutineComplete` execution, including awaited
  result transfer into the resume block;
- executor-switch suspension as a cooperative continuation boundary. The
  executor identity is preserved by bytecode, while this slice resumes on the
  calling VM thread until worker queues are introduced;
- bytecode verification for async result types, dense coroutine frames and
  states, resume payloads, suspension operands, cancellation checks, and
  completion values;
- deterministic wrapping integer arithmetic without host signed-overflow UB;
- checked integer division and shifts;
- instruction, call-depth, and per-frame register budgets;
- explicit rejection of unsupported operations and external/native calls;
- stable `WVM` diagnostics carrying function, block, instruction, and source
  span context.

The VM consumes a module that must remain alive and immutable for the
machine's lifetime. `.wiob` decoding owns its module, so embedders normally
keep the decode result beside the machine.

## Remaining Sprint 19 surface

The next slices add real main/worker/blocking/I/O executor queues, externally
completed native tasks and timers, panic stack traces, exceptional cleanup and
unwind, and debugger hooks. Native functions, callbacks, opaque values, and
foreign-thread entry are deliberately reserved for the Sprint 20 VM native
bridge; Sprint 19 reports them as unsupported instead of silently changing
behavior.
