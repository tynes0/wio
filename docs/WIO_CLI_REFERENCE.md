# Wio CLI Reference

This document is the command reference for the current Wio CLI.

The stable `v1` command families are:

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

---

## 2. `wio build`

`wio build` is the repo/toolchain-oriented build command.

Typical source-tree usage:

```powershell
build\app\Debug\wio.exe build --build-dir build --config Debug --configure
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
build\app\Debug\wio.exe test --build-dir build --config Debug --configure
build\app\Debug\wio.exe test --build-dir build --config Debug --list
build\app\Debug\wio.exe test --build-dir build --config Debug --filter std_result_
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

Additional arguments after the file path are forwarded to the compiler/backend
workflow when appropriate.

Example:

```powershell
wio file run .\tests\native\native_bridge.wio --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
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
```

Use this to see what Wio thinks the project root, manifest, sources, outputs,
and native/host pieces are.

### 5.3 `project build`

```powershell
wio project build --project C:\Projects\MyGame
```

Wio uses up-to-date checks so repeated builds can become very cheap when the
project is unchanged.

### 5.4 `project run`

```powershell
wio project run --project C:\Projects\MyGame
```

Use this when you want the project-aware run path rather than single-file
execution.

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

### 7.1 `env print`

```powershell
wio env print --wio-root . --shell powershell --add-path
```

Use it when you want to preview environment commands without modifying the
machine.

### 7.2 `env setup`

```powershell
wio env setup --wio-root . --no-prompt
wio env setup --wio-root C:\Wio --set-user --add-path
```

Use it when you want to:

- set `WIO_ROOT`
- set `WIO_HOME`
- add the packaged `bin` directory to `PATH`

Packaged installer wrappers delegate to this command.

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
build\app\Debug\wio.exe build --build-dir build --config Debug --configure
```

### 11.2 Check Or Run A Single File

```powershell
build\app\Debug\wio.exe file check .\playground\main.wio
build\app\Debug\wio.exe file run .\playground\main.wio
```

### 11.3 Create And Run A Project

```powershell
build\app\Debug\wio.exe project new MyGame --output-dir C:\Projects --template wio-app
build\app\Debug\wio.exe project build --project C:\Projects\MyGame
build\app\Debug\wio.exe project run --project C:\Projects\MyGame
```

### 11.4 Generate A Binding

```powershell
build\app\Debug\wio.exe bind import --header .\tests\native\binding_import_smoke.h --realm binding_import_smoke --output .\build\generated\binding_import_smoke.wio
```

### 11.5 Stage A Package

```powershell
build\app\Debug\wio.exe package --build-dir build --config Debug --output-dir .\artifacts\packages
```

### 11.6 Inspect Performance Smoke

```powershell
build\app\Debug\wio.exe perf smoke --iterations 1
```

---

## 12. See Also

- [`WIO_GETTING_STARTED.md`](./WIO_GETTING_STARTED.md)
- [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- [`WIO_TROUBLESHOOTING.md`](./WIO_TROUBLESHOOTING.md)
