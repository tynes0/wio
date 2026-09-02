# Wio Module, Export, and SDK Model

This document freezes the backend-neutral module boundary shared by Typed WIR,
Lowered WIR, the future C++ backend, bytecode modules, the VM loader, and host
SDKs. The existing `WioModuleApi` v11 remains compatible while the new backend
is developed; it is not silently replaced by this model.

## Module identity and dependencies

Every WIR module has a logical name, a stable key, a 64-bit FNV-1a identity,
and an explicit kind: executable program, Wio library, or native library. The
logical identity is independent of the absolute checkout directory. A manifest
backend may supply a package-qualified name; a single-file build uses its file
name.

Imports are data, not discarded parser trivia. Each dependency records whether
it is a Wio module, standard module, C++ header, or native library plus its
logical name, source path, alias, selected symbols, wildcard state, and stable
identity. This lets a native backend include a header while a VM registers a
native library without pretending those operations are equivalent.

## Exports and concrete generics

An export has a stable key/id, logical and backend symbol names, an export kind
and role, its WIR function or type target, and one deterministic SDK call-table
slot. Function exports retain concrete parameter/result types and async state.

A generic declaration never crosses the SDK boundary as an open C++ template.
Every accepted `[instantiate(...)]` set creates a separate
`generic-function-specialization` export. Generic parameter occurrences in its
SDK signature are substituted with concrete WIR types, and the specialization
arguments remain available as metadata.

Object and component exports are distinct descriptors. They do not collapse to
C++ layouts: reflection and generated adapter entries decide how the host may
construct, retain, inspect, mutate, or release them.

## Reflection and lifecycle

Every named type receives a stable reflection descriptor tied to the exact WIR
type and nominal kind. Exported status is explicit. Future attribute processors
may extend the descriptor, but neither backend may rediscover reflection by
parsing source attributes.

Module API-version, load, update, unload, save-state, and restore-state hooks are
stored as function IDs. Save and restore are one capability and must appear as a
pair. State schema version belongs to the module contract so hot reload can
reject incompatible snapshots before calling user code.

## SDK call table and compatibility

`sdk/include/wio_module_contract.h` defines a fixed-width C-shaped sidecar:

- `WioSdkModuleContract` identifies the module and lifecycle capabilities;
- `WioSdkExportDescriptor` maps stable export IDs to call-table slots;
- `WioSdkCallEntry` provides one uniform checked invoke thunk plus context;
- `WioSdkReflectionDescriptor` publishes stable runtime type identities;
- `WioValidateSdkModuleContract` rejects malformed slot and lifecycle layouts;
- `WioFindSdkExport` resolves by stable ID rather than compiler-specific C++
  mangling.

During migration, generated libraries may expose both `WioModuleApi` v11 and
the sidecar. Existing hosts continue to load v11. New hosts prefer the canonical
contract and can fall back to the legacy table. After the Lowered-WIR C++ backend
reaches parity, both descriptors will be emitted from the same `ModuleContract`,
ending the current generator's independent export indexing.

## Verification rules

Typed and Lowered WIR reject:

- missing, duplicate, or recomputed stable identities;
- call-table/export count or slot drift;
- exports targeting unknown functions/types;
- generic-specialization exports without concrete arguments;
- reflection descriptors that disagree with nominal WIR types;
- lifecycle hooks pointing outside the module;
- unpaired save/restore hooks.

Canonical lowering copies the entire contract exactly. A backend is therefore a
consumer of already-resolved module semantics, never a second module analyzer.
