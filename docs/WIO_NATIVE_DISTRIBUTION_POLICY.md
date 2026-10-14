# Native Distribution Policy

Status: declared-dependency policy implemented for VM executable/shared/static
artifacts. Focused local qualification is Windows-only; Linux/macOS artifact and
loader qualification remains the next backend gate. Full SDK kit parity is a
separate pre-v1 requirement.

## Explicit roles, not automatic discovery

- `vm.nativeLibraries` / CMake `NATIVE_LIBRARIES` lists wire-registry producers.
  The generated project bridge is also a producer. These files must export the
  supported registry and pass ABI validation when loaded.
- `vm.runtimeLibraries` / CMake `RUNTIME_LIBRARIES` lists private shared-library
  dependencies. These are copied and checked, but never registered as VM native
  producers. Executable packaging accepts repeated `--runtime-library FILE`.
- The build author declares every non-system transitive dependency needed by
  the selected producers. Wio does not scan import tables, search PATH or guess
  which platform runtime files are redistributable. Assets remain package assets.
- Private filenames must match native import references. Windows DLLs resolve
  beside the producer through its loader search mode. Unix libraries must be
  linked with a suitable origin-relative RPATH/install name; bundling does not
  rewrite their binaries. OS/architecture/ABI compatibility remains mandatory.
- Framework bundles, arbitrary dependency directory trees, signing, installers,
  and automatic system-runtime redistribution are not supported by this policy.

## Cohorts and integrity

Executable packages with private runtime dependencies use artifact version 2:
all selected producers and private libraries live under
`wio-native/<content-id>/<original-filename>`. Role, filename, size and file hash
participate in the content identity. Changing even one private library selects
a new cohort without overwriting the previous one. Input order is significant
and deterministic. Producer-only legacy packages remain version 1, including
their existing optional versioned filenames.

The v2 footer retains the 32-byte layout used by v1. After the payload's u64
bytecode length and u32 dependency count comes u32 marker `0x324e4357`. Each
dependency stores u32 role (1 registry, 0 private), u64 size, u64 FNV-1a checksum,
u32 UTF-8 relative-path length, and path bytes. Raw bytecode follows the records.
Registry records precede private records, and the reader validates the cohort
identity against these records. At most 64 total dependencies are accepted;
each original basename is at most 1,024 UTF-8 bytes. Paths must identify one
shared cohort. Traversal, reserved
portable filenames, ASCII-case collisions, dangling links and links escaping
the package root reject. The outer payload checksum and bytecode verifier
remain mandatory.

Before embedded execution, `--verify`, or fresh deployment, every v2 dependency
must exist and match its recorded size/hash. This includes private libraries
that the VM itself does not load as registries. V1 compatibility has presence
checks, not retroactive per-library integrity guarantees. Shared/static CMake
module bundles retain their existing SHA-256 manifests and image-relative
deployment rules; their binary format is not replaced by the executable footer.

FNV-1a is an accidental-corruption/content-addressing check, **not authentication**.
Conflicting existing cohort files are additionally compared byte-for-byte and
never overwritten. The executable output cannot use the reserved `wio-native`
directory name when a cohort is present. A malicious writer able to replace
native code and rewrite metadata is outside the trust boundary. Native libraries
execute privileged code;
the distribution layer is not a sandbox or cryptographic signature system.
Cooperating publishers serialize with a directory lock; hostile concurrent
filesystem mutation is not a supported security boundary.

## Publication, repair and retention

Dependencies are prepared before atomic image publication. Failed publication
preserves the old image and removes only files created by that invocation where
possible; empty directories may remain. Replacement refuses unknown/corrupt
images and conflicting existing dependency bytes. A missing dependency can be
restored from matching build inputs by a normal project build/run; `--no-build`
fails instead of silently selecting another library.

Old cohorts are deliberately retained in a build/deployment directory. There is
no automatic garbage collector: an old executable, retained module session or
another process may still reference them. Age or the current manifest alone is
not evidence that deletion is safe. Remove a retired distribution as a whole
only after stopping its consumers and deciding its retained images are no longer
needed. Wio never auto-prunes live deployment cohorts.

`wio-vm --deploy APP --output NEW_APP` and project package copy only the selected
cohort into a fresh destination. They do not copy obsolete generations or infer
new dependencies. Relocation/renaming preserves the selected cohort identity.
Successful standalone distributions need neither Wio sources nor external
bytecode nor original producer build directories.

See [VM artifacts](WIO_VM_ARTIFACTS.md) and
[project configuration](WIO_PROJECT_SYSTEM.md) for command examples.
