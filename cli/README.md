# Wio Self-Hosted CLI

This directory is the staged replacement for the native C++ command-line
frontend.

- `main.wio` owns commands that have moved to Wio.
- `vendor/argonaut.wio` is the bootstrap-compatible Argonaut-Wio surface.
- the native `wio` executable remains the stage-0 source compiler and temporary
  fallback for command families not migrated yet.
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
- raw Wio compilation is deliberately delegated to the native stage-0
  compiler;
- other tooling commands are routed through the companion, then bridged.
