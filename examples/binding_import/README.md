# Binding Import

This example is the release-grade reference for bridge generation.

It demonstrates both supported entrypoints:

- `wio bind import` from a real C/C++ header
- `wio bind new` from a declarative manifest

## Files

- `binding_import_example.h`
- `binding_manifest.json`

## Generate From The Header

```powershell
wio bind import --header .\examples\binding_import\binding_import_example.h --realm binding_import_example --output .\examples\binding_import\binding_import_example.wio
```

## Generate From The Manifest

```powershell
wio bind new --manifest .\examples\binding_import\binding_manifest.json --output .\examples\binding_import\binding_manifest_example.wio
```

## Validate The Generated Binding

```powershell
wio file check .\examples\binding_import\binding_import_example.wio --include-dir .\examples\binding_import
```

