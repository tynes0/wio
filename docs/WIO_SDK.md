# Wio Host SDK

This document defines the current public C++ host and embedding surface for Wio.
It is the SDK contract shipped with Wio v0.15 and the baseline of the planned
Wio v1 host integration layer. The pre-v1 parity and synchronized-version plan
is tracked in [`WIO_SDK_EVOLUTION_PLAN.md`](./WIO_SDK_EVOLUTION_PLAN.md).

For representative host-interop and stale-binding conformance tests, see
[`WIO_TRACEABILITY.md`](./WIO_TRACEABILITY.md).

For a practical interop-first walkthrough, also see:

- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- [`WIO_EXAMPLES.md`](./WIO_EXAMPLES.md)

Official public headers:

- `sdk/include/wio_version.h`
- `sdk/include/module_api.h`
- `sdk/include/wio_features.h`
- `sdk/include/wio_values.h`
- `sdk/include/wio_sdk.h`

Everything else should be treated as implementation detail unless it is
explicitly re-exported through those headers.

The SDK product version is available without loading a module:

```cpp
static_assert(WIO_SDK_VERSION_MAJOR == 0);
static_assert(WIO_SDK_VERSION_MINOR == 15);
static_assert(wio::sdk::product_version.patch == 0);

std::cout << wio::sdk::product_version_string; // 0.15.0
```

`WIO_MODULE_API_DESCRIPTOR_VERSION` remains an independent low-level ABI
revision. Product releases advance even when the ABI descriptor does not; hosts
must check both values for their respective purposes.

---

## 1. Scope

The current SDK covers:

- inspecting module product version, ABI layout size, capabilities, and feature support
- loading shared Wio modules through `wio::sdk::Module`
- reloading shared Wio modules through `wio::sdk::HotReloadModule`
- binding `@Export`, `@Command`, `@Event`, and event-hook entrypoints
- discovering exported `object` and `component` types
- constructing exported `object` and `component` instances from C++
- reading metadata for exported fields and methods
- typed and dynamic field access for all currently supported exported field kinds
- first-class enum/flagset field identity through `WioEnum` and `WioFlagset`
- validated Unicode `text` field exchange through `WioText`
- host-semantic values for Option, Result/UnitResult, tuple, queue, sets, span,
  buffers, pools, Box, and any
- stable type IDs and concrete generic-argument metadata for generated modules

The current SDK does not expose compiler internals such as AST, parser, sema,
or codegen APIs.

`WioObject` and `WioComponent` are runtime reflection wrappers. They are not a
host-side registration DSL and they are not intended to expose compiler
internals.

---

## 1.1 Stability Reading

For the current pre-`v1` freeze, the SDK should be read in three buckets:

- **Stable now**: raw ABI loading, ergonomic C++ wrappers, exports, commands,
  events, reload helpers, exported object/component reflection for the
  currently documented field kinds, and `WioEnum` / `WioFlagset`.
- **Stable with explicit caveats**: areas whose broad direction is frozen, but
  whose narrow ABI surface is still intentionally documented as incomplete.
- **Not yet part of the stable SDK boundary**: future-facing reflection layers
  or wrappers that are planned but not yet documented as part of `v1`.

That means “available in the SDK codebase” and “part of the current public
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
using wio::text;
using wio::opaque;
```

Important mappings:

- `wio::isize` -> `std::intptr_t`
- `wio::usize` -> `std::uintptr_t`
- `wio::string` -> `std::string`
- `wio::text` -> `wio::sdk::WioText` (owned, validated UTF-8 with code-point indexing)
- `wio::opaque` -> `void*`

This means host code can stay visually aligned with Wio source code instead of
mixing Wio type names with unrelated C++ spellings.

### 3.1 Explicit ABI integer markers

C++ typedef identity cannot always preserve Wio's semantic distinction. On a
64-bit platform `std::uint64_t` and `std::uintptr_t` may be the same type; on
common platforms `std::uint8_t` is an alias of `unsigned char`. A function
template therefore cannot infer whether the host intended `u64` or `usize`, or
`u8` or `uchar`, from those aliases alone.

Use explicit SDK marker values whenever the distinction is observable at the
module boundary:

```cpp
using namespace wio::sdk;

auto readFingerprint = module.load_command<WioU64()>("telemetry.fingerprint");
auto resize = module.load_command<void(WioUSize)>("buffer.resize");
auto decodeByte = module.load_command<WioU8(WioUChar)>("decode.byte");

