# Application, System, Attribute, and Reflection WIR Contract

This document freezes the backend-neutral metadata consumed by the future C++
backend, bytecode compiler, VM, SDK generator, and tooling. Backends must not
re-read source AST nodes to rediscover application schedules, attribute
processors, or reflected layouts.

## Application and system model

Source `system` declarations remain stack-resident components. Source
`application` declarations remain one owning component plus extension-based
lifecycle functions and a generated entry point. Typed WIR now publishes that
desugaring as an explicit `ApplicationDescriptor` and ordered
`SystemDescriptor` records.

An application descriptor fixes:

- the application component type and stable identity;
- entry, `Start`, `Update`, `Close`, and `Exit` function identities;
- the system component types stored by the application;
- deterministic stage order, `after` dependencies, fixed frequency, and
  main/worker/inherited affinity;
- every stage run's target type, resolved extension function, delta contract,
  and resource borrows;
- read versus write access for each injected resource, derived from the
  resolved `view` versus `ref` parameter.

The contract is scheduling metadata, not an instruction to allocate systems on
the heap. `hostOwnsStorage` remains true and the existing component ownership
and cleanup rules apply. `nonBlockingScheduling` prevents a native host or VM
from interpreting an application update as an implicit wait for unrelated
async work.

## Attribute model

Every effective attribute application has a deterministic identity and keeps:

- its canonical semantic name;
- target kind and exact stable target identity;
- direct, inherited, scoped, composed, generated, or compiler origin;
- normalized argument text, names, default-use markers, and type arguments;
- compile-time versus runtime retention;
- ordered validation, derive, pre, post, finally, and around processor
  bindings, including hook name/mode and typed payload where present.

This is the result of semantic attribute expansion. A backend executes or emits
the already-selected processor contract; it does not repeat composition,
conflict, cardinality, target, or ordering resolution.

## Reflection model

Every named WIR type has one stable reflection descriptor. In addition to its
nominal kind and export state, the descriptor now contains ordered fields and
methods. Fields retain type, visibility, mutability, and applied attribute IDs.
Methods retain function identity, signature, dispatch slot, async state, and
applied attribute IDs. Enum/flagset cases keep stable identities and case
attributes. Parameter attributes preserve their function target and normalized
parameter index. Type attributes refer to the same module-level attribute records.

Reflection and scheduling share WIR `TypeId`/`FunctionId` identities. Therefore
SDK generation, a bytecode loader, and a debugger cannot disagree about which
method or field an attribute describes.

## Verification and lowering

Typed and Lowered WIR reject duplicate or missing stable IDs, invalid type and
function targets, unknown attribute references, unknown processor phases,
non-component application/system types, unresolved lifecycle functions,
non-canonical stage order, backward or missing dependencies, invalid fixed
frequencies, unresolved runs, and invalid resource bindings.

Canonical lowering copies the complete module contract exactly. This sprint
does not yet emit a new application runtime: it freezes the common input that
the new C++ backend and VM runtime will consume.
