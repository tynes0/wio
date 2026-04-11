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

If the CMake tool window is available, you can also run the generated targets
such as `wio_playground_run`, `wio_tests`, or `wio_file_<name>_run` directly
from there.

### Backend Compiler

Generated C++ is compiled with `g++` by default. You can override that at
configure time:

```powershell
cmake -S . -B build -DWIO_BACKEND_CXX_COMPILER=clang++
```