const std::uint64_t fingerprint = readFingerprint().value();
resize(WioUSize{4096u});
```

The complete explicit integer set is:

- `WioUChar`, `WioByte`
- `WioI8`, `WioI16`, `WioI32`, `WioI64`
- `WioU8`, `WioU16`, `WioU32`, `WioU64`
- `WioISize`, `WioUSize`

The wrappers contain only the requested C++ storage value, expose `.value()`,
and bind to the ABI kind encoded in their name. Existing natural C++ scalar
bindings remain source-compatible; markers are the deterministic choice for
platform-dependent alias collisions.

### 3.2 Current value surface

`wio_values.h` mirrors stable Wio value semantics without pretending that C++
templates are a binary ABI:

```cpp
using namespace wio::sdk;

auto label = WioText::from_utf8("İstanbul 🚀");
auto selected = WioOption<wio::i32>::some(13);
auto loaded = WioResult<wio::i32>::ok(42);
WioTuple<wio::i32, std::string> pair{13, "wio"};

WioQueue<std::string> queue{"first", "second"};
WioUnorderedSet<wio::i32> ids{1, 2, 2};
WioOrderedSet<wio::i32> sorted{9, 3, 7};

std::array<wio::i32, 3> storage{1, 2, 3};
WioSpan<wio::i32> view(storage);

WioByteBuffer bytes(64);
WioPool<std::string> pool;
auto handle = pool.rent("owned by the pool");
```

These are host-side semantic counterparts. `WioOption<T>` and
`WioResult<T>`, for example, do not establish a raw C++ template ABI with a
Wio object instance. Generated descriptors publish the concrete identity and
generic arguments; a direct value bridge is added only when the feature
catalog advertises one.

Use `wio::sdk::features()`, `feature_info(...)`, or `find_feature(...)` to
inspect that distinction programmatically. The authoritative human-readable
matrix is [`WIO_SDK_0_14_PARITY_MATRIX.md`](./WIO_SDK_0_14_PARITY_MATRIX.md).

From v0.14 onward every catalog row also has an explicit `FeatureSupport`
state: `Supported`, `Partial`, or `Deferred`. `FeatureSurface` continues to say
*where* a feature works; the support state says whether the advertised surface
is complete or intentionally bounded. In particular, the newly bridged stable
values advertise `DynamicField`, while pool identity, generic instantiation,
interfaces, `Box`, `Any`, and async tasks remain visibly partial.

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

Generated 0.14 modules also publish their product version and descriptor size.
Hosts can take an owned inspection snapshot:

```cpp
auto info = module.inspect();
if (!info.product_version || info.product_version->minor != 13)
    throw std::runtime_error("unexpected Wio module version");

if (!info.has_capability(WIO_MODULE_CAP_TYPE_METADATA_V2))
    throw std::runtime_error("generic metadata is unavailable");
```

`ModuleInfo` owns its export, command, event, and type names, so those lists
remain valid after the module is unloaded. Descriptor views and instance
wrappers remain generation-bound.

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
- `text`
- `object`
- `component`
- dynamic arrays
- static arrays
- dictionaries
- trees
- function fields

The current SDK treats `ref` and `view` field export semantics as outside the
stable documented host field ABI. They should not be relied on as part of the
public SDK contract yet.

Practical access matrix:

- primitive scalar field -> `get<T>()`, `set(...)`, `get_scalar_value()`,
  `set_scalar_value(...)`
- enum field -> `get_enum()`, `set_enum(...)`, `get_dynamic()`
- flagset field -> `get_flagset()`, `set_flagset(...)`, `get_dynamic()`
- `string` field -> `get<string>()`, `set(...)`, `get_dynamic()`
- `text` field -> `field(...).get_text()`, `set_text(...)`, `get_dynamic()`
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
- whether a type is `text`, Option, Result, tuple, queue, ordered/unordered
  set, span, byte buffer, Box, any, interface, async task, or another concrete
  generic instance
- its deterministic FNV-1a `stable_id()`
- `generic_argument_count()`, `generic_argument(index)`, and
  `generic_arguments()` for concrete generic instantiations

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
- validated Unicode `WioText`
- `WioObject`
- `WioComponent`
- `WioDynamicArray`
- `WioDynamicStaticArray`
- `WioDynamicDict`
- `WioDynamicTree`
- `WioDynamicFunction`
- `WioDynamicTypedValue` for the v0.14 Option/Result/unit, tuple, queue/set,
  span, and byte-buffer bridge families

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

Portable v0.14 values retain their exact host representation behind a checked
typed wrapper:

```cpp
using Count = wio::sdk::WioOption<std::int32_t>;

