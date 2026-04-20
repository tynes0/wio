# Static CMake Consumer

This example demonstrates the intended static-link workflow for Wio projects.

What it shows:

- a Wio project described by `wio.makewio`
- CMake integration through `cmake/WioProject.cmake`
- a native C++ executable linked against a generated static Wio library
- runtime access through the public SDK and `WioModuleGetApi()`

## Layout

- `wio.makewio`
  - the Wio project manifest
- `wio/module.wio`
  - the exported Wio functions compiled into a static library
- `host/main.cpp`
  - the native C++ consumer

## Build From The Source Tree

```powershell
cmake -S .\examples\static_cmake_consumer -B .\build-codex\examples\static_cmake_consumer -DWIO_ROOT=C:\Users\cihan\RiderProjects\wio
cmake --build .\build-codex\examples\static_cmake_consumer --config Debug
.\build-codex\examples\static_cmake_consumer\Debug\wio_static_consumer.exe
```

## Build Against A Packaged Wio Distribution

First produce a distribution:

```powershell
cmake --build .\build --target wio_dist --config Debug
```

That creates a packaged layout under:

- `build/dist`

Then point the example to that package:

```powershell
cmake -S .\examples\static_cmake_consumer -B .\build-codex\examples\static_cmake_consumer_pkg -DWIO_ROOT=C:\Users\cihan\RiderProjects\wio\build\dist
cmake --build .\build-codex\examples\static_cmake_consumer_pkg --config Debug
.\build-codex\examples\static_cmake_consumer_pkg\Debug\wio_static_consumer.exe
```

Expected output:

```text
Static consumer: export=42 command=55
```
