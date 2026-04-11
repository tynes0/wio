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

### Backend Compiler

Generated C++ is compiled with `g++` by default. You can override that at
configure time:

```powershell
cmake -S . -B build -DWIO_BACKEND_CXX_COMPILER=clang++
```
