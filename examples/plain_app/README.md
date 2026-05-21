# Plain App

This is the smallest release-grade Wio application example.

It demonstrates:

- a handwritten `wio.makewio`
- a normal executable target
- the direct `wio project build` / `wio project run` flow
- an `Entry` function without an explicit `-> void`

## Files

- `wio.makewio`
- `wio/main.wio`

## Run

```powershell
wio project describe --project .\examples\plain_app
wio project build --project .\examples\plain_app
wio project run --project .\examples\plain_app
```

Expected output:

```text
Hello from the plain Wio app example.
```

