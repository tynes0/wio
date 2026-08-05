# Wio Observatory

Wio Observatory is a non-trivial, multi-module Wio application that audits a
workspace and produces a machine-readable JSON report. It is designed as a
practical tour of the language and standard-library work delivered through
Wio 0.10.

## What it demonstrates

- a real `wio.makewio` project with multiple Wio modules
- `Entry(args: string[])`, manifest arguments, and ternary expressions
- default initialization for components
- `Result<T>` for filesystem, JSON, regex, and report failures
- `Option<T>` for JSON fields, semantic versions, arrays, and collections
- packed `std::semver::Version` plus component extensions
- ordinary integer const generics through `FixedWindow<T, const N>`
- ordinary component extensions through `FileAudit.Summary()`
- declaration-level native POD components
- direct native component extensions over C++ `T&`, `const T&`, and `const T*`
- recursive filesystem traversal and normalized paths
- JSON parse, validation, construction, and report writing
- regex scanning, SHA-256 hashing, deterministic MT19937 sampling, UUIDs, and time
- queue, ordered set, sorting, span, tuple, byte buffer, vector math, and reflection
- interpolated strings containing method calls and nested expressions

## Run

From the repository root:

```powershell
wio project describe --project .\examples\wio_observatory
wio project build --project .\examples\wio_observatory
wio project run --project .\examples\wio_observatory
```

The manifest supplies the sample policy and workspace. Override them with
application arguments:

```powershell
wio project run --project .\examples\wio_observatory --no-manifest-args -- `
  .\data\policy.json .\data\sample-workspace .\.wio-build\reports\custom.json
```

The run prints `OBSERVATORY AUDIT COMPLETE` and writes:

```text
examples/wio_observatory/.wio-build/reports/audit-report.json
```

The bundled fixture contains two intentional findings, so the native score is
76 and remains above the policy threshold of 70.
