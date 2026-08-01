# Packaged Quickstart

This example is the release-grade reference for a normal installed Wio setup.

## 1. Install Wio

Recommended on Windows:

```powershell
.\WioSetup-0.3.0-windows-x64.exe
```

Or with the lightweight bootstrap script:

```powershell
powershell -ExecutionPolicy Bypass -File .\wio-0.5.0-windows-x64-release-installer.ps1
```

After installation, open a new terminal and verify:

```powershell
wio
wio env status
wio env doctor --backend-smoke
```

## 2. Create A Hello World Project

```powershell
wio project new HelloWorld --output-dir C:\Projects --template wio-app
wio project run --project C:\Projects\HelloWorld
```

## 3. Run A Single File

```powershell
wio file run .\test.wio
```

Example source:

```wio
use std::console as console;

fn Entry() {
    console::PrintLine!("Hello from packaged Wio.");
}
```

## 4. Package Layout

Important paths in the packaged toolchain:

- `bin/wio.exe`
- `sdk/include`
- `std/`
- `cmake/WioProject.cmake`
- `toolchains/windows-x64-mingw/` on Windows packages
