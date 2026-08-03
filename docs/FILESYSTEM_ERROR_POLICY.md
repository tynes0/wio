# Compiler filesystem error policy

The compiler's low-level `wio::common::filesystem` helpers use return values for ordinary operating-system failures:

- boolean operations return `false`;
- reads and listings return an empty value;
- path normalization returns the original path when normalization fails;
- `createDirectories` is idempotent and returns `true` when the directory already exists.

These helpers do not throw for missing files, invalid paths, permission failures, null `FILE*` handles, short reads, or failed writes. Callers add the operation and path context needed for a user-facing diagnostic. An allocation failure remains exceptional and is translated to `OutOfMemory`; silently treating memory exhaustion as an empty source file would hide the real failure.

This low-level compiler API predates Wio's result type, so an empty file and a
failed read both produce an empty string. Compiler entry points validate
existence and add diagnostic context.

The user-facing `std::fs` layer no longer inherits that ambiguity. Its
canonical fallible operations return `std::Result<T>` and preserve
`ResultDomain::fs`, a portable filesystem error code, the native platform code,
and an actionable message. Reads/writes, recursive enumeration, metadata,
permissions, copy/move/remove, canonicalization, and atomic replacement share
this contract. Explicit `Try*` and `*Raw` helpers are retained only as low-level
or compatibility escapes.
