# Wio SDK 0.14 Parity Matrix

Status: normative value-boundary inventory for Wio `0.14.0`.

Wio 0.14 separates product version, ABI descriptor revision, feature surface,
and support state. A feature is never inferred from a C++ mirror type alone:
hosts inspect `wio::sdk::features()` and module capabilities before use.

Support states mean:

- **Supported**: the advertised 0.14 surfaces are complete and conformance-tested.
- **Partial**: a useful bounded surface exists, but it is not a general bridge.
- **Deferred**: the public contract deliberately advertises no usable surface yet.
- **Rejected**: an unsupported exported shape fails during Wio analysis.

## Stable value bridge

| Wio value | Descriptor / host value | 0.14 boundary |
|---|---|---|
| numeric, bool, char, byte, bit | scalar ABI aliases | Supported direct field bridge |
| `string` | owned UTF-8 byte string | Supported dynamic field bridge |
| `text` | validated `WioText` | Supported dynamic field bridge |
| enum / flagset | identity, members, underlying ABI | Supported typed/dynamic bridge |
| `Option<T>` | `WioOption<T>` | Supported recursive dynamic bridge |
| `Result<T>` / `UnitResult` | `WioResult<T>`, `WioUnitResult` | Supported recursive dynamic bridge |
| tuple | `WioTuple<...>` | Supported recursive dynamic bridge |
| dynamic/fixed array | `WioArray`, `WioStaticArray` | Supported recursive dynamic bridge |
| dictionary / tree | `WioDict`, `WioTree` | Supported recursive dynamic bridge |
| queue | `WioQueue<T>` | Supported recursive dynamic bridge |
| unordered / ordered set | `WioUnorderedSet<T>`, `WioOrderedSet<T>` | Supported recursive dynamic bridge |
| `Span` | `WioSpanRange` | Supported checked range token over host-owned storage |
| `ByteBuffer` | `WioByteBuffer` | Supported owned bridge preserving bytes, cursor, and capacity |

Nested combinations are validated recursively. A supported outer collection
does not make an unsupported payload valid.

These portable bridge values also participate in generic dynamic field access
through `WioDynamicValue::typed(...)` and checked `WioDynamicTypedValue`
extraction; they do not need an unsafe cast to Wio's internal C++ layout.

## Metadata and bounded values

| Surface | Support | Contract |
|---|---|---|
| concrete generic instance | Partial | stable identity and ordered arguments; no raw C++ template ABI |
| integer/string/text const generic | Supported metadata | ABI v8 `CONST_VALUE`, declared type, exact payload |
| nullable | Partial | descriptor and host value; general dynamic bridge is not advertised |
| byte/generic pool | Partial | host semantics and owner-safe handles; pool instances do not cross modules |
| interface | Partial | identity metadata; invocation requires an exported concrete adapter |
| `Box<T>` / `any` | Partial | host values; arbitrary module payload exchange is not advertised |
| async task/coroutine | Partial | result metadata; task control ABI belongs to v0.15 |
| typed attributes | Deferred | retained host metadata capability is not published in v0.14 |
| application host | Deferred | lifecycle/executor host ABI belongs to v0.15 |

## ABI descriptor v8

Generated 0.14 modules publish product version `0.14.0`, descriptor layout
size, capability bits, stable FNV-1a type IDs, ordered concrete arguments, and
const-value descriptors containing their declared type and exact value text.
An empty const `string`/`text` uses a non-null zero-length payload.

`Module::inspect()` owns its snapshot. Raw `TypeDescriptorView` values remain
generation-bound to the loaded module.

## Compile-time export boundary

Wio analysis rejects public exports whose reachable shape has no 0.14 bridge.
Covered failures include unsupported Option/Result payloads, nested collection
payloads, queue/tuple payloads, and unspecialized user generic components.
User generics must expose an explicitly exported concrete specialization.

## Conformance anchors

- `wio_sdk_014_feature_inventory` checks the machine-readable support table.
- `wio_test_sdk_014_option_host_interop` and
  `wio_test_sdk_014_result_host_interop` cover Option/Result/unit values.
- `wio_test_sdk_014_nested_collections_host_interop` covers recursive values.
- `wio_test_sdk_014_sequence_containers_host_interop` and
  `wio_test_sdk_014_tuple_host_interop` cover queue/set/tuple values.
- `wio_test_sdk_014_span_range_host_interop` and
  `wio_test_sdk_014_byte_buffer_host_interop` cover span and buffer semantics.
- `wio_test_sdk_014_const_generic_metadata_host_interop` covers descriptor v8.
- the `wio_invalid_exported_*` tests keep unsupported boundaries in analysis.

The historical 0.13 contract remains in
[`WIO_SDK_0_13_PARITY_MATRIX.md`](./WIO_SDK_0_13_PARITY_MATRIX.md).
