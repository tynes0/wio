# Compiler filesystem error policy

The compiler's low-level `wio::common::filesystem` helpers use return values for ordinary operating-system failures:

- boolean operations return `false`;
- reads and listings return an empty value;
- path normalization returns the original path when normalization fails;
- `createDirectories` is idempotent and returns `true` when the directory already exists.

These helpers do not throw for missing files, invalid paths, permission failures, null `FILE*` handles, short reads, or failed writes. Callers add the operation and path context needed for a user-facing diagnostic. An allocation failure remains exceptional and is translated to `OutOfMemory`; silently treating memory exhaustion as an empty source file would hide the real failure.

The API predates a result type, so an empty file and a failed read both produce an empty string. Compiler entry points already validate existence and report “empty or not found.” New higher-level filesystem APIs should use a structured `Result` carrying the operation, path, platform error code, and message.
