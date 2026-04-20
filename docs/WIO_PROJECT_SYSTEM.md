# Wio Project System

This document describes the current Wio project model used by:

- `scripts/New-WioProject.ps1`
- `scripts/Invoke-WioProject.ps1`
- `cmake/WioProject.cmake`

The design goal is straightforward:

- the easiest path should require very little configuration,
- the manual path should still let users override every important directory,
- mixed Wio + C++ projects should stay readable,
- and CMake integration should feel natural instead of “shell command glued into a build”.

---

## 1. The Two Ways To Use It

Wio now supports two practical workflows.

### 1.1 The Easy Path

Create a project from a template:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\New-WioProject.ps1 -Name MyGame -OutputDir C:\Projects
```

Then build it:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Projects\MyGame\build.ps1
```

Or build and run it:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Projects\MyGame\run.ps1
```

### 1.2 The Manual Path

Create your own folder structure and write a `wio.makewio` file by hand.

Then build it directly:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Wio\scripts\Invoke-WioProject.ps1 -Project C:\Projects\MyGame
```

`-Project` may be:

- the project directory
- a `wio.makewio` file
- a `makewio` file
- a legacy `wio.project.json` file

If `-Project` points to a directory, Wio searches in this order:

1. `wio.makewio`
2. `makewio`
3. `wio.project.json`

So the recommended user-facing format is now:

- `wio.makewio`

Legacy JSON manifests are still supported for compatibility and tooling.

---

## 2. The Recommended File

The primary manifest format is `wio.makewio`.

It is intentionally human-written, line-oriented, and easy to edit.

Example:

```ini
schemaVersion = 1
name = "MyGame"
template = "hybrid-module"

[toolchain]
buildDir = "build"
config = "Debug"

[wio]
entry = "wio/module.wio"
target = "shared"
sourceRoots = ["wio"]
includeDirs = ["native/include"]
linkDirs = ["native/lib"]
linkLibraries = []
nativeSources = []
backendArgs = []

[host]
enabled = true
compiler = "g++"
sourceRoots = ["host"]
includeDirs = ["host/include", "native/include"]
linkDirs = ["native/lib"]
linkLibraries = []
compilerArgs = []

[build]
buildDir = ".wio-build"
config = "Debug"

[outputs]
directory = ".wio-build/interop"
baseName = "mygame"
wioName = "mygame"
hostName = "mygame.host"

[run]
passLibraryPath = true
args = []
workingDirectory = "."
```

Rules:

- empty lines are allowed
- full-line comments may start with `#`, `;`, or `//`
- inline comments with `#` or `//` are also accepted outside quoted strings
- booleans are `true` / `false`
- arrays use `[a, b, c]`
- quotes are optional for simple strings, but recommended when values contain spaces
- section names map to nested manifest objects such as `[wio]`, `[host]`, `[outputs]`

You may also still use the older JSON form if you want machine-generated configuration.

---

## 3. Quick Start Layouts

### 3.1 Generated Layout

The `hybrid-module` template creates:

```text
MyGame/
  wio/
    module.wio
  host/
    main.cpp
    include/
  native/
    include/
    src/
    lib/
  wio.makewio
  build.ps1
  run.ps1
  CMakeLists.txt
  README.md
  .gitignore
```

Meaning:

- `wio/`: Wio source files
- `host/`: C++ host source files
- `host/include/`: host-only headers
- `native/include/`: headers visible to Wio `@CppHeader(...)`
- `native/src/`: extra native C/C++ files compiled into the Wio backend output
- `native/lib/`: prebuilt native libraries used by Wio or the host

### 3.2 From Scratch

If you do not want a template at all, the smallest useful manual project is:

```text
MyTool/
  wio/
    main.wio
  wio.makewio
```

with:

```ini
schemaVersion = 1
name = "MyTool"

[wio]
target = "exe"
entry = "wio/main.wio"

[host]
enabled = false
```

and:

```wio
use std::io as console;

fn Entry() -> i32 {
    console::Print("Hello from Wio.");
    return 0;
}
```