auto value = state.field("selected").get_dynamic();
if (value.is_typed() && value.as_typed().can_access_as<Count>())
{
    auto count = value.as_typed().get_as<Count>();
    (void)count;
}

state.field("selected").set_dynamic(
    wio::sdk::WioDynamicValue::typed(Count::some(14)));
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

### Typed `Option` fields in v0.14

The v0.14 value bridge begins with `std::Option<T>`. A C++ host uses
`WioOption<T>`; the generated module converts the representation instead of
exposing Wio's internal reference-counted `Option` object:

```cpp
auto state = module.load_object("Sdk14Options").create();
auto selected = state.field("selected");

auto value = selected.get_as<WioOption<std::int32_t>>();
selected.set_as(WioOption<std::int32_t>::some(42));
selected.set_as(WioOption<std::int32_t>::none());
```

Nested options preserve every level:

```cpp
using Nested = WioOption<WioOption<std::int32_t>>;
auto value = state.field("nested").get_as<Nested>();
```

The initial bridge supports primitive, `string`, `text`, and recursively nested
`Option` payloads. Unsupported payloads are rejected while compiling the Wio
module, so they cannot degrade into a host-side `not callable` surprise.

`std::Result<T>` uses the existing `WioResult<T>` host value in the same way:

```cpp
using HostResult = WioResult<std::int32_t>;
auto calculation = state.field("calculation");

calculation.set_as(HostResult::ok(84));
calculation.set_as(HostResult::error({
    WioResultDomain::Custom,
    701,
    -9001,
    "host calculation failed"
}));
```

The bridge preserves the success value and the complete Wio error record:
domain, portable code, native code, and message. `std::UnitResult` maps to
`WioUnitResult`, with `WioUnit` represented by the dedicated `UNIT` type
descriptor rather than pretending it is an exported component handle.

Arrays, fixed arrays, `Dict`, and `Tree` apply the same conversion recursively.
The host uses its normal SDK/C++ container surface while every nested Wio value
keeps its semantics:

```cpp
using Choice = WioOption<std::int32_t>;
using Outcome = WioResult<std::int32_t>;

auto choices = state.field("choices").get_as<std::vector<Choice>>();
auto fixed = state.field("fixed").get_as<std::array<Outcome, 2>>();
auto lookup = state.field("lookup").get_as<
    std::unordered_map<std::string, WioOption<WioU64>>
>();
```

Conversion is recursive in both directions. A nested category without a stable
bridge is rejected in semantic analysis instead of exporting a raw generated
C++ template type that the host cannot name safely.

Queue and set fields use their semantic SDK mirrors rather than exposing their
private Wio storage:

```cpp
using Work = WioQueue<WioOption<std::int32_t>>;
using Tags = WioUnorderedSet<std::string>;
using Levels = WioOrderedSet<std::int32_t>;

auto work = state.field("work").get_as<Work>();
state.field("tags").set_as(Tags{"host", "sdk"});
```

Queue order and ordered-set ordering are preserved. Unordered sets preserve
membership without promising iteration order.

`std::Tuple<...>` fields also cross the generated bridge as
`wio::sdk::WioTuple<...>` (an SDK spelling for `std::tuple`). Every tuple slot
is checked against the published generic argument descriptor and converted
recursively, so mixed values remain strongly typed:

```cpp
using Snapshot = WioTuple<
    WioOption<std::int32_t>,
    WioResult<std::string>,
    WioU64
>;

Snapshot snapshot = object.field("snapshot").get_as<Snapshot>();
object.field("snapshot").set_as(Snapshot{
    WioOption<std::int32_t>::none(),
    WioResult<std::string>::ok("updated"),
    WioU64{14}
});
```

Tuple arity and every element type participate in `can_access_as<T>()`.
Unsupported nested values are rejected by Wio analysis instead of reaching a
C++ template or erased-payload failure.

Wio's `std::Span` is deliberately a checked `(start, count)` token; it does not
own or retain the array it was created from. Exported span fields therefore use
`WioSpanRange`, while `WioSpan<T>` remains the host's borrowed memory view:

