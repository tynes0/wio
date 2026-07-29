# Wio Self-Hosted CLI

This directory is the staged replacement for the native C++ command-line
frontend.

- `main.wio` owns commands that have moved to Wio.
- `vendor/argonaut.wio` is the bootstrap-compatible Argonaut-Wio surface.
- the native `wio` executable remains the stage-0 source compiler and backend
  service host for platform-heavy command internals not migrated yet.
- `--native-cli` is an internal recursion-breaking bridge and is not a public
  command.

Migration is command-by-command. A command is routed through this frontend
only after parity tests cover its public argument surface. Its native business
logic may then be replaced independently.

Current ownership:

- the complete `project new/describe/build/run/test/package` family and the
  `wio run` shorthand are fully Wio-owned;
- `file run/check/tokens/ast`, repository `build/test`, `dev build/test`, and
  `perf smoke` are fully Wio-owned;
- global help/version routing, nested help rewriting, and typo suggestions are
  fully Wio-owned;
- `env` argument parsing, group help, shell rendering, and non-interactive
  setup/removal previews are Wio-owned; persistent user mutation and detailed
  status/doctor probes temporarily use the native platform service;
- `bind new/import` argument parsing, required-option validation, help/version,
  and typo diagnostics are Wio-owned; header and manifest generation currently
  use the native binding backend;
- release `package` argument parsing, defaults, help/version, and conflicting
  installer-option validation are Wio-owned; distribution staging and archive/
  installer generation currently use the native release backend;
- raw Wio compilation is deliberately delegated to the native stage-0
  compiler;
- remaining environment, binding-generator, and release-packager internals use
  explicit backend service calls after Wio-owned parsing and validation.
