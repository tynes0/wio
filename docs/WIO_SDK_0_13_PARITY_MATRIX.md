# Wio SDK 0.13 Parity Matrix

Status: normative inventory for the Wio `0.13.0` C++ host SDK.

This matrix prevents “the language accepts it” from silently becoming “the
host can exchange it.” Each row has an explicit 0.13 outcome:

- **Bridge**: generated modules and `wio_sdk.h` exchange the value.
- **Metadata**: descriptors preserve identity and shape, but no direct value
  bridge is promised.
- **Host value**: `wio_values.h` provides matching C++ semantics for host code;
  this alone is not a cross-module template ABI.
- **Opaque**: the boundary deliberately exposes identity/ownership, not layout.
- **Deferred**: planned and advertised as unavailable rather than silently
  producing an unknown descriptor.

The same inventory is available to C++ code through
`wio::sdk::features()` in `wio_features.h`.

## Values and collections

| Wio surface | 0.13 descriptor | C++ host surface | Cross-module status |
|---|---|---|---|
| numeric, bool, char, byte, bit | primitive + concrete `WioAbiType` | `wio::*` aliases, `WioValue` | Bridge; scalar ABI |
| `string` | string | `wio::string` | Bridge; owned dynamic field |
| `text` | text | `WioText`, `wio::text` | Bridge; validated UTF-8 dynamic field |
| enum | enum + members + underlying ABI | `WioEnum` | Bridge; identity retained |
| flagset | flagset + members + underlying ABI | `WioFlagset` | Bridge; raw bits retained |
| nullable | nullable + element | `WioNullable<T>` | Metadata + host value |
| `Option<T>` | option + generic argument | `WioOption<T>` | Metadata + host value |
| `Result<T>` / UnitResult | result + success argument | `WioResult<T>`, `WioUnitResult` | Metadata + host value |
| tuple | tuple + concrete arguments | `WioTuple<...>` | Metadata + host value |
| dynamic/fixed array | element; fixed extent when present | existing `WioArray` / `WioStaticArray` | Bridge; owned dynamic field |
| dictionary/tree | key + value | existing `WioDict` / `WioTree` | Bridge; owned dynamic field |
| queue | queue + element argument | `WioQueue<T>` | Metadata + host value |
| unordered/ordered set | set kind + element argument | `WioUnorderedSet<T>`, `WioOrderedSet<T>` | Metadata + host value |
| span/view | span | `WioSpan<T>` | Metadata + borrowed host value |
| byte buffer | byte-buffer | `WioByteBuffer` | Metadata + owned host value |
| byte/generic pool | generic identity where exported | `WioBytePool`, `WioPool<T>`, generation handles | Host value + ownership contract |
| function/callback field | function + parameters + return | `std::function`, dynamic function wrapper | Bridge |

## Runtime identities

| Wio surface | 0.13 outcome |
|---|---|
| exported object | reflected, generation-bound owned/borrowed handle; dynamic fields and exported methods |
| exported component | reflected, generation-bound owned/borrowed handle; dynamic fields and exported methods |
| interface | interface descriptor and ownership identity; invocation needs an exported concrete adapter |
| `opaque` | opaque descriptor/pointer identity; lifetime follows the declared native contract |
| `Box<T>` | Box descriptor + argument and `WioBox<T>` host ownership value; no template-layout ABI |
| `any` | any descriptor and `WioAny` host value; module payload exchange needs a declared adapter |
| concrete generic instance | deterministic stable ID, logical identity, ordered concrete arguments; no C++ template ABI promise |
| async task/coroutine | task descriptor + result type; task polling/callback/cancellation ABI deferred |

## Metadata and compatibility

Generated 0.13 modules use ABI descriptor version `7` and publish:

- `{major, minor, patch}` product version;
- `sizeof(WioModuleApi)` for layout negotiation;
- product-version, type-metadata-v2, and text-field capability bits;
- FNV-1a stable IDs over canonical displayed type names;
- ordered generic-argument descriptors;
- distinct descriptor kinds for all types listed above.

`Module::inspect()` returns an owned `ModuleInfo` snapshot. Raw
`TypeDescriptorView` values remain tied to the loaded module generation.
Modules that claim metadata v2 are rejected when a reachable descriptor omits
or falsifies its stable ID or required shape.

## Explicit 0.13 boundaries

The following are not silently treated as working:

- `ref`/`view` exported field mutation and borrowed call-frame escape are not a
  stable host ABI yet;
- typed and behavioral attributes are language features, but retained host
  attribute metadata is deferred to its own ABI capability;
- Option/Result/tuple/queue/set/span/Box/any have metadata and host-semantic
  values, but no raw cross-module C++ template layout;
- async task control and application/system lifecycle hosting are deferred to
  dedicated non-blocking ABI milestones;
- compiler AST, parser, semantic-analysis, and code-generation objects are not
  SDK surface.

## Conformance anchors

- `tests/sdk_013_value_surface_test.cpp` checks the header-only value and
  feature catalog.
- `tests/native/sdk_013_parity_library.wio` and
  `tests/native/sdk_013_parity_host.cpp` compile a real shared Wio module and
  verify product version, descriptor size/capabilities, Unicode text
  round-tripping, stable IDs, generic arguments, and owned inspection
  snapshots.
- Existing SDK invalid-descriptor, complex-field, and stale-binding tests
  remain the regression baseline for ABI validation, dynamic reflection, and
  reload ownership.
