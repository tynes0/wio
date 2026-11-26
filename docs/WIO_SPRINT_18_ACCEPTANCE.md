# Sprint 18 Acceptance: Bytecode Format and Compiler

Sprint 18 is accepted as the non-executing bytecode compiler milestone. VM
execution intentionally belongs to Sprint 19.

## Accepted surface

- verified canonical Lowered WIR is the only bytecode compiler input;
- `.wiob` v1 uses a fixed header, section directory, little-endian integers,
  payload checksum, explicit size/count limits, and overlap rejection;
- opcode and every auxiliary semantic enum cross the file boundary through
  explicit pinned values rather than compiler ordinals;
- strings, constants, types, globals, functions, blocks, typed SSA
  instructions, source spans, native ABI, coroutine layout, reflection,
  attributes, application/system scheduling, lifecycle, exports, and SDK call
  tables round-trip deterministically;
- the loader rejects bad magic/version/endian/size/checksum, truncated and
  overlapping sections, duplicate sections, unreasonable counts, malformed
  lists, and invalid boolean encodings before accepting a module;
- the verifier rejects invalid tables, duplicate or undefined SSA values,
  malformed terminators, invalid branch targets/arguments/types, invalid
  callee/global references, and inconsistent external/code bodies;
- the disassembler exposes manifest, contract summaries, native/coroutine
  metadata, CFG, typed values, selectors, branch arguments, and source spans;
- `--emit-bytecode`, `--bytecode-output`, and `--disassemble-bytecode` work in
  the native compiler; `wio file bytecode`, `wio file disassemble`, and
  `wio project build --emit-bytecode` work through the self-hosted CLI;
- `.wiob` output uses staging plus atomic replacement;
- focused round-trip, determinism, corruption, truncation, overlap, malformed
  SSA/CFG, native/async/contract metadata, native CLI, self-hosted CLI, and
  project CLI tests pass;
- a bounded libFuzzer/ASan/UBSan loader entry point is available as
  `wio_bytecode_loader_fuzzer` when fuzzers are enabled.

## Deferred by design

- instruction execution, frames, heap/object handles, scheduling, panic/stack
  traces, and debugger hooks are Sprint 19 VM Runtime;
- native registry/thunks/callback return and foreign-thread VM entry are Sprint
  20 VM Native Bridge;
- compact execution decoding and source-map compression may optimize the
  loader but must preserve `.wiob` v1 semantics and compatibility.

## Focused gates

- `wio_bytecode_format`
- `wio_wir_emit_cli`
