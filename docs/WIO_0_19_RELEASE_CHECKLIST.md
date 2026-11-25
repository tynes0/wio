# Wio 0.19.0 Release Checklist

Preparation date: 2026-11-25. Candidate branch: `releases/0.19.0`.
This is a release checkpoint, not a new optimization sprint or v1 sign-off.

## Scope

- Preserve the user's dirty playground, Pacman and std work outside this clean
  candidate. They are not implicitly part of the release.
- Include committed compiler/runtime/VM/benchmark work since 0.18.1.
- Fix KI-007 operand materialization, KI-009 indirect native dispatch and KI-010
  VM global initialization with positive regressions, not tolerated failures.
- Align product metadata, SDK header, installer, VS Code package and web docs.
- Keep C++ as the default backend and ABI descriptor 11 unchanged.
- Keep N04 onward and formal timing budgets open; do not advertise optimization
  completion or full v1 SDK-kit parity.

## Acceptance gates

- [x] Clean Windows Release compiler/VM/self-hosted CLI/benchmark build.
- [x] Local `wio_release_019_semantics`: native O0/O2 + full/pruned VM producers;
  foreign-thread global callback uses the separate pruned-only case (KI-011).
- [x] Local `wio_vm_native_bridge`: function entries and global native resource cleanup.
- [x] Local `wio_vm_global_initialization`: lazy/once/concurrent/reentrant/failure rules.
- [x] Local N02 concat/append, N03 formatting-call and N04 byte-borrow gates.
- [x] SDK version contract and generated diagnostic reference freshness (694 IDs).
- [x] Windows PATH lookup in Run/Capture/Spawn and Unicode Spawn working directory;
  the updated process regression fails before the fix and passes after it.
- [ ] Three-platform candidate CI (Windows, Linux, macOS), recorded by run URL.
- [ ] Review KI-011's full C++ DLL loader-startup limitation before publication;
  general post-load global construction is not implemented by this candidate.
- [x] Clean staged-package smoke outside the source/build tree: 60 std modules
  independently/together, external project build/run/test/package and native interop.
- [x] VS Code checks, 32 unit tests, nine compiler integration tests and VSIX package.
- [x] Web catalog/version checks, all 56 source paths and production build.
- [ ] Package hashes and exact candidate commit recorded before publication.
- [ ] PRs merged, release assets uploaded, then website recommendation updated.

The checklist is intentionally not auto-completed by a version-number change.
Failed or cancelled CI is not green qualification. Publishing must wait for
the required gates or an explicit, documented scope decision.

## Preparation evidence

The final Wio gate passed in 142.58 seconds with the distinguishing comparison
snapshot and the separate foreign-thread initializer callback. All eight
selected correctness groups passed in their final scoped runs (not a full
historical compiler suite). The initial full-DLL callback failure is retained
and described below, rather than counted as a pass.

VS Code local checks, 32 unit tests and all nine compiler integration tests pass
against the clean 0.19 compiler/companion. VSIX 0.19.0 was generated. Its Windows/Linux
[CI run](https://github.com/tynes0/wio-vscode/actions/runs/45660002188) is green.
Draft companion PRs: [VS Code #10](https://github.com/tynes0/wio-vscode/pull/10),
[web #7](https://github.com/tynes0/wio-web/pull/7). The web catalog must not deploy
before the new compiler documentation is on main.

The first foreign-thread initializer rerun exposed KI-011 in the **full C++
producer's DLL static initialization**, and was stopped with a recorded failure.
The targeted callback fixture now tests native executables and the pruned VM
producer; pure globals continue to test both producer modes. This is explicit
scope separation, not a fix or a passing observation for KI-011.

The package smoke initially failed launching bare `cmake` on Windows, exposing
KI-012. After its regression/fix and rebuilding both CLI and benchmark tool, the
maintained installed-package qualification passed from
`build/release-019-qualification/installed`, outside this candidate checkout.
No persistent user environment or existing Wio installation was changed.

The synthetic all-std external project uses its template's Debug configuration:
9,550,718 generated C++ bytes and a 58,650,915-byte executable in this local run.
This is functionality evidence, not a compile-time/size optimization success;
KI-004 and the planned heavy optimization passes remain open.

## First cross-platform CI triage

The new interim-release correctness group passed on Windows, Linux and macOS,
and the release sanitizer job passed. The complete workflows were nevertheless
red: Linux rejected an ownership-test callback inferred as `unsigned long long`
instead of `uint64_t`; newer MSVC instantiated the sequence adapter's discarded
fixed-array branch inside a dependent lambda; and an old string fast-path gate
still searched generated text for integer formatting now delegated to the
shared runtime header. These are not waived gates.

The callback now declares its exact ABI return type. Sequence materialization
selects vector/array construction outside the boundary lambda, preserving direct
fixed-array construction and native failure translation. The formatting gate
checks the actual generated-to-runtime delegation while keeping its execution
and allocation budgets unchanged. All four affected local tests pass; fresh CI
on the newer compiler/platform matrix remains required before publication.
