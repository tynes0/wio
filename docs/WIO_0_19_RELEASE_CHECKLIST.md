# Wio 0.19.0 Release Checklist

Preparation date: 2026-11-25. Candidate branch: `releases/0.19.0`.
This is a release checkpoint, not a new optimization sprint or v1 sign-off.
This file preserves preparation-time observations, including superseded
candidates and failed runs. Final CI, merge and publication decisions are
recorded in [release PR #18](https://github.com/tynes0/wio/pull/18) and
[GitHub Releases](https://github.com/tynes0/wio/releases); unchecked items below
must not be interpreted as live publication status after this snapshot.

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
- [x] Review KI-011's full C++ DLL loader-startup limitation before publication;
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

## Broad freeze triage

The backend closure workflow on candidate `3a38369` passed Windows Debug,
Ubuntu Release and macOS Release:
[backend CI](https://github.com/tynes0/wio/actions/runs/45688037886).
The general validation sanitizer job also passed. These do not replace the
broader release workflow, whose Ubuntu freeze selection passed 386/390 tests
and failed four; later matrix stages therefore did not run on that job.

Three failures were stale expectations: the lowering pass-order list omitted
`materialize-formatting-calls`, telemetry hard-coded the old product version,
and an object/component access test expected 17 although 15 + 2 + 7 is 24.
The corrected access fixture checks each operand as well as the total. The
same stale pass-order list in the canonical optimizer test is updated too.

The fourth failure was a real target-ABI code-generation defect (KI-013): two
different C++ spellings can denote the same container type. The new constrained
projection specialization preserves all Wio stable identities and adds actual
native/VM alias mutation coverage, not just a generated-text assertion.
The earlier local ZIP/setup artifacts predate this compiler fix and must be
regenerated and rehashed before publication. No failing CI is waived.

The container-alias reproducer fails with duplicate-specialization diagnostics
using the previous staged compiler, and passes native O0/O2 plus full/pruned VM
producers with the new compiler. The existing sequence, POD-sequence and
dictionary projection differential/host-validation gates also pass locally,
3/3 in 85.42 seconds. Their identity/expiry/invalid-projection checks are retained.
The final six-test local triage selection passes 6/6 in 252.95 seconds,
including all five release-specific Wio fixtures. An earlier local authoring
run used an invalid byte-literal suffix in the new fixture and failed; it is
retained in scratch evidence, not counted as a passing run.

Post-triage review also exercised an unqualified native C++ type literally named
`Container`. In candidate `25bb15d`, the new projection template parameter could
shadow that name: flat sequence access passed, but a dictionary-to-array-to-POD
reference failed with WVM1222. The generated parameter now uses a compiler-owned
identifier, and the container-alias fixture includes this exact nested native
name/alias mutation case. Packages built from `25bb15d` are superseded until this
follow-up is rebuilt and qualified; the failure is not waived as a native limit.
The extended container-alias case passes locally in native O0/O2 and both VM
producer modes after the follow-up; the isolated failing VM probe also passes.

The final nine-test local selection on implementation `433898e` passes 9/9 in
293.09 seconds, including the extended native name/alias case. Installed-package
qualification on `25bb15d` also passes all 60 std modules and the external
project lifecycle/native interop; that result is not relabeled as a later build.
On 2026-11-26 the project owner accepted publishing KI-011 as a documented
limitation with explicit post-load initialization, not as a fixed architecture.

## Remaining native/artifact preflight

The final release workflow on `9e24136` passed the 390-test freeze and the
earlier product matrices on Windows/Ubuntu, then failed one stale C++ backend
test that required the old switch dispatcher. Test-only `a9afd14` requires the
optimized direct CFG and executes the enum/Option/Result fixture (expected 13).
The production payload did not change in that commit.

Preflighting the following native groups locally exposed KI-014 (enum metadata
cycles during POD code generation) and KI-015 (nullable opaque copy-back codec
identity). Their three original failed differential cases pass after the fixes
in 48.43 seconds. A follow-up preserves logical place IDs for nullable POD-field
projections separately from payload codec IDs; its POD alias differential passes,
and a host test rejects conflicting codec identities for one place.

The SDK adapter negative fixture now constructs an actual CoroutineSuspend in
a synchronous function. Merely tagging Return with unused async metadata did
not express that invalid operation. No SDK signature rejection was relaxed.
The isolated four-test boundary selection passes 4/4 in 82.46 seconds, including
shared/static project-library repair and deployment. An earlier overlapping
preflight had failed its repair timestamp assertion; no CLI repair/cache code
or acceptance condition was changed, and the stable-input rerun passes.

Opaque callback payloads are not enabled by this correctness work. The new
nullable-opaque fixture keeps their existing VM WVM1225 rejection as a distinct
negative branch, while checking supported values/ref copy-back/function values
positively. It is not reported as positive C++/VM callback parity.
