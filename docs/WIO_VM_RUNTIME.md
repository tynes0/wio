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

## Implemented in Sprint 19.1-19.3

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
- stack-value components with explicit deep `CopyValue`, heap objects with
  intrusive strong references, and borrowed constructor/destructor receivers;
- recursive default field construction, field-initializer/constructor chaining,
  nested component field places, mutable array-element places, and direct,
  extension, method, virtual, and interface call execution;
- `Retain`, `Release`, and `ReleasePlace` object ownership operations with the
  Wio destructor scheduled exactly once before the final strong reference is
  cleared;
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

The next slices extend the aggregate core with dictionaries and the remaining
string/text/container intrinsic families. They then add function values,
closures and indirect dispatch, interface slot validation, coroutine
scheduling, panic stack traces, exceptional cleanup/unwind, and debugger
hooks. Native functions, callbacks, opaque values, and foreign-thread entry
are deliberately reserved for the Sprint 20 VM native bridge; Sprint 19
reports them as unsupported instead of silently changing behavior.
