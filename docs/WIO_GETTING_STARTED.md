# Wio Getting Started

This guide is the practical entrypoint for new Wio users.

It answers four simple questions:

1. How do I build or install Wio?
2. How do I run a single `.wio` file?
3. How do I create and run a project?
4. Where do I go next for native interop, packaging, and the SDK?

If you want the full command reference, see
[`WIO_CLI_REFERENCE.md`](./WIO_CLI_REFERENCE.md).

If you want the full manifest/build/package story, see
[`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md).

---

## 1. Choose Your Starting Point

There are two normal ways to use Wio.

### 1.1 Use Wio From The Source Tree

Choose this when you are:

- working on the Wio compiler itself
- testing new language features
- editing `std/`, the runtime, or the SDK
- using repo-local examples and tests

Build the toolchain:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

After that, the repo-local executable is typically:

```powershell
build\app\Debug\wio.exe
```

### 1.2 Use A Packaged Wio Distribution

Choose this when you want Wio as a normal installed toolchain rather than a
source checkout.

From a package root, the important paths are:

- `bin/wio.exe`
- `sdk/include`
- `std/`
- `cmake/WioProject.cmake`
- `QUICKSTART.md`

To print the environment commands without modifying anything:

```powershell
C:\Wio\bin\wio.exe env print --wio-root C:\Wio --shell powershell --add-path
```

To set up the user environment:

```powershell
C:\Wio\bin\wio.exe env setup --wio-root C:\Wio --set-user --add-path
```

To inspect or troubleshoot the current environment:

```powershell
C:\Wio\bin\wio.exe env status --wio-root C:\Wio
C:\Wio\bin\wio.exe env doctor --wio-root C:\Wio
```

After that, new terminals should be able to use:

```powershell
wio --help
```

---

## 2. Run A Single File

The quickest way to experiment with Wio is a single source file.

Example file:

```wio
use std::console as console;

fn Entry() -> i32 {
    console::PrintLine!("Hello from Wio");
    return 0;
}
```

### 2.1 Check It

This validates the file through parsing, semantic analysis, and normal
compiler checks without running it:

```powershell
build\app\Debug\wio.exe file check .\playground\main.wio
```

### 2.2 Run It

```powershell
build\app\Debug\wio.exe file run .\playground\main.wio
```

### 2.3 Inspect Tokens Or AST

```powershell
build\app\Debug\wio.exe file tokens .\playground\main.wio
build\app\Debug\wio.exe file ast .\playground\main.wio
```

### 2.4 Hidden Output Behavior

Wio is a compiled language, but single-file workflows do not leave generated
executables beside your source by default.

For `wio file run ...`:

- generated backend outputs go into hidden cache locations
- generated `.wio.cpp` files are treated as intermediates
- use `--emit-cpp` only when you intentionally want to inspect generated C++

This keeps playground and script folders clean.

---

## 3. Create A Project

The primary manifest is `wio.makewio`.

The easiest way to create a project is:

```powershell
build\app\Debug\wio.exe project new MyGame --output-dir C:\Projects --template wio-app
```

Useful templates currently include:

- `wio-app`
- `wio-native-app`
- `wio-module`
- `hybrid-module`

### 3.1 Inspect The Project

```powershell
build\app\Debug\wio.exe project describe --project C:\Projects\MyGame
```

### 3.2 Build The Project

```powershell
build\app\Debug\wio.exe project build --project C:\Projects\MyGame
```

### 3.3 Run The Project

```powershell
build\app\Debug\wio.exe project run --project C:\Projects\MyGame
```

The stable user-facing project flow is:

1. `project new`
2. `project describe`
3. `project build`
4. `project run`

---

## 4. First Native Bridge

Wio can call into existing C++ through `@Native`, `@CppHeader`, and `@CppName`.

Example:

```wio
@Native
@CppHeader("native_math.h")
@CppName(native_math::Multiply)
fn Multiply(lhs: i32, rhs: i32) -> i32;
```

Then build or run with the required include/source inputs:

```powershell
build\app\Debug\wio.exe file run .\tests\native\native_bridge.wio --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
```

If you want the full interop/export/SDK story, go straight to
[`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md).

---

## 5. Package Wio

To stage a packaged toolchain from the source tree:

```powershell
build\app\Debug\wio.exe package --build-dir build --config Debug --output-dir .\artifacts\packages
```

That gives you a package root containing:

- `bin/wio.exe`
- `sdk/include`
- `std/`
- installer wrappers
- `QUICKSTART.md`

The package model is intentionally CLI-first:

- installers call `wio env setup`
- environment printing uses `wio env print`
- package behavior matches the repo CLI story

---

## 6. Learn The Main Workflows

After the first successful run, the next documents are usually:

- [`WIO_CLI_REFERENCE.md`](./WIO_CLI_REFERENCE.md)
- [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- [`WIO_STD.md`](./WIO_STD.md)
- [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- [`WIO_SDK.md`](./WIO_SDK.md)

For examples and troubleshooting:

- [`WIO_EXAMPLES.md`](./WIO_EXAMPLES.md)
- [`WIO_TROUBLESHOOTING.md`](./WIO_TROUBLESHOOTING.md)

---

## 7. Recommended First-Day Path

If you want the shortest good path:

1. build Wio from source or install a package
2. run `wio file run` on a tiny file
3. create a project with `wio project new`
4. build and run that project
5. read the interop guide if native C++ is involved
6. read the SDK guide if a host loads Wio modules

That path matches the intended `v1` user experience.