Then:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Wio\scripts\Invoke-WioProject.ps1 -Project C:\Projects\MyTool
```

---

## 4. What Gets Auto-Detected

If the user does not override a path, the runner falls back to stable defaults.

### 4.1 Wio Entry

If `wio.entry` is missing, the runner searches:

1. `wio/module.wio`
2. `wio/main.wio`
3. `src/module.wio`
4. `src/main.wio`
5. `main.wio`

If none exist, it looks for exactly one `.wio` file under `wio/` or `src/`.

If that still is not unique, the runner stops and asks for `wio.entry`.

### 4.2 Wio Source Roots

Only the entry file is compiled directly.

Other `.wio` files are discovered through `use ...` resolution.

If `wio.sourceRoots` is not provided, Wio uses:

- `wio/`
- `src/`
- the directory that contains the entry file

Aliases accepted:

- `wio.sourceRoots`
- `wio.usePaths`
- `wio.moduleDirs`

### 4.3 Wio Native Includes

If `wio.includeDirs` is omitted, Wio uses:

- `native/include`

This is the default user-owned place for headers referenced by:

```wio
@CppHeader("foo.h")
```

### 4.4 Wio Native Sources

If `wio.nativeSources` is omitted, Wio auto-discovers:

- `.c`
- `.cc`
- `.cpp`
- `.cxx`

under:

- `native/src`

### 4.5 Wio Native Link Dirs

If `wio.linkDirs` is omitted, Wio uses:

- `native/lib`

Libraries are not auto-linked.

You still explicitly choose them through `wio.linkLibraries`.

`wio.linkLibraries` may contain:

- bare linker names such as `zlib`
- platform library names such as `opengl32.lib`
- explicit relative paths such as `native/lib/mylib.lib`
- explicit absolute paths
- raw flags such as `-pthread`

### 4.6 Host Sources

If `host.enabled = true` and `host.sources` is omitted, host sources are auto-discovered from:

- `host/`
- `host/src/`

with:

- `.c`
- `.cc`
- `.cpp`
- `.cxx`

### 4.7 Host Include Dirs

The host always receives:

- `<WIO_ROOT>/sdk/include`

So host code can always include:

```cpp
#include <wio_sdk.h>
#include <module_api.h>
```

If `host.includeDirs` is omitted, the runner also adds:

- `host/include`
- `native/include`

### 4.8 Host Link Dirs

If `host.linkDirs` is omitted, the runner uses:

- `native/lib`

### 4.9 Output Paths

If `outputs.directory` is omitted, outputs go to:

- `.wio-build/interop`

If `outputs.baseName` is omitted, Wio derives one from the project name.

Default output names:

- Wio executable: `<baseName>.exe` on Windows, `<baseName>` on Unix
- Wio shared library: `<baseName>.dll` / `.so` / `.dylib`
- Wio static library: `<baseName>.a`
- host executable: `<baseName>.host.exe` on Windows, `<baseName>.host` on Unix

### 4.10 Explicit Empty Arrays

If you set an array field to `[]`, you are explicitly turning off auto-discovery for that field.

Examples:

- `nativeSources = []`
- `includeDirs = []`
- `sourceRoots = []`

This matters when you want total manual control.

---

## 5. Full Override Example

This is the style for users who want to choose every directory themselves.

```ini
schemaVersion = 1
name = "CustomPaths"

[wio]
entry = "scripts/game/main.wio"
target = "shared"
sourceRoots = ["scripts", "vendor/wio"]
includeDirs = ["bridge/include", "third_party/sdk/include"]
linkDirs = ["bridge/lib", "third_party/sdk/lib"]
linkLibraries = ["core_bridge", "third_party/sdk/lib/external.lib"]
nativeSources = ["bridge/src/runtime_glue.cpp", "bridge/src/exports.cpp"]

[host]
enabled = true
compiler = "clang++"
sources = ["app/main.cpp", "app/bootstrap.cpp"]
includeDirs = ["app/include", "bridge/include"]
linkDirs = ["bridge/lib"]
linkLibraries = ["host_support"]

[outputs]
directory = "artifacts/dev"
baseName = "gameplay"
hostName = "gameplay_runner"
```

Nothing in this layout depends on templates.

As long as the paths exist, the runner accepts it.

---

## 6. Available Templates

`New-WioProject.ps1` currently supports:

- `hybrid-module`
  - shared Wio module plus native C++ host
- `wio-app`
  - plain Wio executable
- `wio-native-app`
  - plain Wio executable plus example native bridge
- `wio-module`
  - exported Wio shared module without host

Examples:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\New-WioProject.ps1 -Name Gameplay -Template wio-app
powershell -ExecutionPolicy Bypass -File .\scripts\New-WioProject.ps1 -Name NativeTool -Template wio-native-app
powershell -ExecutionPolicy Bypass -File .\scripts\New-WioProject.ps1 -Name ScriptModule -Template wio-module
```

---

## 7. CMake Integration

Wio ships:

- `cmake/WioProject.cmake`

This helper now works at two levels:

- build/run custom targets
- imported-target integration for static Wio outputs

### 7.1 Smallest CMake Usage

If your source tree already contains `wio.makewio` or `wio.project.json` in the current source directory:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_scripts LANGUAGES NONE)

set(WIO_ROOT "$ENV{WIO_ROOT}" CACHE PATH "Path to the Wio toolchain root")
include("${WIO_ROOT}/cmake/WioProject.cmake")

wio_add_project_targets(gameplay_scripts)
```

This creates:

- `gameplay_scripts_build`
- `gameplay_scripts_run`
- `gameplay_scripts::wio`
- `gameplay_scripts::host` when the manifest builds a host executable

### 7.2 Explicit Manifest Path

You can still pass a specific file:

```cmake
wio_add_project_targets(gameplay_scripts
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.makewio"
    WIO_ROOT "${WIO_ROOT}"
)
```

### 7.3 Static Host-Link Integration

If the Wio project output is `static`, CMake exposes a real imported library target:

```cmake
add_executable(my_game main.cpp)

wio_add_project_targets(gameplay_static
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/wio.makewio"
    WIO_ROOT "${WIO_ROOT}"
)

