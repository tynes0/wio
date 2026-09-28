# Wio Bytecode Format

Status: format v1 implementation in progress. This document defines the
compatibility rules that are already frozen; sections marked as pending are
not yet release-compatible.

## Purpose

Bytecode is compiled from verified canonical Lowered WIR. It is not a second
semantic pipeline. The native C++ backend and the bytecode compiler therefore
consume the same resolved calls, ownership operations, native ABI metadata,
coroutine states, module identities, and source locations.

The `.wiob` file is a portable module container. It never stores a host
pointer, a C++ object representation, `sizeof`-dependent padding, a
`std::variant` index, or an unpinned compiler enum ordinal.

## Header

All integers are little-endian. The fixed 48-byte header contains:

| Field | Size | Meaning |
| --- | ---: | --- |
| magic | 8 | `WIOBC` followed by `0D 0A 1A` |
| major/minor | 2 + 2 | format compatibility version |
| header size | 4 | currently 48 |
| endian marker | 4 | `0x01020304` |
| section count | 4 | bounded before allocation |
| directory offset | 8 | absolute file offset |
| file size | 8 | exact encoded size |
| payload checksum | 8 | FNV-1a-64 of bytes after the header |

Unknown major versions are rejected. A reader may accept a newer minor only
after the compatibility policy for that minor is implemented.

## Section directory

Each 32-byte directory entry contains a pinned section kind, flags, absolute
offset, byte size, logical record count, and a reserved field. Readers reject
duplicate required sections, overlap, integer overflow, out-of-file ranges,
unsupported flags, unreasonable counts, and trailing malformed records.

The implementation emits manifest, strings, constants, types, globals,
functions, code, and module-contract sections. The contract section preserves
imports/exports, SDK slots, reflection, behavioral attributes, application and
system scheduling, lifecycle hooks, and state-transfer identities. The debug
section has a reserved stable identifier and will be filled before format v1
is declared release-compatible.

## Instruction set

Bytecode opcodes are explicit 16-bit values in
`wio/bytecode/format.h`. Existing numeric values are never reordered or
reused. Lowered WIR opcodes are converted with an exhaustive mapping rather
than a cast. Type kinds, ownership, native ABI, async, attribute, application,
operator, and lowering-policy enums likewise pass through exhaustive pinned
encoders; no compiler enum ordinal crosses the file boundary. The current wide instruction encoding preserves typed SSA ids,
places, branch arguments, generic specialization identity, ownership and
borrow annotations, storage/escape classifications, bounds-check decisions,
intrinsic selectors, async operations, and source spans.

The wide v1 representation deliberately favors validation and tooling over
premature packing. A later compact execution representation may be produced
by the VM loader without changing the file ABI.

## Validation layers

1. The Lowered WIR verifier runs before bytecode compilation.
2. The binary loader validates the header, checksum, directory, limits, and
   every variable-length record before accepting the module.
3. The bytecode verifier checks indices, tables, function-local CFG targets,
   SSA definition/use identity, terminator placement, branch argument arity and
   types, body/external consistency, and known instructions without executing
   code.
4. The future VM verifier will add instruction-specific type and stack/frame
   invariants before a module becomes executable.

## Determinism

Compilation follows canonical Lowered WIR order. String ids are assigned on
first deterministic encounter; functions, blocks, instructions, and constants
retain source-module order. Encoding the same bytecode module twice must
produce byte-for-byte identical output.

## Pending before v1 freeze

- execution-specific type invariants beyond the current SSA/CFG verifier;
- debug/source-map compression beyond the source spans already carried by
  wide instructions;
- seeded malformed-input corpora for every record family (the bounded loader
  already has a libFuzzer entry point).
