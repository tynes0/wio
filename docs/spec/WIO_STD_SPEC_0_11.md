# Wio Standard Library Contract 0.11

Status: normative library contract

## 1. Failure and absence

Recoverable failure uses `Result<T>`, value-less success uses `UnitResult`,
expected absence uses `Option<T>`, and `Panic` is reserved for invariant
violations and unrecoverable misuse.

## 2. Text and bytes

Strings are UTF-8. `std::unicode` validates/decodes Unicode scalar values,
slices by codepoint or grapheme cluster, counts graphemes, computes display
width, and provides case folding. Invalid UTF-8 never yields partial success.

`StringBuilder`, `ByteWriter`, and `ByteReader` own their storage. Numeric
binary methods state endianness. Failed reads do not advance the cursor.
VarUInt is unsigned LEB128 and rejects overflow.

## 3. Serialization

JSON preserves exact integer text, validates UTF-8 and number grammar,
enforces duplicate/depth/size policy, supports deterministic key ordering,
JSON Pointer, and RFC 7396 merge patch.

`std::binary` frames contain little-endian `WIO1` magic, `u16` version, `u16`
flags, `u64` payload length, and payload. Decoders reject bad magic,
truncation, trailing bytes, and configured size-limit violations.

## 4. Random, hash, and identity

Deterministic PRNGs do not claim cryptographic security. `SecureBytes` uses
host cryptographic entropy or fails. FNV-1a is the default non-cryptographic
hash; SHA-256 is cryptographic. UUID parsing validates canonical form,
version, and RFC variant. SemVer implements numeric prerelease ordering and
ignores build metadata for precedence.

## 5. Concurrency

Mutex, condition variable, atomic i64, thread, channel, blocking channel,
Promise/Future, cancellation token, and TaskGroup share one host model.
Condition waits release and reacquire their mutex. Closing a blocking channel
wakes waiters; closed/drained receive returns `None`. Promises complete once;
timed future waits return `None` on timeout or closure without a value.

`std::async` defines the 0.11 coroutine task surface. `Sleep` and `Yield`
suspend without creating one thread per timer. `RunBlocking` moves bounded
synchronous work to the process worker pool. `All` preserves input order;
`Any` reports a ready index; `Race` cancels losers; `Timeout` is the terminal
deadline form and `TimeoutOption` is the recoverable expected-timeout form.
Task state, timed wait, cancellation, cancellation sources, generic/void task
groups, and an owner-drained dispatcher are part of the frozen surface.

Cancellation is cooperative and never terminates a native thread. Detached
task timers are drained without their remaining delay during process shutdown.
Continuations have no automatic main-thread
affinity. `WIO_ASYNC_WORKERS` selects 2 through 256 process workers when set
before first scheduler use. The detailed contract is in
[`WIO_ASYNC_MODEL.md`](../WIO_ASYNC_MODEL.md).

## 6. Networking

DNS, TCP, UDP, and URI operations return structured results. Socket ownership
is deterministic and close is idempotent. TCP sends a complete buffer or
fails. UDP preserves datagram boundaries and reports the numeric peer.
Timeouts apply to host send/receive. TLS, HTTP, proxies, and certificate APIs
are not declared complete by this contract.

## 7. Utility contracts

Structured logs produce deterministic JSON and retain typed encoded fields;
file sinks expose `UnitResult`. Regex exposes captures, find-all, replacement,
split, escape, and conservative input/pattern safety limits. Time formatting
and UTC offsets use the host calendar. Big integer, RLE compression, CSV, INI,
MIME, geometry/color, localization, UUID, and SemVer reject malformed inputs
instead of silently accepting partial data.
