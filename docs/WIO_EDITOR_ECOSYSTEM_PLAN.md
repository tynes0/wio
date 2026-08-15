# Wio Editor Ecosystem Plan

This document defines how Wio editor integrations reach and remain aligned
with the compiler before `v1.0.0`.

The product targets are:

- `wio-vscode` for Visual Studio Code;
- `wio-vs` for Visual Studio 2022 and later supported versions;
- one IntelliJ Platform plugin, provisionally named `wio-jetbrains`, tested and
  distributed for both JetBrains Rider and CLion.

Rider and CLion share the IntelliJ Platform, so syntax, project discovery,
language-server transport, navigation, diagnostics, formatting, and Wio
commands should live in one plugin codebase. Product-specific adapters are
allowed only where Rider or CLion exposes a genuinely different project/build
integration API. We should not maintain separate Wio parsers for the two IDEs.

---

## 1. Shared Architecture

The compiler is the semantic authority. Every editor uses the same layers:

1. a small editor-native client handles activation, settings, commands, UI,
   installation discovery, and protocol transport;
2. the Wio CLI/compiler owns project discovery, parsing, name resolution,
   types, generics, extensions, attributes, diagnostics, formatting, and
   language intelligence;
3. a versioned language-service protocol carries diagnostics, completion,
   hover, signatures, definitions, references, symbols, semantic tokens,
   formatting, and workspace/project state;
4. lightweight TextMate or IDE-native syntax rules provide immediate color
   before the semantic service is ready.

No editor extension may silently invent different Wio semantics. A temporary
source index may provide a fallback, but compiler results win and the fallback
must be removable.

---

## 2. Version and Compatibility Contract

Official editor artifacts use the same product version as Wio:

| Wio | VS Code | Visual Studio | Rider/CLion | Website docs |
| --- | --- | --- | --- | --- |
| `0.13.0` | `0.13.0` | planned | planned | `0.13.0` |
| `0.16.0` | `0.16.0` | preview | preview | `0.16.0` |
| `0.18.0` | `0.18.0` | release gate | release gate | `0.18.0` |
| `1.0.0` | `1.0.0` | `1.0.0` | `1.0.0` | `1.0.0` |

The product version and language-service protocol version are independent.
Clients perform a handshake and report an actionable compatibility diagnostic
instead of guessing when the protocol is unsupported.

All official extension packages are produced and tested during the matching
Wio release freeze. An editor-only emergency patch may advance first within the
same major/minor compatibility line, but the next Wio patch realigns it.

---

## 3. Delivery Plan

### v0.13.0 - Alignment Baseline

- align `wio-vscode` package/VSIX version with Wio;
- add `text`, Unicode literals, named typed attributes, textual const generics,
  match guards, fixed-array extent inference, and generic extension methods to
  syntax/completion/snippets/tests;
- fix stale README and compatibility claims;
- publish machine-readable product/SDK/editor/docs version metadata;
- freeze project-versus-standalone invocation behavior as a shared client rule.

### v0.14.0-v0.15.0 - Compiler Service Foundation

- stable diagnostic codes and JSON spans;
- compiler-owned project discovery and incremental workspace sessions;
- language-service handshake, capabilities, cancellation, and process lifetime;
- diagnostics, symbols, completion, hover, definition, references, signatures,
  semantic tokens, and formatting protocol slices;
- shared fixtures proving identical answers across clients.

### v0.16.0 - New Client Previews

- create `wio-vs` with VSIX packaging, Wio project/file recognition, syntax,
  diagnostics, build/run/test, and compiler-service transport;
- create the IntelliJ Platform plugin with Rider and CLion compatibility,
  `.wio` recognition, syntax, diagnostics, project discovery, commands, and
  compiler-service transport;
- publish preview artifacts using the Wio product version;
- test installation, upgrade, uninstall, paths with spaces/Unicode, and missing
  or incompatible toolchains.

### v0.17.0 - Package and Native-project Integration

- package restore/status and native dependency diagnostics in every client;
- Visual Studio solution/project integration where useful without requiring a
  second build system;
- Rider/CLion CMake/native-project integration through shared manifest truth;
- source navigation into packages and generated/native boundaries;
- preview-channel packaging in the Wio release pipeline.

### v0.18.0 - v1 Editor Parity Gate

The three clients must provide the same baseline capabilities:

- diagnostics and fix information;
- completion and signature help;
- hover, definition, references, document/workspace symbols;
- semantic tokens and canonical formatting;
- manifest-aware build, run, test, restore, and doctor commands;
- compiler/SDK version and protocol compatibility reporting;
- settings and command documentation;
- install, upgrade, uninstall, and smoke-test automation.

Advanced refactors and debugger support remain post-v1 unless they become
release blockers. The baseline above is mandatory before `v1.0.0`.

---

## 4. Repository and Naming Rule

- keep `wio-vscode` as the VS Code client;
- use `wio-vs` for Visual Studio;
- prefer `wio-jetbrains` for the shared Rider/CLion plugin so its name does not
  promise support for only one IntelliJ product;
- keep protocol fixtures and compiler-owned semantics in the main `wio`
  repository;
- keep each client thin, independently packageable, and version-aligned.

If marketplace naming later favors `wio-rider`, it may be the published product
name while the source remains shared and CLion-compatible. That is a branding
decision, not an architectural fork.

---

## 5. Release Gate

For each supported client and platform:

1. install the packaged matching Wio toolchain;
2. install the packaged editor extension/plugin;
3. open console, library, native, and application fixtures;
4. compare diagnostics and language queries with compiler-owned golden results;
5. build, run, test, restore, and invoke doctor through the client;
6. verify an incompatible compiler produces a compatibility message;
7. verify upgrade and uninstall leave no stale executable or protocol process.

An editor is not declared v1-ready solely because syntax highlighting works.
