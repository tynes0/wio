# Hybrid Module

This is the smallest release-grade mixed Wio + host C++ project example.

It demonstrates:

- `hybrid-module` layout
- a shared Wio module target
- host-side C++ build from the same manifest
- `@Command`, `@Event`, and module lifecycle hooks
- direct `wio project build` / `wio project run`

## Run

```powershell
wio project describe --project .\examples\hybrid_module
wio project build --project .\examples\hybrid_module
wio project run --project .\examples\hybrid_module
```

Expected output:

```text
Hybrid project: before=11 afterAdd=16 afterTick=18
```

