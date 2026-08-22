# Wio Troubleshooting

This guide collects the most common build, package, interop, and workflow
problems we expect users to hit.

It is written for practical debugging, not for formal specification language.

---

## 1. `wio` Is Not Found

Symptom:

```text
'wio' is not recognized as an internal or external command
```

What to do:

1. If you are in the source tree, use the repo-local executable first:

```powershell
wio --help
```

2. If you are using a package, print the environment setup commands:

```powershell
C:\Wio\bin\wio.exe env print --wio-root C:\Wio --shell powershell --add-path
```

3. To make it persistent:

```powershell
C:\Wio\bin\wio.exe env setup --wio-root C:\Wio --set-user --add-path
```

---

## 2. I Cannot Find The Generated `.wio.cpp`

This is usually expected.

By default:

- generated C++ is treated as an intermediate
- it is not meant to permanently clutter source directories

If you explicitly want to inspect it:

```powershell
wio .\tests\test1.wio --emit-cpp
```

The normal model is:

- default build: generated C++ cleaned up
- explicit inspection: `--emit-cpp`

---

## 3. `file run` Did Not Leave An `.exe` Beside My Source

Also expected.

Wio is compiled, but the single-file workflow intentionally uses hidden cache
locations instead of writing outputs beside the source file.

This keeps:

- `playground/`
- `scripts/wio/`
- ad hoc scratch folders

from filling up with backend artifacts.

---

## 4. `cpp::header(...)` Cannot Be Resolved

Symptom:

- backend compile says a header cannot be found
- `with native` import compiles semantically, but C++ compilation fails

What to check:

1. The header path is correct.
2. The include directory is present in:
   - `wio.makewio`
   - `--include-dir`
   - or the relevant project/native include settings
3. The header is public and meant to be used by generated backend code.

If the import is repo-local and ad hoc, test it with:

```powershell
wio file run .\tests\native\native_bridge.wio --include-dir .\tests\native --backend-arg .\tests\native\native_math.cpp
```

---

## 5. A Packaged Toolchain Builds In The Wrong Place

Packaged Wio intentionally prefers safe output/cache roots for single-file
workflows.

If you expected it to write inside the install directory, that is the wrong
mental model.

Normal packaged behavior:

- projects build in their own project/build roots
- single-file/package-adjacent workflows fall back to user-cache locations when
  needed

This avoids writing into read-only install roots such as protected directories.

---

## 6. A Wrapper Went Stale After Reload

This is expected for some SDK surfaces.

`HotReloadModule` keeps top-level callable bindings reload-friendly, but
generation-bound wrappers must be reacquired:

- `WioObjectType`
- `WioComponentType`
- `WioObject`
- `WioComponent`
- `WioFieldAccessor`
- bound instance methods

If you see a stale-binding error, reacquire the wrapper from the current module
generation after reload.

---

## 7. A Generic Pack Or `std::meta` Program Fails In A Confusing Way

Start by checking whether you are inside the current `v1` slice.

Supported `v1` pack/meta directions include:

- pack indexing with integer literals
- same-pack `.size - N`
- top-level `const` integer declarations in pack index positions
- simple compile-time integer expressions over those constants
- `std::meta` helpers such as `Second`, `Penultimate`, `SecondValue`,
  `PenultimateValue`, and the current `Values<...>` helpers

Still outside the current `v1` scope:

- broad non-type generic programming like `Vector<T, N>`
- richer transforms such as `Take`, `Drop`, `Zip`, `MapTypes`

If the code is supposed to be inside the `v1` slice, compare it against:

- [`WIO_LANGUAGE_DRAFT.md`](./WIO_LANGUAGE_DRAFT.md)
- [`WIO_V1_FREEZE.md`](./WIO_V1_FREEZE.md)
- [`WIO_STD.md`](./WIO_STD.md)

---

## 8. Generic Native Calls Compile Semantically But Fail In Backend C++

This usually means the source-level generic shape is fine, but the bridge layer
or provided native symbol does not match the generated C++ call.

Good first checks:

1. Is the `cpp::name(...)` symbol actually a template or ordinary function?
2. Does the native function expect explicit template arguments or normal
   argument deduction?
3. Does the declaration-only Wio signature match the native callable shape?
4. If needed, regenerate with explicit C++ emission and inspect the wrapper:

```powershell
wio .\tests\some_case.wio --emit-cpp
```

---

## 9. `Result` Calls Feel Noisy Or Unexpected

The intended `v1` model is:

- canonical fallible APIs return `std::Result<T>`
- `Foo!()` unwraps and fails hard
- `Foo?()` unwraps and propagates

So if you see both `ReadAll()` and `ReadAllResult()` in older code, treat
`ReadAll()` as the canonical direction and the suffixed name as compatibility
surface.

---

## 10. I Want To Know If Something Is Stable

Read in this order:

1. [`WIO_V1_FREEZE.md`](./WIO_V1_FREEZE.md)
2. [`WIO_COMPATIBILITY.md`](./WIO_COMPATIBILITY.md)
3. the specific module/reference doc:
   - [`WIO_STD.md`](./WIO_STD.md)
   - [`WIO_SDK.md`](./WIO_SDK.md)
   - [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)

That gives you:

- whether it belongs to `v1`
- whether it is stable with caveats
- whether it is still experimental/post-`v1`

---

## 11. Windows Build Or Rebuild Feels Stuck

On Windows, an executable can remain locked if it is still running or was
recently launched by another process.

Practical fixes:

- close running `wio.exe` instances
- avoid keeping the generated program open while rebuilding it
- use side build directories when isolating a risky compiler change

This is one reason repo work sometimes uses alternate build folders for focused
experiments.

---

## 12. Where To Escalate Next

If the problem is mainly:

- command usage or manifests:
  [`WIO_CLI_REFERENCE.md`](./WIO_CLI_REFERENCE.md)
- project/package/install layout:
  [`WIO_PROJECT_SYSTEM.md`](./WIO_PROJECT_SYSTEM.md)
- native interop:
  [`WIO_INTEROP_GUIDE.md`](./WIO_INTEROP_GUIDE.md)
- host SDK:
  [`WIO_SDK.md`](./WIO_SDK.md)
- language or pack/meta semantics:
  [`WIO_LANGUAGE_DRAFT.md`](./WIO_LANGUAGE_DRAFT.md)
