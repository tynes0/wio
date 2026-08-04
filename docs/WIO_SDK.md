# Wio Host SDK

This document defines the current public C++ host and embedding surface for Wio.
It is the SDK contract for the current Wio v1 host integration layer.

For representative host-interop and stale-binding conformance tests, see
[`WIO_TRACEABILITY.md`](./WIO_TRACEABILITY.md).

For a practical interop-first walkthrough, also see:

- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- [`WIO_EXAMPLES.md`](./WIO_EXAMPLES.md)

Official public headers:

- `sdk/include/module_api.h`
- `sdk/include/wio_sdk.h`

Everything else should be treated as implementation detail unless it is
explicitly re-exported through those headers.

---

## 1. Scope

The current SDK covers:

- loading shared Wio modules through `wio::sdk::Module`
- reloading shared Wio modules through `wio::sdk::HotReloadModule`
- binding `@Export`, `@Command`, `@Event`, and event-hook entrypoints
- discovering exported `object` and `component` types
- constructing exported `object` and `component` instances from C++
- reading metadata for exported fields and methods
- typed and dynamic field access for all currently supported exported field kinds
- first-class enum/flagset field identity through `WioEnum` and `WioFlagset`

The current SDK does not expose compiler internals such as AST, parser, sema,
or codegen APIs.

`WioObject` and `WioComponent` are runtime reflection wrappers. They are not a
host-side registration DSL and they are not intended to expose compiler
internals.

---

## 1.1 Stability Reading

For the current `v1` freeze, the SDK should be read in three buckets:

- **Stable now**: raw ABI loading, ergonomic C++ wrappers, exports, commands,
  events, reload helpers, exported object/component reflection for the
  currently documented field kinds, and `WioEnum` / `WioFlagset`.
- **Stable with explicit caveats**: areas whose broad direction is frozen, but
  whose narrow ABI surface is still intentionally documented as incomplete.
- **Not yet part of the stable SDK boundary**: future-facing reflection layers
  or wrappers that are planned but not yet documented as part of `v1`.

That means “available in the SDK codebase” and “part of the public `v1`
contract” should not be treated as the same thing automatically.

---

## 2. Include Model

Most host applications only need:

```cpp
#include <wio_sdk.h>
```

Include `module_api.h` only if you want to work with the raw ABI directly, such
as:

- consuming a statically linked module that exposes `WioModuleGetApi()`
- inspecting the raw `WioModuleApi`
- using the ABI structs without the higher-level SDK wrappers

If you are using a packaged Wio toolchain, the public include directory is:

- `<WIO_ROOT>/sdk/include`

For project integration details, see [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md).

---

## 3. Primitive Host Type Aliases

`wio_sdk.h` exposes the Wio primitive names as C++ aliases so hosts do not have
to guess how Wio types map to C++:

```cpp
using wio::i8;
using wio::i16;
using wio::i32;
using wio::i64;
using wio::u8;
using wio::u16;
using wio::u32;
using wio::u64;
using wio::f32;
using wio::f64;
using wio::isize;
using wio::usize;
using wio::byte;
using wio::string;
using wio::opaque;
```

Important mappings:

- `wio::isize` -> `std::intptr_t`
- `wio::usize` -> `std::uintptr_t`
- `wio::string` -> `std::string`
- `wio::opaque` -> `void*`

This means host code can stay visually aligned with Wio source code instead of
mixing Wio type names with unrelated C++ spellings.

---

## 4. Loading Modules

### 4.1 Shared Modules

Use `wio::sdk::Module` when you want to load a Wio shared library:

```cpp
#include <cstdint>
#include <wio_sdk.h>

int main()
{
    auto module = wio::sdk::Module::load("gameplay.dll");

    auto getCounter = module.load_command<std::int32_t()>("counter.get");
    auto addCounter = module.load_function<std::int32_t(std::int32_t)>("counter.add");
    auto onTick = module.load_event<void(float)>("game.tick");

    const std::int32_t before = getCounter();
    const std::int32_t afterAdd = addCounter(3);
    onTick(5.0f);

    return before == 0 && afterAdd == 3 ? 0 : 1;
}
```

`Module::load(...)`:

- opens the dynamic library
- resolves `WioModuleGetApi`
- validates the SDK descriptor version and the full host-visible ABI descriptor contract
- invokes `@ModuleLoad` automatically when present

