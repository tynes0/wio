# Wio 0.14.0 Release Notes

Wio 0.14 is the standard-library correctness and C++ SDK value-parity release.
It makes stable nested values behave consistently inside Wio and across a
loaded module boundary.

## SDK value parity

- Option, Result, UnitResult, tuple, array, dictionary/tree, queue, ordered and
  unordered set, span range, ByteBuffer, and nested combinations now round-trip
  through typed/dynamic host fields.
- `WioSpanRange` is a checked `(start, count)` token over host-owned storage.
- `WioByteBuffer` preserves owned bytes, cursor position, and reserved capacity.
- `WioDynamicTypedValue` gives the new portable values a real checked
  `get_dynamic()` / `set_dynamic(...)` path.
- `wio_features.h` reports both surface and Supported/Partial/Deferred state.
- Unsupported reachable export shapes fail during Wio analysis.

## Generic metadata and ABI v8

- Module ABI descriptor v8 adds `CONST_VALUE` descriptors.
- Concrete generic arguments preserve ordered type or const-value identity.
- Integer, `string`, and `text` constants retain their declared type and exact
  payload, including an empty string.
- Unspecialized user generic fields require an exported concrete specialization.

## Standard-library correctness

- JSON preserves integer and fractional/exponent tokens exactly and exposes
  checked accessors for every integer width.
- Unicode NFC, NFD, NFKC, and NFKD normalization uses vendored utf8proc 2.11.3
  and Unicode 17 data on every platform.
- `std::serialization::Codec<TValue, TWire>` provides typed user codecs.
- Regex adds bounded detailed Match/Capture records and unsafe-pattern rejection.
- Hash and seeded RNG algorithms have fixed cross-platform golden vectors.
- UTC civil-time conversion is independent of host `time_t`.
- Queue, set, ByteBuffer, pool, and span invariants have focused tests.

## Language/backend fix

Generic interface implementations whose methods accept generic parameters now
emit typed virtual-slot bridges and work through interface views.

## Compatibility

- Product version: `0.14.0`
- Module ABI descriptor: `8`
- Current contract: [`WIO_SDK_0_14_PARITY_MATRIX.md`](./WIO_SDK_0_14_PARITY_MATRIX.md)
