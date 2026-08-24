## Wio Programming Language

[![release validation](https://github.com/tynes0/wio/actions/workflows/v1-validation.yml/badge.svg?branch=main)](https://github.com/tynes0/wio/actions/workflows/v1-validation.yml)

Wio is a C++20-based language experiment with a pipeline that lexes, parses,
analyzes, and lowers `.wio` files into generated C++.

Useful reference docs:

- [Documentation index](docs/README.md)
- [Getting started](docs/WIO_GETTING_STARTED.md)
- [CLI reference](docs/WIO_CLI_REFERENCE.md)
- [Editor ecosystem plan](docs/WIO_EDITOR_ECOSYSTEM_PLAN.md)
- [Language reference](docs/WIO_LANGUAGE_DRAFT.md)
- [v1 freeze snapshot](docs/WIO_V1_FREEZE.md)
- [v1 release plan](docs/WIO_V1_RELEASE_PLAN.md)
- [Post-v1 roadmap](docs/WIO_POST_V1_ROADMAP.md)
- [Project system](docs/WIO_PROJECT_SYSTEM.md)
- [Standard library](docs/WIO_STD.md)
- [Interop guide](docs/WIO_INTEROP_GUIDE.md)
- [Host SDK](docs/WIO_SDK.md)
- [SDK 0.14 parity matrix](docs/WIO_SDK_0_14_PARITY_MATRIX.md)
- [Wio 0.15 typed attribute specification](docs/spec/WIO_LANGUAGE_SPEC_0_15.md)
- [Wio 0.15 release notes](docs/WIO_0_15_RELEASE_NOTES.md)
- [Examples guide](docs/WIO_EXAMPLES.md)
- [Troubleshooting](docs/WIO_TROUBLESHOOTING.md)
- [FAQ](docs/WIO_FAQ.md)

Current `v1` direction in one sentence:

- `wio` is the primary CLI,
- `wio.makewio` is the primary manifest,
- `scripts/wio/*.wio` hosts source-based workflow tools,
- and `scripts/*.ps1` are compatibility wrappers.

### Build

```powershell
cmake -S . -B build
cmake --build build
```

Once the compiler has been built at least once, the preferred repo-local flow
is now the Wio CLI itself:

```powershell
wio build --build-dir build --config Debug --configure
wio help project run
wio file check .\playground\main.wio
wio file run .\playground\main.wio
```

The next direct tooling slice is also live:

```powershell
wio project new MyGame --output-dir C:\Projects --template wio-native-app
cd C:\Projects\MyGame
wio project describe
wio project build
wio project run -- "two words" --verbose
wio bind import --header .\tests\native\binding_import_smoke.h --realm binding_import_smoke --output .\build\generated\binding_import_smoke.wio
wio bind new --manifest .\tests\native\binding_manifest_smoke.json --output .\build\generated\binding_manifest_smoke.wio
wio env print --wio-root . --shell powershell --add-path
wio env setup --wio-root . --set-user --add-path
wio env status --wio-root .
wio env doctor --wio-root .
wio env remove --wio-root . --set-user --remove-path
wio package --build-dir build --config Debug --output-dir .\artifacts\packages-cli --no-zip
```

### Run

```powershell
wio tests\test1.wio
wio tests\test1.wio --run
wio tests\native\exported_library.wio --target static --output build\interop\exported_library.a
wio tests\native\exported_library.wio --target shared --output build\interop\exported_library.dll
```

`--dry-run` validates the source through semantic analysis without generating or
building backend C++ output.

### Test

```powershell
ctest --test-dir build --output-on-failure
```

Or, once `wio.exe` exists:

```powershell
wio test --build-dir build --config Debug --configure
```

The CTest targets currently run the sample programs in `tests/` with
`--dry-run`.

The legacy PowerShell helper still exists as a compatibility shim, but it now
prefers routing through the Wio CLI when possible:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1
```

### Package

To produce a packaged Wio distribution quickly, the preferred path is now:

```powershell
wio package --build-dir build --config Debug --output-dir .\artifacts\packages
```

The older compatibility helper still exists:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Pack-Wio.ps1
```

That script is now only a thin compatibility wrapper over `wio package`.

Example with an explicit configuration:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Pack-Wio.ps1 -Config Debug
```

The generated package now includes thin installer wrappers:

- `Install-Wio.ps1`
- `install-wio.sh`

They delegate to:

```powershell
bin\wio.exe env setup --wio-root <package-root> --set-user --add-path
```

So the persistent environment story now lives in the CLI itself. The packaged
compiler still works without `WIO_ROOT` or `WIO_HOME` thanks to
executable-relative toolchain discovery.

### IDE and Playground

For quick language experiments, use `playground/main.wio`.

You can build and run it directly with:

```powershell
wio file run .\playground\main.wio
```

You can also point the runner at any `.wio` file:

```powershell
wio file run .\playground\combat_scratch.wio
```

Supported modes are `run`, `check`, `tokens`, and `ast`:

```powershell
wio file check .\playground\combat_scratch.wio
wio file tokens .\playground\combat_scratch.wio
wio file ast .\playground\combat_scratch.wio
```

Any extra arguments after the file path are forwarded directly to the compiler,
which is useful for experimental interop and backend tuning:

```powershell
wio file run .\tests\native\native_bridge.wio --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
```

The older `Run-WioFile.ps1` helper still exists as a compatibility wrapper, but
it now prefers delegating to `wio file ...`.

### Wio Source Tools

The first source-based tooling files now live under `scripts/wio/`:

- `scripts/wio/print_file.wio`
- `scripts/wio/line_count.wio`

Run them through the same CLI:

```powershell
wio file run .\scripts\wio\print_file.wio
wio file run .\scripts\wio\line_count.wio
```

If you use the CMake project inside Rider or Visual Studio, these IDE-friendly
targets are also generated automatically:

- `wio_tests`
- `wio_playground_check`
- `wio_playground_run`
- `wio_playground_tokens`
- `wio_playground_ast`

Every file under `playground/*.wio` also gets its own parameterless targets:

- `wio_file_<name>_check`
- `wio_file_<name>_run`

### Rider

If your Rider build does not show `Settings | Build, Execution, Deployment | CMake`,
that is okay. You can still work comfortably with shared scripts and Run
Configurations.

Recommended Rider setup:

1. Open `Run | Edit Configurations...`.
2. Create a new `PowerShell` configuration. If that template is missing, create a
   `Native Executable` configuration and use `powershell` as the program.
3. Use this quick playground command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-WioFile.ps1 -BuildDir build-rider -Mode run
```

4. For the file currently open in the editor, use this command instead:

```powershell
powershell -ExecutionPolicy Bypass -File "$ProjectFileDir$\scripts\Run-WioFile.ps1" -BuildDir build-rider -File "$FilePath$" -Mode run
```

5. Set the working directory to `$ProjectFileDir$`.
6. Remove any extra `Build before launch` step, because the script already
   configures and builds the compiler executable.

For all tests in Rider, use:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1 -BuildDir build-rider
```

To see every registered CTest case without running them:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1 -BuildDir build-rider -List
```

To run or list a single test by name pattern:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1 -BuildDir build-rider -Filter module_lifecycle_host_interop -Test
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1 -BuildDir build-rider -Filter module_lifecycle_host_interop -List
```

If you want to build and run a native host interop example directly without
going through CTest, use:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-WioHostInterop.ps1 -BuildDir build-rider -WioFile .\tests\native\module_lifecycle.wio -HostSource .\tests\native\module_lifecycle_host.cpp -Target shared
```

For the reload example:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1 -BuildDir build-rider -Filter module_reload_host_interop -Test
```

For a larger mixed Wio/C++ demo project:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-HybridArenaDemo.ps1 -BuildDir build-rider -Config Debug
```

If the CMake tool window is available, you can also run the generated targets
such as `wio_playground_run`, `wio_tests`, `wio_tests_list`, or
`wio_file_<name>_run` directly from there.

### Native Interop

Wio now has an early native bridge for top-level functions:

```wio
using cpp::header("native_math.h");

fn Multiply(lhs: i32, rhs: i32) -> i32
    with native, cpp::name(native_math::Multiply);
```

Current status:

- `with native` marks a declaration as native-backed.
- `using cpp::header("...")` injects a C++ header include into generated output.
- `cpp::name(...)` maps the Wio declaration to an existing C++ symbol.
- `--include-dir`, `--link-dir`, `--link-lib`, and `--backend-arg` forward
  backend build inputs to the generated C++ compile step.

Legacy `@Native`, `@CppHeader`, and `@CppName` spellings remain compatibility
input, but new code should use the typed `with`/`using` forms above.

### Source-Based `std`

Wio's public standard-library surface is now moving toward source-based `.wio`
modules under [`std/`](std).

Current modules include:

- `std::console`
- `std::math`
- `std::collections`
- `std::algorithms`
- `std::strings`
- `std::convert`
- `std::chars`

Example:

```wio
use std::console;
use std::algorithms as algorithms;

fn Entry(args: string[]) -> i32 {
    let values: i32[] = [2, 4, 6, 8];
    let flags: bool[] = [true, true, false];

    console::Print($"first=${algorithms::FirstOr(values, 99)}");
    console::PrintSpace();
    console::Print($"any=${algorithms::Any(flags)}");
    console::PrintLine();
    return 0;
}
```

This keeps the user-facing library in Wio itself while still allowing
low-level runtime-backed pieces such as `std::io` to bridge into C++ through
`with native`.

Numeric and boolean text conversion is available both ergonomically and
safely:

```wio
use std::convert as convert;

let port = "8080".ToI32();
let mask = "0xff".ToU32(0);
let checked = convert::ParseF64("3.14159");
let label = convert::ToString<i32>(port);
```

`convert::Parse*` returns `std::Result<T>`, `convert::TryTo*` writes through an
output reference, and direct string `To*` members panic with a precise runtime
message when parsing fails.

### Experimental Library Mode

Wio now also has an early distinction between executable and library outputs:

- `--target exe` keeps the current executable workflow.
- `--target static` builds a static archive.
- `--target shared` builds a shared library.
- `--output <path>` overrides the produced file path.

For host-visible Wio functions, use `export::c`:

```wio
fn AddNumbers(lhs: i32, rhs: i32) -> i32
    with export::c, cpp::name(WioAddNumbers) {
    return lhs + rhs;
}
```

Current status:

- `export::c` works for top-level Wio functions with bodies.
- Export wrappers are emitted as `extern "C"` bridge functions.
- The export ABI is intentionally narrow for now: primitive parameters and
  primitive or `void` return types only.
- Static host interop is covered by an end-to-end test.
- Shared host loading is now covered by an end-to-end `LoadLibrary`/`dlopen`
  test.
- Module lifecycle hooks are now available through fixed exports:
  `@ModuleApiVersion`, `@ModuleLoad`, `@ModuleUpdate`, and `@ModuleUnload`.
- Reload-oriented state handoff now has an initial ABI through
  `@ModuleSaveState` and `@ModuleRestoreState`.
- Lifecycle-capable modules now also export a single `WioModuleGetApi()` entry
  that returns a function-pointer table for host integration.
- The shared host ABI now lives in a reusable runtime header:
  `sdk/include/module_api.h`.
- `WioModuleGetApi()` now also exposes an export registry, so hosts can
  discover Wio-callable entrypoints by logical name and inspect their primitive
  ABI metadata before resolving symbols.
- Hosts can invoke primitive `@Export` entrypoints directly through
  `WioInvokeModuleExport(...)`, which removes the need to bind every exported
  function manually with `GetProcAddress`/`dlsym`.
- `@Command("name")` and `@Event("event.name")` extend the module API with
  discoverable command and event-hook registries. Hosts can query them through
  `WioFindModuleCommand(...)`, `WioInvokeModuleCommand(...)`,
  `WioFindModuleEventHook(...)`, `WioCountModuleEventHooksForEvent(...)`,
  `WioBroadcastModuleEvent(...)`, and `WioInvokeModuleEventHook(...)`.
- Numeric `fit` lowering now goes through a runtime helper in
  `runtime/include/fit.h`, which avoids the mixed signed/unsigned
  clamp crash seen in earlier generated code.

Example:

```wio
@ModuleApiVersion
fn RuntimeAbi() -> u32 {
    return 1;
}

@ModuleLoad
fn BootModule() -> i32 {
    return 0;
}

@ModuleUpdate
fn TickModule(deltaTime: f32) {
}

@ModuleUnload
fn StopModule() {
}

@ModuleSaveState
fn SaveState() -> i32 {
    return 0;
}

@ModuleRestoreState
fn RestoreState(snapshot: i32) -> i32 {
    return 0;
}
```

### Host SDK

The public SDK now lives in
[`sdk/include/wio_sdk.h`](sdk/include/wio_sdk.h).

The full host-SDK walkthrough now lives in
[`docs/WIO_SDK.md`](docs/WIO_SDK.md).

It sits on top of `module_api.h` and is designed to make host-side usage much
shorter:

```cpp
#include <wio_sdk.h>

auto module = wio::sdk::Module::load("gameplay.dll");
auto getCounter = module.load_command<std::function<std::int32_t()>>("counter.get");
auto addCounter = wio_load_function<std::int32_t(std::int32_t)>(module, "counter.add");
auto onTick = module.load_event<void(float)>("game.tick");

std::int32_t before = getCounter();
std::int32_t afterAdd = addCounter(3);
onTick(5.0f);
```

If you already have a raw `const WioModuleApi*`, you can also bind directly:

```cpp
auto addNumbers = wio_load_function<std::function<std::int32_t(std::int32_t, std::int32_t)>>(api, "AddNumbers");
```

For hot reload, use `wio::sdk::HotReloadModule`:

```cpp
auto module = wio::sdk::HotReloadModule::load("gameplay.dll");
module.enable_auto_reload();

auto getCounter = module.load_command<std::int32_t()>("counter.get");
auto onTick = module.load_event<void(float)>("game.tick");
```

The wrapper stages DLL copies internally, preserves state when
`@ModuleSaveState`/`@ModuleRestoreState` are available, and can either reload
manually with `reload()` / `reload_from(...)` or lazily through
`reload_if_changed()` when auto-reload is enabled.

The SDK now also covers exported `object` / `component` reflection, including:

- `WioObjectType` / `WioComponentType`
- `WioObject` / `WioComponent`
- `list_fields()` / `field_info(...)`
- typed field access for primitives, strings, arrays, dicts, trees, functions,
  nested objects, and nested components
- dynamic field access through `WioDynamicValue`

### Backend Compiler

Generated C++ is compiled with `g++` by default. You can override that at
configure time:

```powershell
cmake -S . -B build -DWIO_BACKEND_CXX_COMPILER=clang++
```
