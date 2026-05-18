# Wio Todo List

This file is now the active and simplified roadmap.

- Completed work: `COMPLETED.md`
- `v1.0.0` closes first.
- Work that belongs after `v1` stays in the second section below.

Status markers:

- `[ ]` not started
- `[~]` partially landed; hardening, docs, tests, or polish still needed
- `[x]` done

## Work Required For Version 1.0.0

1. [x] Finish operator overloading.
   Member/free unary-binary-assignment operators, `fit`, `[]`, `()`, generic overloads, and the `deref` ergonomics pass are now in place.
2. [~] Freeze the language surface.
   A stable contract must be written for `component / object / interface / native / Result / any / Box / opaque / enum / flagset / reflection / pack`, and the generic surface should now move into freeze / polish mode.
3. [~] Declare `makewio` as the official primary project format.
   `wio.project.json` should remain clearly marked as legacy / compatibility only.
4. [~] Make the CLI first-class and freeze its behavior.
   `build`, `test`, `file`, `project`, `bind`, `env`, and `package` should be stabilized together with help, exit-code, and path behavior.
5. [~] Remove PowerShell from the primary path completely.
   `scripts/*.ps1` should exist only as compatibility wrappers.
6. [~] Formalize the Wio-written tooling model.
   The role split between core CLI commands and `scripts/wio/*.wio` source tools should be explicit.
7. [ ] Finalize the `file run` and tool-script output/cache policy.
   Single-file and tool workflows should not clutter source directories.
8. [ ] Finish the generated C++ cleanup policy.
   Generated backend files should be deleted by default and preserved only behind explicit flags.
9. [ ] Finalize cache/output policy for packaged builds.
   Packaged `wio` should use safe cache/output locations even in non-writable installs.
10. [~] Seal packaging and installation UX.
    `wio package`, installer wrappers, `env print/setup`, and the `WIO_ROOT`, `WIO_HOME`, and `PATH` story should reach release quality.
11. [ ] Validate the cross-platform flow for real.
    At minimum, Windows and Linux should both be covered across `project`, `bind`, `package`, `env`, and `std::process`.
12. [~] Stabilize the std surface.
    The stable vs experimental boundary should be clear for `std::Result<T>`, `std::console`, `std::io`, `std::process`, `std::fs`, `std::path`, `std::reflect`, `std::traits`, and `std::meta`.
13. [~] Fully seal the `Result` model.
    `std::Result<T>`, `Foo!()`, and `Foo?()` should be frozen as the official error model together with docs and tests.
14. [~] Freeze the native interop contract.
    `@Native`, `@CppHeader`, `@CppName`, POD/native enum/flagset handling, export rules, and ABI-safe surface rules should be documented clearly.
15. [~] Make `any / Box / opaque` boundaries official.
    Boxing rules, foreign-handle rules, the `is/fit` matrix, and native representation should be stabilized.
16. [~] Finalize mutable data ergonomics.
    `ref values[i]`, nested `ref`, dict mutation, and container mutability rules should stop surprising users.
17. [~] Make enum/flagset fully first-class.
    Native support, const compatibility, helper APIs, and everyday state/kind/mode usage should be fully closed.
18. [ ] Expand the tooling test suite.
    CLI smoke, package smoke, binding smoke, project smoke, Wio tool smoke, and packaged smoke should all have release-level coverage.
19. [ ] Raise the docs to release quality.
    README, project system, language draft/spec, runtime type model, std, sdk, and traceability docs should tell one coherent story.
20. [ ] Raise the example project set to release level.
    Plain app, native app, hybrid module, host interop, binding import, and packaged quickstart examples should all be polished.
21. [ ] Write versioning and compatibility policy.
    Stable / experimental / deprecated boundaries and the post-`1.0.0` contract should be explicit.
22. [ ] Make the `v1` decision for `const generics` / `std::meta` wave 3.
    If they are not part of `v1`, that should be stated clearly; if they are, their scope must be frozen.
23. [ ] Add a performance and memory story note.
    Users should understand the cost model for `view/ref/value`, `Box`, `any`, containers, and native passing.
24. [ ] Add first-class enum/flagset support on the SDK side.
    `WioObject` / `WioComponent` should preserve reflection identity through `WioEnum` and `WioFlagset`.
25. [ ] Finish the normal Wio reflection API for enum/flagset.
    `name`, `value`, `underlying_type`, `size`, and `index` support should become stable.

## Remaining Work After Version 1

This section collects the larger backlog items that belong after `v1`.

1. [ ] Turn the draft reference into a real, versioned language spec.
   Formal grammar, syntax, type system, and feature status markers should be completed.
2. [ ] Design a stronger nullability model.
   `null`, `ref`, `view`, object/reference, and container interactions may need to become stricter.
3. [ ] Implement or formally settle generic defaults and partial specialization.
4. [ ] Expand the variadic/pack metaprogramming surface.
   Add deeper compile-time transforms such as `Take`, `Drop`, `Zip`, and `MapTypes`.
5. [ ] Implement `const generics` for real.
   Open up non-type generic use cases such as `Vector<T, N>`.
6. [ ] Continue with `std::meta` wave 3 and richer compile-time type/value tooling.
7. [ ] Design the concurrency model.
   This includes `async`, `await`, `yield`, `thread`, and scheduler/host interaction.
8. [ ] Evaluate time/game scheduling keywords.
   `every`, `after`, `during`, and `wait` should be tied clearly either to the core language or to a std DSL.
9. [ ] Evaluate pipeline/data-flow operators.
   The real value and semantic cost of `|>` and `<|` should be measured.
10. [ ] Clarify higher-level `system` and `program` abstractions.
11. [ ] Design the broader reflection/runtime metadata model beyond enum/flagset.
12. [ ] Grow the std surface after `v1`.
    Planned additions include `std::json`, `std::http`, `std::time`, `std::random`, `std::hash`, `std::log`, `std::bytes`, `Buffer<T>`, and related modules.
13. [ ] Expand the editor/LSP/formatter/tooling ecosystem.
14. [ ] Grow the Wio-written tooling side further and remove the remaining compatibility wrappers.
15. [ ] Complete deeper backend portability and performance benchmarking work.
