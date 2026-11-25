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

- [ ] Clean Windows Release compiler/VM/self-hosted CLI/benchmark build.
- [ ] `wio_release_019_semantics`: native O0/O2 + full/pruned VM producers.
- [x] Local `wio_vm_native_bridge`: function entries and global native resource cleanup.
- [x] Local `wio_vm_global_initialization`: lazy/once/concurrent/reentrant/failure rules.
- [ ] Affected N02 concat and N03 formatting-call regression gates.
- [ ] SDK version contract and generated diagnostic reference freshness.
- [ ] Three-platform candidate CI (Windows, Linux, macOS), recorded by run URL.
- [ ] Review KI-011's full C++ DLL loader-startup limitation before publication;
  general post-load global construction is not implemented by this candidate.
- [ ] Clean staged-package smoke outside the source/build tree.
- [ ] VS Code checks, tests, compiler integration and VSIX package.
- [x] Web catalog/version checks, all 56 source paths and production build.
- [ ] Package hashes and exact candidate commit recorded before publication.
- [ ] PRs merged, release assets uploaded, then website recommendation updated.

The checklist is intentionally not auto-completed by a version-number change.
Failed or cancelled CI is not green qualification. Publishing must wait for
the required gates or an explicit, documented scope decision.

## Preparation evidence

The first new Wio gate passed all native O0/O2 and full/pruned VM variants.
The final rerun adds a distinguishing comparison snapshot and a foreign-thread
initializer callback; it must pass before that checkbox is completed.

VS Code local checks and 32 unit tests pass, with nine compiler integration
tests pending the fresh companion. VSIX 0.19.0 was generated. Its Windows/Linux
[CI run](https://github.com/tynes0/wio-vscode/actions/runs/45660002188) is green.
Draft companion PRs: [VS Code #10](https://github.com/tynes0/wio-vscode/pull/10),
[web #7](https://github.com/tynes0/wio-web/pull/7). The web catalog must not deploy
before the new compiler documentation is on main.

The first foreign-thread initializer rerun exposed KI-011 in the **full C++
producer's DLL static initialization**, and was stopped with a recorded failure.
The targeted callback fixture now tests native executables and the pruned VM
producer; pure globals continue to test both producer modes. This is explicit
scope separation, not a fix or a passing observation for KI-011.
