# Wio 0.20.0 Release Notes

Wio 0.20.0 adds manifest-aware analysis without building a project, paired with
VS Code diagnostics that no longer trigger a native build on save. This is the
first small delivery in the 0.20.x usability/correctness family, not completion
of that family or the v1 roadmap.

## Project checks

```sh
wio project check
wio project check --project path/to/wio.makewio
wio project check --backend vm --config Release
```

The command analyzes the declared entry and its import closure, preserving
manifest module roots, native include directories and executable/library target.
Library targets do not require Entry; executable targets still do. Nested-folder
manifest discovery works as it does for project build/run.

Check does not generate C++/WIR/bytecode, compile or link native/host sources,
package a VM bridge, run the application, create output directories or change
build signatures. Missing native build outputs do not block Wio analysis.
Declared C++ header availability is checked, but C++ bodies, native template
instantiation, linker symbols and WIR/backend diagnostics require explicit build.
Unimported files and unsaved-buffer incremental analysis remain outside this
command's scope.

## VS Code and SDK compatibility

Use wio-vscode 0.20.0 with the 0.20.x CLI. Project checks and automatic save
diagnostics use `project check`; they never fall back to a full build on older
toolchains. Build/run/generated-C++ commands remain explicit operations.

Compiler/runtime/std/CLI/SDK/editor/documentation product metadata advances to
0.20.0. Module ABI descriptor remains 11. This version alignment does not add
new SDK value/async/reflection capabilities or close the separate v1 SDK gate.

## Qualification policy

On 2026-11-27 the project owner explicitly waived the full minor-transition
suite for this narrow release. Release acceptance uses affected CLI/editor
regressions, SDK version consistency and staged-package smoke checks. This is
a one-release exception, not a full-suite pass or a change to the standing
minor-release policy. Exact CI/package evidence is recorded in the published
GitHub release and its PR.

The initial Windows implementation qualification passed 19 CLI test methods,
13 editor command/planner tests and four actual-CLI editor check fixtures.
The registered CLI CTest gate also passed, and the current Pacman project
passed frontend check. These initial tests used a rebuilt CLI companion with
the unchanged 0.19 compiler; release-package checks use the rebuilt 0.20 binaries.

## Remaining limitations

- VM WVM1008 instruction-budget policy is unchanged; long-lived application
  execution remains a separate 0.20.x task.
- Pure VM projects still use the existing native bridge build path; bytecode-first
  build architecture and compiler-stage splitting remain upcoming work.
- Windows full native DLL producers must not start a thread and wait for it from
  a global initializer under the loader lock (KI-011). Use explicit post-load
  startup; the interim-release limitation remains open.
- Native/VM/compile-time optimization, Result-first std consolidation, legacy
  attribute retirement, IDE language service and full SDK parity are ongoing.

See [CLI reference](WIO_CLI_REFERENCE.md), [release plan](WIO_V1_RELEASE_PLAN.md),
[known issues](WIO_KNOWN_ISSUES.md) and [0.19 baseline notes](WIO_0_19_RELEASE_NOTES.md).
