# Wio Self-Hosted CLI

Wio is migrating its user-facing CLI from C++/Argonaut to
Wio/Argonaut-Wio.

## Bootstrap model

A separately installed release is not required for ordinary development.

1. The current native `wio` binary is the stage-0 compiler.
2. During the same build it compiles `cli/main.wio` into the
   `wio-selfhost` companion.
3. The native executable routes migrated commands to that companion.
4. The companion uses the internal `--native-cli` bridge only for command
   families that have not moved to Wio yet. Raw `.wio` compilation remains a
   stage-0 compiler service.

`--native-cli` exists solely to prevent recursive dispatch. It is not a public
command contract. `WIO_FORCE_NATIVE_CLI=1` is the emergency/debug bypass.

Release packages install both binaries in `bin/`. Clean installed-package
qualification must prove that the companion exists and that migrated commands
pass through it.

## Release bootstrap

Release reproducibility will use one of these equivalent stage-0 inputs:

- the previous pinned stable Wio release, or
- a reviewed generated-C++ snapshot whose source/compiler versions and hash
  are recorded.

The newly built compiler then rebuilds the self-hosted CLI and compares the
result through behavior/parity tests. A release must never silently depend on
an arbitrary Wio installation from the developer machine.

## Migration status

Every recognized tooling command family now enters the Wio companion before
using either Wio-owned behavior or the temporary bridge. Raw source/compiler
invocations remain in the native stage-0 driver.

The complete project lifecycle is Wio-owned:

- `wio project new`
- `wio project describe`
- `wio project build`
- `wio project run` and its `wio run` shorthand
- `wio project test`
- `wio project package`

Argument parsing, manifest discovery and normalization, compiler command
construction, native host builds, process execution, regex test discovery, and
directory packaging all run in Wio. The project implementation invokes the
native executable only as a raw source compiler; it no longer crosses the
native `project ...` command bridge.

The second migration wave is also Wio-owned:

- `wio file run/check/tokens/ast`,
- repository `wio build` and `wio test`,
- `wio dev build/test`,
- and `wio perf smoke`.

This wave added public `std::environment` and `std::statistics` modules instead
of embedding platform/cache and measurement logic inside the CLI.

The remaining migration work covers environment management, binding
generation, release packaging, and global help/dispatch cleanup.

Every migrated command needs:

- native/self-hosted argument parity tests,
- exit-code and output compatibility tests,
- a packaged-install test,
- paths-with-spaces coverage,
- and a documented rollback boundary.
