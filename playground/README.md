# Wio Playground

This is the repository's scratch Wio application. The launcher can run it using
the canonical C++ native backend or the bytecode VM, in Debug or Release.

- Edit `wio/main.wio` for Wio code.
- Add native declarations under `native/include`.
- Add native implementations under `native/src` and list new source files in
  `wio.makewio`.
- Select the `wio_playground` CMake target in Rider or Visual Studio and press
  Play to build and run the project through the current Wio compiler.

In Rider, put these flags in the `wio_playground` run configuration's **Program
arguments** field. In Visual Studio use the startup project's **Debugging >
Command Arguments**. No source edits are required to switch:

```text
--native --debug
--native --release
--vm --debug
--vm --release
```

No flags means native C++, with the application profile following the launcher's
CMake configuration (Debug/Release). `--cpp` aliases `--native`.
`--backend native|vm` and `--config Debug|Release` are also accepted.

The launcher prints the selected backend/profile and the separate compiler-host
configuration. A Debug compiler can generate an optimized Release application;
these are different things. Native Debug explicitly uses `-O0 -g -UNDEBUG`, and
Release `-O2 -DNDEBUG`, appended after manifest backend arguments. Merely setting
`build.config` is not a native compiler optimization flag in ordinary projects.
The generic CLI now also accepts repeatable `--backend-arg` overrides.

VM selection incrementally builds the **selected configuration's** `wio_vm_runner`
and packages that runtime image, not just an optimized native bridge around a
Debug VM. Multi-config CMake builds (Visual Studio) support either profile from
one launcher. Single-config Ninja/Make builds require a matching
`CMAKE_BUILD_TYPE` for VM mode; a mismatch fails explicitly. The compiler host is
not rebuilt in another configuration just to change application optimization.

Outputs are isolated under `.wio-build/playground/native-debug`, `native-release`,
`vm-debug`, and `vm-release`. The manifest's empty `outputs.directory` deliberately
follows `--build-dir`. A custom project's explicitly fixed output directory still
wins; clear it to get the same isolation. Generated output is not added to the
solution. Compiler-argument signatures invalidate cached artifacts when flags
change; a failed build cannot leave a valid success signature.

Additional launcher options:

```text
--help
--release --dry-run
--vm --release --dry-run
--native --release --rebuild
--native --release -- --application-flag "two words"
--project path/to/wio.makewio --native --debug
```

`--dry-run` only prints commands. Unknown/conflicting launcher flags fail;
everything after `--` is forwarded literally to the Wio application, including
spaces, flag-looking strings and empty arguments. Child processes use argument
vectors and the existing UTF-8 process transport, not shell concatenation.

VM safety limits are unchanged: the production runtime's default instruction
budget is 10 million. A large Mandelbrot or other long loop may hit that limit;
Release mode is not permission to remove it. The performance benchmark's larger
explicit diagnostic budget is separate. Debug native symbols are generated, but
this change does not promise a source-level Wio debugger or VM debug adapter.

Launcher tests use isolated scratch projects, not the current user playground
source or its output files.