Use `Module::open(...)` if you need the raw module without calling the load
lifecycle hook yet.

### 4.2 Statically Linked Modules

For a statically linked Wio library, use the raw API pointer exposed by the
module:

```cpp
#include <cstdint>
#include <wio_sdk.h>

extern "C" const WioModuleApi* WioModuleGetApi();

int main()
{
    const WioModuleApi* api = WioModuleGetApi();
    wio_validate_module_api(api);
    auto addNumbers = wio_load_export<std::int32_t(std::int32_t, std::int32_t)>(api, "AddNumbers");
    auto weighted = wio_load_function<std::int32_t(std::int32_t, std::int32_t)>(api, "math.weighted");
    return addNumbers(10, 20) == 30 && weighted(3, 4) == 10 ? 0 : 1;
}
```

`wio_validate_module_api(...)` is available both as:

- `wio::sdk::validate_module_api(...)`
- `wio_validate_module_api(...)`

Use it when you receive a raw `WioModuleApi*` from a statically linked module or
from a custom host pipeline and want an early ABI sanity check before binding any
exports, commands, events, objects, or components.

The free helper overloads also accept `wio::sdk::Module` and
`wio::sdk::HotReloadModule`, so the call sites stay uniform:

- `wio_load_export<Signature>(...)`
- `wio_load_command<Signature>(...)`
- `wio_load_event_hook<Signature>(...)`
- `wio_load_event<Signature>(...)`
- `wio_load_function<Signature>(...)`
- `wio_load_object(...)`
- `wio_load_component(...)`

### 4.3 Choosing `Module` vs `HotReloadModule`

The rule of thumb is:

- use `Module` when you want one loaded generation and explicit lifecycle calls,
- use `HotReloadModule` when the host wants to keep a stable C++ handle while
  the underlying DLL generation may change.

In practice:

- editor tools, gameplay scripting hosts, and live-reload workflows usually
  want `HotReloadModule`
- packaging tests, static host integration, CI, and deterministic automation
  usually want plain `Module`

The important distinction is not just "reload support":

- `Module` gives you direct generation-bound wrappers
- `HotReloadModule` keeps top-level callable reloadable wrappers stable while
  object/component/field wrappers remain generation-bound on purpose

---

## 5. Top-Level Entry Points

The SDK currently supports four host-facing top-level entrypoint families:

- `@Export`
- `@Command("name")`
- `@Event("name")`
- event hooks discovered through the module API

Typical usage:

```cpp
auto exported = module.load_export<std::int32_t(std::int32_t)>("Double");
auto command = module.load_command<std::int32_t(std::int32_t)>("counter.add");
auto event = module.load_event<void(float)>("game.tick");
auto hook = module.load_event_hook<void(float)>("game.tick::ui");
```

Use commands for named host-driven invocations and events for fan-out
broadcasting across multiple listeners.

---

## 6. Exported Objects and Components

The SDK can discover exported `object` and `component` declarations through the
module metadata.

Example:

```cpp
auto profileType = module.load_object("Profile");
auto statsType = module.load_component("Stats");

auto profile = profileType.create();
auto stats = statsType.create();

profile.set("level", 9);
stats.set("hp", 120);

auto level = profile.get<std::int32_t>("level");
auto hp = stats.get<std::int32_t>("hp");
```

Type wrappers:

- `wio::sdk::WioObjectType`
- `wio::sdk::WioComponentType`

Instance wrappers:

- `wio::sdk::WioObject`
- `wio::sdk::WioComponent`

These wrappers support:

- constructor binding through `create(...)`
- bound-method loading through `method<Signature>(...)`
- field metadata through `list_fields()` and `field_info(...)`
- typed field access through `get<T>(...)`, `set(...)`, and the collection helpers
- field accessor objects through `field(...)`
- explicit enum/flagset helpers through `get_enum(...)`, `set_enum(...)`,
  `get_flagset(...)`, and `set_flagset(...)`

Method binding example:

```cpp
auto enemyType = module.load_object("Enemy");
auto enemy = enemyType.create();

enemy.set("hp", 40);

auto addDamage = enemy.method<void(std::int32_t)>("ApplyDamage");
auto currentHp = enemy.method<std::int32_t()>("GetHp");

addDamage(7);
const std::int32_t hp = currentHp();
```

This is intentionally different from loading top-level exports:

