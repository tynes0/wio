## Wio Programming Language

Wio is a C++20-based language experiment with a pipeline that lexes, parses,
analyzes, and lowers `.wio` files into generated C++.

### Build

```powershell
cmake -S . -B build
cmake --build build
```

### Run

```powershell
build\app\Debug\wio_app.exe tests\test1.wio
build\app\Debug\wio_app.exe tests\test1.wio --run
build\app\Debug\wio_app.exe tests\native\exported_library.wio --target static --output build\interop\exported_library.a
build\app\Debug\wio_app.exe tests\native\exported_library.wio --target shared --output build\interop\exported_library.dll
```

`--dry-run` validates the source through semantic analysis without generating or
building backend C++ output.

### Test

```powershell
ctest --test-dir build --output-on-failure
```

The CTest targets currently run the sample programs in `tests/` with
`--dry-run`.

For a one-command build + test pass on Windows:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-Wio.ps1
```

### IDE and Playground

For quick language experiments, use `playground/main.wio`.

You can build and run it directly with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-WioFile.ps1
```

You can also point the runner at any `.wio` file:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-WioFile.ps1 -File .\playground\combat_scratch.wio -Mode run
```

Supported modes are `run`, `check`, `tokens`, and `ast`.

Any extra arguments after the script parameters are forwarded directly to
`wio_app.exe`, which is useful for experimental interop and backend tuning:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-WioFile.ps1 -File .\tests\native\native_bridge.wio -Mode run --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
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

If the CMake tool window is available, you can also run the generated targets
such as `wio_playground_run`, `wio_tests`, `wio_tests_list`, or
`wio_file_<name>_run` directly from there.

### Experimental Native Interop

Wio now has an early native bridge for top-level functions:

```wio
@Native
@CppHeader("native_math.h")
@CppName(native_math::Multiply)
fn Multiply(lhs: i32, rhs: i32) -> i32;
```

Current status:

- `@Native` works for top-level bodyless functions.
- `@CppHeader("...")` injects a C++ header include into generated output.
- `@CppName(...)` maps the Wio declaration to an existing C++ symbol.
- `--include-dir`, `--link-dir`, `--link-lib`, and `--backend-arg` forward
  backend build inputs to the generated C++ compile step.

This is intentionally still an alpha bridge, but it already lets Wio call into
existing C++ code with a real end-to-end workflow.

### Experimental Library Mode

Wio now also has an early distinction between executable and library outputs:

- `--target exe` keeps the current executable workflow.
- `--target static` builds a static archive.
- `--target shared` builds a shared library.
- `--output <path>` overrides the produced file path.

For host-visible Wio functions, use `@Export`:

```wio
@Export
@CppName(WioAddNumbers)
fn AddNumbers(lhs: i32, rhs: i32) -> i32 {
    return lhs + rhs;
}
```

Current status:

- `@Export` works for top-level Wio functions with bodies.
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

### Backend Compiler

Generated C++ is compiled with `g++` by default. You can override that at
configure time:

```powershell
cmake -S . -B build -DWIO_BACKEND_CXX_COMPILER=clang++
```