```cpp
auto values = object.field("values").get_as<std::vector<std::int32_t>>();
auto range = object.field("window").get_as<WioSpanRange>();
WioSpan<const std::int32_t> window(values.data(), values.size(), range);
```

The host must keep `values` alive while `window` is used. Applying a range to a
host span clamps it to the source bounds, matching `std::span::Make` and
`std::span::Slice`. The SDK does not disguise an exported range token as a
borrow into memory owned by another module.

`std::ByteBuffer` uses the owned `WioByteBuffer` bridge. Reads and writes copy
the byte content across the module boundary and retain both reserved capacity
and cursor position:

```cpp
auto payload = object.field("payload").get_as<WioByteBuffer>();
payload.write_u32_le(0x12345678u);
payload.rewind();
object.field("payload").set_as(std::move(payload));
```

No Wio object pointer or private buffer layout crosses the ABI. Pools remain a
separate ownership facility: their generation handles are meaningful only to
the pool instance that issued them and are not flattened into an owned buffer.

### Concrete const-generic metadata in v0.14

ABI descriptor version `8` adds a dedicated `CONST_VALUE` descriptor. Generic
arguments now distinguish a type argument from a compile-time value and expose
both the value's declared type and its canonical payload:

```cpp
auto blockType = object.field("block").type(); // SizedValue<i32, 4>
auto extent = blockType.generic_argument(1);

if (extent.is_const_value() &&
    extent.const_value_type().abi_type() == WIO_ABI_USIZE) {
    std::cout << extent.const_value(); // "4"
}
```

Integer, `string`, and `text` const arguments retain distinct value-type
descriptors. Empty strings are represented by a non-null, zero-length payload,
so they cannot be confused with missing metadata. Stable IDs continue to hash
an unambiguous canonical displayed identity, including both the declared
const-value type and its concrete value.

User-defined generic object/component fields require an explicitly exported
concrete specialization. An unspecialized instance is rejected during Wio
analysis because the host would otherwise receive metadata for a type absent
from the module's concrete type table. This keeps the failure at the source
declaration instead of deferring it to module loading.

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

## 12. Official v0.15 Boundary

Module ABI descriptor v9 preserves the v0.14 value bridge and adds retained
typed-attribute metadata. A generated module advertises
`WIO_MODULE_CAP_ATTRIBUTE_METADATA_V1` only when at least one exported
function, type, field, or method carries a runtime-retained attribute.

```cpp
const WioModuleType* type = WioFindModuleType(api, "Profile");
const auto* label = WioFindModuleAttribute(
    type->attributes,
    type->attributeCount,
    "Label");

for (std::uint32_t i = 0; i < label->processorCount; ++i) {
    const auto& processor = label->processors[i];
    // canonicalTypeName, phase, hookMode, and deterministic order
}
```

The same `attributeCount`/`attributes` pair appears on `WioModuleExport`,
`WioModuleType`, `WioModuleField`, and `WioModuleMethod`. Each descriptor
contains canonical identity, normalized argument text, direct/composed/scoped
origin, and its ordered behavioral processor records.

### Preserved v0.14 value boundary

For the current SDK version, the public and documented boundary is:

- `module_api.h` for the raw ABI
- `wio_version.h`, `wio_features.h`, and `wio_values.h` for release identity,
  machine-readable capability inventory, and host-semantic values
- `wio_sdk.h` for the ergonomic C++ layer
- shared-module loading, static-module consumption, reload helpers, exports,
  commands, events, exported object/component reflection, and dynamic field access
- `TypeDescriptorView` as the host-readable type-metadata view
- `FieldInfo` as the stable per-field metadata carrier
- `WioEnum` and `WioFlagset` as the first-class host wrappers for enum identity
- `WioDynamicValue` and the current dynamic container wrappers as the runtime
  reflection payload family
- generation-aware stale-wrapper diagnostics for reload-sensitive wrappers
- ABI descriptor version `9`, module product/version inspection, stable type
  IDs, concrete type/const arguments, and supported nested dynamic fields
- runtime-retained typed-attribute descriptors on exports, types, fields, and
  methods, including ordered behavioral processor metadata

The following should be treated as stable with explicit caveats:

- `ref` / `view` exported field semantics are intentionally documented as
  outside the stable host ABI for now
