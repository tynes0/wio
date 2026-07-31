# Wio Self-Hosted CLI

Wio is migrating its user-facing CLI from C++/Argonaut to
Wio/Argonaut-Wio.

## Bootstrap model

A separately installed release is not required for ordinary development.

1. The current native `wio` binary is the stage-0 compiler.
2. During the same build it compiles `cli/main.wio` into the
   `wio-selfhost` companion.
3. The native executable routes migrated commands to that companion.
4. The companion uses a named private compiler service only for the remaining
   binding backend internals. Raw `.wio` compilation remains a stage-0 compiler
   service.

`--wio-service` and `--compiler-version` are private bootstrap contracts, not
public commands. The generic `--native-cli` fallback and
`WIO_FORCE_NATIVE_CLI` bypass no longer exist.

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

Every recognized tooling command family now has Wio-owned parsing and enters
the Wio companion before using either Wio-owned behavior or an explicit
backend service. Raw source/compiler invocations remain in the native stage-0
driver. The generic fallback path has been removed from `cli/main.wio`.

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

Global empty invocation, help rewriting, version routing, command
classification, and typo suggestions are now Wio-owned as well. Direct source
and raw compiler-option invocations continue to bypass the companion.

The complete environment family is Wio-owned: argument parsing, group help,
shell-command rendering, root discovery, interactive and non-interactive
setup/removal, persistent user environment/PATH mutation, status inspection,
diagnostics, and the backend smoke probe. Reusable process/user environment
operations live in public `std::environment`; Windows uses the user Environment
registry and POSIX uses a managed `.profile` block.

The first binding wave moved the complete `bind new/import` command contract,
required-option checks, help/version handling, and typo diagnostics into Wio.
The mature C/C++ header and JSON-manifest generators remain a native backend
service while their reusable parser/model layers are prepared for migration.

The release-package migration is complete. Its option surface, validation,
distribution staging, portable-toolchain discovery/copy, package metadata,
quickstart and installer generation, archive production, and optional visual
installer orchestration all run in Wio. The old C++ package service has been
deleted.

The remaining migration work covers binding generator internals.

Every migrated command needs:

- native/self-hosted argument parity tests,
- exit-code and output compatibility tests,
- a packaged-install test,
- paths-with-spaces coverage,
- and a documented rollback boundary.