- top-level `load_export(...)`, `load_command(...)`, and `load_event(...)`
  bind module-level entrypoints
- `object.method<...>(...)` and `component.method<...>(...)` bind instance
  methods and keep the instance handle captured in the callable

### 6.1 Exported Type ABI Validation

Before the SDK binds an exported `object` or `component`, it now validates the
host-visible ABI shape of the type metadata. In v1 that validation includes:

- `createExport` must be a zero-argument bridge returning `usize`
- `destroyExport` must be a one-argument bridge taking `usize` and returning `void`
- constructor entries must return `usize`
- if `createExport` exists, one constructor entry must reuse that same zero-argument export
- readable primitive fields must expose `getter(handle) -> value`
- writable primitive fields must expose `setter(handle, value) -> void`
- readable `object` and `component` fields must expose `getter(handle) -> usize`
- writable `object` and `component` fields must expose `setter(handle, usize) -> void`
- readable and writable dynamic/container/function fields must expose raw bridges plus `dynamicGetter` / `dynamicSetter`
- methods must reserve parameter `0` for the instance handle as `usize`
- methods must return a concrete ABI type or `void`

This means malformed exported-type descriptors fail fast as SDK diagnostics
instead of surfacing later as undefined host behavior.

---

## 7. Field Export Contract In v1

The current documented field-export contract is:

- only `public` fields participate in the public host field surface
- `private` and `protected` fields are not part of the host field contract
- `@ReadOnly` fields expose a getter but not a setter
- `object` fields are exposed as `WioObject`
- `component` fields are exposed as `WioComponent`

The supported exported field families in the current SDK are:

- primitive scalars
- enum
- flagset
- `string`
- `object`
- `component`
- dynamic arrays
- static arrays
- dictionaries
- trees
- function fields

The current v1 SDK treats `ref` and `view` field export semantics as outside the
stable documented host field ABI. They should not be relied on as part of the
public SDK contract yet.

Practical access matrix:

- primitive scalar field -> `get<T>()`, `set(...)`, `get_scalar_value()`,
  `set_scalar_value(...)`
- enum field -> `get_enum()`, `set_enum(...)`, `get_dynamic()`
- flagset field -> `get_flagset()`, `set_flagset(...)`, `get_dynamic()`
- `string` field -> `get<string>()`, `set(...)`, `get_dynamic()`
- `object` / `component` field -> typed object/component wrappers or dynamic
  wrappers
- container field -> typed container helpers or dynamic wrappers
- function field -> `get_function<Signature>()`, `set_function(...)`, or
  `get_dynamic()`

### 7.1 Field Metadata

Use `list_fields()` or `field_info(...)` to inspect exported fields:

```cpp
auto stateType = module.load_object("ComplexState");

for (const auto& field : stateType.list_fields())
{
    if (field.can_read())
    {
        // field.name
        // field.access
        // field.type
    }
}
```

`FieldInfo` carries:

- field name
- access modifier
- read/write flags
- read-only flag
- logical type name
- a `TypeDescriptorView` describing the field type

`TypeDescriptorView` lets the host inspect:

- whether a type is explicitly nullable through `is_nullable()`; for a
  nullable descriptor, `element_type()` returns the non-null value type

- primitive ABI type
- logical type name
- element type for arrays
- key/value types for dictionaries and trees
- return type and parameter types for function fields
- static extent for fixed-size arrays
- enum/flagset member descriptors through the embedded enum-member helpers

For enum and flagset fields, `TypeDescriptorView` also exposes enough metadata
for host tooling to stay symbolic instead of flattening everything into raw
integers:

- `enum_member_count()`
- `enum_member_name(index)`
- `enum_member_scalar_value(index)`
- `enum_index_of(value)`

### 7.2 Ownership Rules

The ownership model in the current SDK is intentionally simple:

- values created through `WioObjectType::create(...)` or `WioComponentType::create(...)` own their handles
- nested `object` and `component` fields returned from another instance are borrowed wrappers
- `WioObject::owns_handle()` and `WioComponent::owns_handle()` tell you whether the wrapper owns destruction
- `is_borrowed()` reports the inverse of owned wrapper handles
- `WioObjectType`, `WioComponentType`, `WioObject`, `WioComponent`, `WioFieldAccessor`, and bound methods are valid only for the module generation they came from
- after `Module::unload()` or `Module::close()`, old wrappers stay stale even if the same `Module` is started again; reacquire fresh wrappers from the new generation