- enum/flagset wrappers are stable at the `WioEnum` / `WioFlagset` level, but
  deeper host-side reflection growth beyond that should still be treated as
  future-facing

The following should not currently be read as part of the stable v0.15 SDK
contract:

- compiler-internal AST/parser/sema/codegen APIs
- host-side registration DSLs for declaring Wio types from C++
- automatic semantic reconciliation of stale object/component wrappers across
  reload generations
- `ref` / `view` field export behavior as a public host ABI promise
- direct binary exchange for partial/deferred catalog entries such as
  interface, Box, any, async task, and pool identity
- non-blocking task control and the future application/system host lifecycle
  ABI

This is the implemented v0.15 typed-metadata baseline layered on the v0.14
value bridge, not the final v1 parity claim. The required path to full
host-observable language/runtime parity is maintained in
[`WIO_SDK_EVOLUTION_PLAN.md`](./WIO_SDK_EVOLUTION_PLAN.md).

---

## 13. v0.16 Preview: Applications, Tasks, and Native Callbacks

ABI descriptor v10 adds host-driven applications and typed scalar async tasks.
Waiting remains explicit: `poll()` does not block, `wait_for(...)` is the
synchronous boundary, and main-executor completions are delivered only by an
explicit application or async-host pump.

```cpp
auto module = wio::sdk::Module::load("workspace_module.dll");

auto application = module.application();
application.start();
while (application.update(1.0 / 60.0) ==
       wio::sdk::ApplicationFrameStatus::Running)
{
    application.pump_main();
}
application.close();

auto loadCount = module.load_async<std::int32_t(std::int32_t)>("LoadCount");
auto task = loadCount(21);
task.on_complete_main([](wio::sdk::AsyncTaskStatus status) {
    // Delivered only when the host pumps the module main executor.
});
if (!task.wait_for(std::chrono::seconds(2)))
    task.cancel();
else
    std::cout << task.get() << '\n';
```

`ApplicationHost::update` is a frame poll and never waits for future async
work. Lifecycle entry is main-thread-affine. A partially failed start closes
only successfully started systems in reverse order, calls application close,
and leaves the host in a terminal closed state.

Native callbacks use `WioHostCallback` instead of a bare userdata pointer. A
descriptor returned by `HostCallback::borrowed()` remains valid only while its
C++ wrapper owns it. Native code that stores the descriptor must retain and
release it as one balanced pair:

```cpp
wio::sdk::HostCallback<std::int32_t(std::int32_t)> callback(
    [](std::int32_t value) { return value * 2; },
    true); // callback target is safe for concurrent native entry

WioHostCallback stored = callback.retained();
register_native_callback(stored);

// When unregistering:
WioReleaseHostCallback(&stored);
```

`WioInvokeHostCallback` validates scalar argument types and contains every C++
exception as `WIO_CALLBACK_FAULTED`; exceptions never unwind through a C or
module boundary. `lastError` exposes the contained diagnostic. `ref` and
`view` remain borrowed and are not legal stored callback payloads: pass an
owned SDK object/component handle or copy a stable value instead.

Native pointer identity and ownership are separate. `opaque` and
`WioBorrowedNativeResource` do not release anything. An owned native handle is
transferred through `WioOwnedNativeResource` and normally held by the move-only
RAII wrapper:

```cpp
wio::sdk::UniqueNativeResource texture(WioOwnedNativeResource{
    native_texture,
    "graphics.Texture",
    WIO_NATIVE_RESOURCE_RELEASE_THREAD_SAFE,
    0u,
    &release_texture
});

use_during_call(texture.borrow());
WioOwnedNativeResource transferred = texture.into_abi();
register_owned_texture(WioTakeNativeResource(&transferred));
```

Every non-empty owned descriptor requires one `noexcept` release operation.
`WioReleaseNativeResource` clears before invoking it, so repeated cleanup is
exactly-once. A resource whose destruction is thread-affine must perform that
dispatch inside its release function; the generic wrapper does not silently
guess an executor.

## 14. See Also

- [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- [`WIO_SDK_0_14_PARITY_MATRIX.md`](./WIO_SDK_0_14_PARITY_MATRIX.md)
- [`examples/static_cmake_consumer/README.md`](../examples/static_cmake_consumer/README.md)
- [`tests/native/sdk_exported_complex_fields_host.cpp`](../tests/native/sdk_exported_complex_fields_host.cpp)
