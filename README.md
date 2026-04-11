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

### Backend Compiler

Generated C++ is compiled with `g++` by default. You can override that at
configure time:

```powershell
cmake -S . -B build -DWIO_BACKEND_CXX_COMPILER=clang++
```