wio_target_link_project(my_game PRIVATE gameplay_static)
```

What `wio_target_link_project(...)` does:

- links `gameplay_static::wio`
- adds a build dependency on `gameplay_static_build`
- forwards Wio SDK headers
- forwards the runtime static library

Current scope:

- this is the recommended path for `static`
- `shared` outputs are still primarily runtime-loaded

### 7.4 Query The Built Artifact Path

For runtime loading or custom launchers:

```cmake
wio_get_project_output(GAMEPLAY_DLL gameplay_scripts)
message(STATUS "Wio output: ${GAMEPLAY_DLL}")
```

If the manifest builds a host executable too:

```cmake
wio_get_host_output(GAMEPLAY_HOST gameplay_scripts)
message(STATUS "Host output: ${GAMEPLAY_HOST}")
```

### 7.5 Why CMake First

Direct `.vcxproj` generation is still possible in theory, but the project system currently prefers:

- CMake for portability
- one integration surface for Rider, CLion, and Visual Studio
- less fragile maintenance

That is the intentional current tradeoff.

---

## 8. Packaged Wio Layout

The packaged distribution is meant to look like a real user-facing toolchain, not like a source checkout.

Expected layout:

```text
<WIO_ROOT>/
  bin/
    wio.exe
  cmake/
    WioProject.cmake
  docs/
    WIO_PROJECT_SYSTEM.md
  runtime/
    include/
    lib/
      libwio_runtime.a
  sdk/
    include/
  scripts/
    Invoke-WioProject.ps1
    Invoke-WithSanitizedPath.ps1
    New-WioProject.ps1
  std/
```

Important points:

- users should think in terms of `wio` or `bin/wio`, not `wio_app`
- the packaged compiler resolves `runtime/include`, `sdk/include`, `std`, and the runtime archive from the toolchain root
- `Invoke-WioProject.ps1` supports both layouts:
  - source checkout with a build tree
  - packaged distribution with a prebuilt `bin/wio`

### 8.1 Building A Package From The Source Tree

After configuring the Wio repo build once, you can produce a packaged layout with:

```powershell
cmake --build .\build --target wio_dist --config Debug
```

By default this writes the package to:

- `build/dist`

You can override that path with the CMake cache variable:

- `WIO_DIST_DIR`

### 8.2 Using The Package

Point `WIO_ROOT` to the package root:

```powershell
$env:WIO_ROOT = "C:\Wio\dist"
```

Then all of these work:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Wio\dist\scripts\New-WioProject.ps1 -Name MyGame -OutputDir C:\Projects
powershell -ExecutionPolicy Bypass -File C:\Wio\dist\scripts\Invoke-WioProject.ps1 -Project C:\Projects\MyGame
```

And inside CMake:

```cmake
set(WIO_ROOT "C:/Wio/dist" CACHE PATH "Path to packaged Wio")
include("${WIO_ROOT}/cmake/WioProject.cmake")
```

---

## 9. How Wio And C++ See Each Other

### 9.1 From C++ Into Wio

Host code uses:

```cpp
#include <wio_sdk.h>
#include <module_api.h>
```

The project runner and the imported CMake targets expose:

- `<WIO_ROOT>/sdk/include`

### 9.2 From Wio Into Native C++

Wio code uses:

```wio
@CppHeader("foo.h")
```

That header may come from:

- toolchain runtime headers
- toolchain SDK headers
- any directory listed in `wio.includeDirs`

The default user-owned place is:

- `native/include`

---

## 10. Practical Build Commands

### 9.1 Build From A Project Root

```powershell
powershell -ExecutionPolicy Bypass -File C:\Wio\scripts\Invoke-WioProject.ps1 -Project C:\Projects\MyGame -NoRun
```

### 9.2 Build And Run From A Project Root

```powershell
powershell -ExecutionPolicy Bypass -File C:\Wio\scripts\Invoke-WioProject.ps1 -Project C:\Projects\MyGame
```

### 9.3 Ask Wio What It Thinks The Project Is

```powershell
powershell -ExecutionPolicy Bypass -File C:\Wio\scripts\Invoke-WioProject.ps1 -Project C:\Projects\MyGame -Describe
```

This prints normalized JSON metadata that other tooling, especially CMake, can consume.

---

## 11. Example Project

A complete static-link example lives under:

- `examples/static_cmake_consumer`

It demonstrates:

- `wio.makewio`
- `wio_add_project_targets(...)`
- `wio_target_link_project(...)`
- a native C++ executable consuming the generated static Wio library

The same example works against:

- the source checkout as `WIO_ROOT`
- a packaged distribution produced by `wio_dist`

---

## 12. Design Intent

The project system is opinionated where it helps and permissive where it matters.

The intended user experience is:

- create a project and immediately run it without memorizing long commands
- keep a single readable manifest file
- move to a manual custom layout when needed without abandoning the tool
- integrate into an existing CMake tree without rebuilding everything from scratch

The next layer after this is not more manifest complexity.

It is:

- stronger std support
- stronger SDK packaging
- cleaner host/runtime distribution
- and eventually richer generator/install flows built on top of the same project description model
