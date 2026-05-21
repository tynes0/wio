# Native App

This example shows the smallest practical `@Native` application.

It demonstrates:

- a Wio executable target
- `@Native`, `@CppHeader`, and `@CppName`
- native include and source layout under `native/`
- the direct `wio project run` path

## Run

```powershell
wio project describe --project .\examples\native_app
wio project build --project .\examples\native_app
wio project run --project .\examples\native_app
```

Expected output:

```text
Native multiply:
42
```

