# Hybrid Arena Demo

This example is a larger mixed Wio/C++ integration project meant to stress the
current state of the language and toolchain.

It exercises:

- multi-file Wio modules via `use`
- nested `realm` declarations
- `component` / `object` / `interface`
- `@From`, `@GenerateCtors`, `self`, and `super`
- `ref`, `view`, and `is ... fit`
- `@Native` calls from Wio into C++
- `@Command` and `@Event` host discovery
- shared-library lifecycle, save/restore, and reload

Run it with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-HybridArenaDemo.ps1 -BuildDir build-rider -Config Debug
```

The host compiles the Wio module twice as separate DLLs, reloads between them,
restores packed state, then continues the simulation from C++.