This keeps normal host usage predictable while avoiding implicit deep copies of
nested exported state.

---

## 8. Typed Field Access

Use typed access whenever you already know the field type.

Examples:

```cpp
auto state = module.load_object("ComplexState").create();

state.set("title", wio::string("arena"));
state.set_array("tags", wio::sdk::WioArray<wio::string>{ "red", "blue" });
state.set_dict("scores", wio::sdk::WioDict<wio::string, std::int32_t>{ {"hp", 10}, {"mp", 4} });
state.set_tree("order", wio::sdk::WioTree<wio::string, std::int32_t>{ {"bronze", 1}, {"silver", 2} });
state.set_static_array("fixed", wio::sdk::WioStaticArray<std::int32_t, 3>{ 1, 2, 3 });
state.set_function<std::int32_t(std::int32_t)>("callback", [](std::int32_t value)
{
    return value + 5;
});

auto title = state.get<wio::string>("title");
auto tags = state.get_array<wio::string>("tags");
auto scores = state.get_dict<wio::string, std::int32_t>("scores");
auto order = state.get_tree<wio::string, std::int32_t>("order");
auto fixed = state.get_static_array<std::int32_t, 3>("fixed");
auto callback = state.get_function<std::int32_t(std::int32_t)>("callback");
```

Enum/flagset example:

```cpp
auto enemy = module.load_object("Enemy").create();

auto state = enemy.get_enum("state");
auto flags = enemy.get_flagset("features");

const std::string_view stateName = state.name();
const std::ptrdiff_t stateIndex = state.index();
const std::uint32_t stateMemberCount = state.member_count();

enemy.set_enum("state", state);
enemy.set_flagset("features", flags);
```

Use typed access when:

- the field schema is known ahead of time
- you want the strongest compile-time host typing
- you do not need to branch on field type at runtime

---

## 9. Dynamic Field Access

Use `WioFieldAccessor::get_dynamic()` and `set_dynamic(...)` when the host needs
runtime reflection rather than compile-time knowledge.

`WioDynamicValue` can currently hold:

- primitive scalar values
- `WioEnum`
- `WioFlagset`
- `string`
- `WioObject`
- `WioComponent`
- `WioDynamicArray`
- `WioDynamicStaticArray`
- `WioDynamicDict`
- `WioDynamicTree`
- `WioDynamicFunction`

Example:

```cpp
auto accessor = state.field("scores");
auto value = accessor.get_dynamic();

if (value.is_dict())
{
    auto scores = value.as_dynamic_dict().as_dict<wio::string, std::int32_t>();
    auto hp = scores.at("hp");
    (void)hp;
}

state.field("callback").set_dynamic(
    wio::sdk::WioDynamicValue(std::function<std::int32_t(std::int32_t)>(
        [](std::int32_t value)
        {
            return value * 3;
        })));
```

Enum/flagset dynamic example:

```cpp
auto stateAccessor = enemy.field("state");
auto stateValue = stateAccessor.get_dynamic();

if (stateValue.is_enum())
{
    auto stateEnum = stateValue.as_enum();
    auto stateName = stateEnum.name();
    auto stateOrdinal = stateEnum.index();
    (void)stateName;
    (void)stateOrdinal;
}

auto featureAccessor = enemy.field("features");
auto featureValue = featureAccessor.get_dynamic();

if (featureValue.is_flagset())
{
    auto flags = featureValue.as_flagset();
    auto type = flags.type();
    const auto bitCount = type.enum_member_count();
    (void)bitCount;
}
```

Use dynamic access when:

- a tool is inspecting arbitrary exported fields
- a host editor is building generic property grids
- runtime code wants to branch based on type metadata
- you need one reflection path that handles primitives and collections alike

If the host asks for the wrong type, the SDK throws `wio::sdk::Error` with
`ErrorCode::SignatureMismatch` instead of leaving the mismatch to raw backend C++
behavior.

The key design boundary here is:

- `WioDynamicValue` preserves enum/flagset identity
- it does not flatten them into anonymous integers unless the host explicitly
  converts them
- the host can still fall back to the underlying scalar representation through
  `scalar_value()` / `as<T>()` on `WioEnum` and `WioFlagset`

---

## 10. Hot Reload

