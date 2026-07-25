# Wio CLI Reference

This document is the command reference for the current Wio CLI.

The stable `v1` command families are:

- `wio run`
- `wio build`
- `wio test`
- `wio file`
- `wio project`
- `wio bind`
- `wio env`
- `wio package`
- `wio perf`

For the broader build and manifest model, see
[`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md).

---

## 1. Command Modes

Wio currently has two practical command styles.

### 1.1 Subcommand Mode

This is the primary user-facing interface:

```powershell
wio build ...
wio test ...
wio help project run
wio file run ...
wio project build ...
wio bind import ...
wio env setup ...
wio package ...
wio perf smoke ...
```

### 1.2 Direct File / Compiler Mode

This is still useful when you want to compile or run a single file directly:

```powershell
wio .\tests\test1.wio
wio .\tests\test1.wio --run
wio .\tests\native\exported_library.wio --target shared --output build\interop\exported_library.dll
```

Think of it as a lower-level compiler entry path, while `wio file ...` is the
more structured single-file UX.

### 1.3 Common CLI Behavior

Every command family follows the same discovery conventions:

```powershell
wio --help
wio help project run
wio env
wio bind --version
```

- `wio help <command> [subcommand]` routes to command-specific help
- a bare command group such as `wio file`, `wio bind`, `wio env`,
  `wio project`, or `wio perf` prints its group help successfully
- `--help`, `-h`, and `help` are accepted help forms
- `--version`, `-v`, and `version` are accepted version forms
- close misspellings produce a concrete suggestion at both top-level and
  subcommand level

---

## 2. `wio build`

`wio build` is the repo/toolchain-oriented build command.

Typical source-tree usage:

```powershell
wio build --build-dir build --config Debug --configure
```

Use it when you want to:

- build the compiler/runtime/app targets
- refresh a side build directory
- script repeated repo-local toolchain rebuilds

This is different from `wio project build`, which builds a user Wio project.

---

## 3. `wio test`

`wio test` is the repo/toolchain-oriented test entrypoint.

Examples:

```powershell
wio test --build-dir build --config Debug --configure
wio test --build-dir build --config Debug --list
wio test --build-dir build --config Debug --filter std_result_
```

Use it when you want to:

- run the curated CTest suite
- list registered test names
- run a filtered subset

---

## 4. `wio file`

`wio file` is the structured single-file workflow.

Supported modes:

- `check`
- `run`
- `tokens`
- `ast`

Examples:

```powershell
wio file check .\playground\main.wio
wio file run .\playground\main.wio
wio file tokens .\playground\main.wio
wio file ast .\playground\main.wio
```

### 4.1 Output Policy

`wio file run ...` intentionally does not behave like a source-adjacent script
runner.

By default:

- generated `.wio.cpp` output is treated as intermediate
- backend outputs go into hidden cache locations
- source directories stay clean

Use `--emit-cpp` when you intentionally want generated C++ preserved for
inspection.

### 4.2 Argument Forwarding

Arguments before `--` belong to the compiler/backend workflow. Arguments after
`--` are passed to `Entry(args: string[])` exactly as application arguments,
including values with spaces, values beginning with `-`, and a later literal
`--`.

Examples:

```powershell
wio file run .\tests\native\native_bridge.wio --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
wio file run .\app.wio -- "two words" --verbose --
```

---

## 5. `wio project`

`wio project` is the main user project workflow.

Subcommands:

- `new`
- `describe`
- `build`
- `run`

### 5.1 `project new`

```powershell
wio project new MyGame --output-dir C:\Projects --template wio-app
```

Templates currently include:

- `wio-app`
- `wio-native-app`
- `wio-module`
- `hybrid-module`

### 5.2 `project describe`

```powershell
wio project describe --project C:\Projects\MyGame
cd C:\Projects\MyGame
wio project describe
```

Use this to see what Wio thinks the project root, manifest, sources, outputs,
and native/host pieces are. When no project path is supplied, Wio searches the
current directory and its ancestors for `wio.makewio` or `makewio`.

### 5.3 `project build`

```powershell
wio project build --project C:\Projects\MyGame
cd C:\Projects\MyGame
wio project build
```

Wio uses up-to-date checks so repeated builds can become very cheap when the
project is unchanged.

### 5.4 `project run`

```powershell
wio project run --project C:\Projects\MyGame
cd C:\Projects\MyGame
wio project run
wio project run -- "two words" --verbose
```

Use this when you want the project-aware run path rather than single-file
execution. `wio run` is the short form of `wio project run`.

Application arguments are assembled in this order:

1. `[run].args` from `wio.makewio`
2. each repeated `--arg VALUE`
3. every value after `--`

Useful run options:

- `--no-build` launches the existing output without an up-to-date build check
- `--rebuild` forces project recompilation
- `--no-manifest-args` omits `[run].args`
- `--cwd DIR` overrides the manifest working directory
- `--print-command` prints the resolved working directory and launch command

The child program's exit code is returned unchanged by Wio.

---

## 6. `wio bind`

`wio bind` is the bridge-generation command family.

Subcommands:

- `import`
- `new`

### 6.1 `bind import`

Imports a header and bootstraps a `.wio` binding:

```powershell
wio bind import --header .\tests\native\binding_import_smoke.h --realm binding_import_smoke --output .\build\generated\binding_import_smoke.wio
```

Use this when you want a fast starting point from an existing native header.

### 6.2 `bind new`

Generates a binding from a manifest description:

```powershell
wio bind new --manifest .\tests\native\binding_manifest_smoke.json --output .\build\generated\binding_manifest_smoke.wio
```

Use this when you want more controlled or regeneration-friendly binding
generation.

---

## 7. `wio env`

`wio env` handles environment setup for repo-local and packaged toolchains.

Subcommands:

- `print`
- `setup`
- `status`
- `remove`
- `doctor`

### 7.1 `env print`

```powershell
wio env print --wio-root . --shell powershell --add-path
```

Use it when you want to preview environment commands without modifying the
machine.

### 7.2 `env setup`

```powershell
wio env setup --wio-root . --set-user --add-path
wio env setup --wio-root C:\Wio --set-user --add-path
```

Use it when you want to:

- set `WIO_ROOT`
- set `WIO_HOME`
- add the packaged `bin` directory to `PATH`

Packaged installer wrappers delegate to this command.

### 7.3 `env status`

```powershell
wio env status --wio-root .
```

Use it when you want a readable snapshot of:

- the resolved Wio toolchain root
- the active `bin` directory
- current-shell `WIO_ROOT` / `WIO_HOME`
- whether the current shell `PATH` contains Wio
- whether the persistent user environment contains Wio

### 7.4 `env remove`

```powershell
wio env remove --wio-root C:\Wio --set-user --remove-path
```

Use it when you want Wio to remove its own user-level environment values.

### 7.5 `env doctor`

```powershell
wio env doctor --wio-root .
```

Use it when you want Wio to diagnose:

- missing `PATH` entries
- missing `WIO_ROOT` / `WIO_HOME`
- Windows `Path` / `PATH` current-shell collisions
- mismatches between the current shell and the persistent user environment

---

## 8. `wio package`

`wio package` stages a distributable Wio toolchain.

Example:

```powershell
wio package --build-dir build --config Debug --output-dir .\artifacts\packages --no-zip
```

The staged package includes:

- `bin/wio.exe`
- `std/`
- `sdk/include`
- `cmake/`
- installer wrappers
- `QUICKSTART.md`

The package story is CLI-first rather than script-first.

---

## 9. `wio perf`

`wio perf` is the current built-in performance smoke entrypoint.

Example:

```powershell
wio perf smoke --iterations 3
```

The current smoke path measures a few representative user workflows such as:

- file check
- file run
- project build cold
- project build warm
- project run warm

It is meant for regression tracking and quick measurement, not for replacing a
full profiler.

---

## 10. Stable Compatibility Reading

For `v1`, this is the intended CLI reading:

- the commands above are the primary user interface
- `scripts/*.ps1` may still exist, but they are compatibility wrappers
- `wio.makewio` is the primary manifest
- `wio.project.json` is legacy / compatibility input

That means:

- new workflows should be documented as `wio ...`
- scripts should defer to the CLI whenever possible
- packaged and source-tree usage should feel like the same tool, not two
  separate products

---

## 11. Common Workflows

### 11.1 Build The Toolchain From Source

```powershell
cmake -S . -B build
cmake --build build --config Debug
wio build --build-dir build --config Debug --configure
```

### 11.2 Check Or Run A Single File

```powershell
wio file check .\playground\main.wio
wio file run .\playground\main.wio
```

### 11.3 Create And Run A Project

```powershell
wio project new MyGame --output-dir C:\Projects --template wio-app
cd C:\Projects\MyGame
wio project build
wio project run -- player-one "--safe mode"
```

### 11.4 Generate A Binding

```powershell
wio bind import --header .\tests\native\binding_import_smoke.h --realm binding_import_smoke --output .\build\generated\binding_import_smoke.wio
```

### 11.5 Stage A Package

```powershell
wio package --build-dir build --config Debug --output-dir .\artifacts\packages
```

### 11.6 Inspect Performance Smoke

```powershell
wio perf smoke --iterations 1
```

---

## 12. See Also

- [`WIO_GETTING_STARTED.md`](./WIO_GETTING_STARTED.md)
- [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- [`WIO_TROUBLESHOOTING.md`](./WIO_TROUBLESHOOTING.md)