Use `wio::sdk::HotReloadModule` for DLL-based scripting workflows:

```cpp
auto module = wio::sdk::HotReloadModule::load("gameplay.dll");
module.enable_auto_reload();

auto update = module.load_event<void(float)>("game.tick");
update(1.0f / 60.0f);

module.reload_if_changed();
```

Current hot-reload behavior:

- stages a private copy of the source DLL before loading it
- can preserve module state when `@ModuleSaveState` and `@ModuleRestoreState` are available
- can reload manually or lazily through `reload_if_changed()`
- top-level `load_export`, `load_command`, `load_event_hook`, and `load_event` bindings loaded from `HotReloadModule` reacquire the current generation automatically
- exported `object`, `component`, field-accessor, and bound-method wrappers are generation-bound; after `reload()`, `reload_from(...)`, `unload()`, or `close()`, reacquire them from the current module generation
- stale wrappers throw `ErrorCode::StaleBinding` instead of calling through unloaded code
- owned stale wrappers skip their destroy bridge during reset/destruction so the SDK does not invoke unloaded module code while cleaning up old handles

Current lifetime and handoff rule:

- Wio owns the exported runtime objects behind module handles
- the host owns SDK wrapper objects and cached callables
- when code changes across reload, the host must decide how to reacquire and reinterpret state from the new generation
- the SDK guarantees fail-fast stale-binding diagnostics and optional module save/restore handoff, but it does not try to make old object wrappers semantically valid against new code automatically

Practical reload rule:

- reload-safe: top-level exports/commands/events loaded from `HotReloadModule`
- reload-unsafe by design: `WioObjectType`, `WioComponentType`, `WioObject`,
  `WioComponent`, `WioFieldAccessor`, and bound instance methods

If you need long-lived host objects across reload, keep host-side keys or
identifiers and reacquire fresh wrappers after reload instead of caching old
instance wrappers indefinitely.

---

## 11. Error Model

The SDK throws `wio::sdk::Error` on failures.

Important error categories include:

- `InvalidArgument`
- `ApiUnavailable`
- `InvalidApiDescriptor`
- `LibraryOpenFailed`
- `SymbolLookupFailed`
- `ExportNotFound`
- `CommandNotFound`
- `EventNotFound`
- `EventHookNotFound`
- `TypeNotFound`
- `FieldNotFound`
- `MethodNotFound`
- `FieldNotWritable`
- `SignatureMismatch`
- `InvokeFailed`
- `LifecycleFailed`
- `StaleBinding`
- `ReloadFailed`
- `IoFailure`

The goal of the SDK layer is that host mistakes are surfaced as Wio SDK errors
rather than as obscure backend-only failures.

---

## 12. Official v1 Boundary

For the current SDK version, the public and documented boundary is:

- `module_api.h` for the raw ABI
- `wio_sdk.h` for the ergonomic C++ layer
- shared-module loading, static-module consumption, reload helpers, exports,
  commands, events, exported object/component reflection, and dynamic field access
- `TypeDescriptorView` as the host-readable type-metadata view
- `FieldInfo` as the stable per-field metadata carrier
- `WioEnum` and `WioFlagset` as the first-class host wrappers for enum identity
- `WioDynamicValue` and the current dynamic container wrappers as the runtime
  reflection payload family
- generation-aware stale-wrapper diagnostics for reload-sensitive wrappers

The following should be treated as stable with explicit caveats:

- `ref` / `view` exported field semantics are intentionally documented as
  outside the stable host ABI for now
- enum/flagset wrappers are stable at the `WioEnum` / `WioFlagset` level, but
  deeper host-side reflection growth beyond that should still be treated as
  future-facing

The following should not currently be read as part of the stable v1 SDK
contract:

- compiler-internal AST/parser/sema/codegen APIs
- host-side registration DSLs for declaring Wio types from C++
- automatic semantic reconciliation of stale object/component wrappers across
  reload generations
- `ref` / `view` field export behavior as a public host ABI promise

Future SDK work can still grow from here, but this is the intended "complete
enough to build against" baseline for the current Wio SDK generation.

---

## 13. See Also

- [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- [`examples/static_cmake_consumer/README.md`](../examples/static_cmake_consumer/README.md)
- [`tests/native/sdk_exported_complex_fields_host.cpp`](../tests/native/sdk_exported_complex_fields_host.cpp)
