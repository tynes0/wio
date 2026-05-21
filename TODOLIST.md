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
2. [x] Freeze the language surface.
   The stable contract is now written across the freeze snapshot, language draft, std docs, runtime type model, and SDK docs; remaining work is reference-quality tightening rather than unsettled surface direction.
3. [x] Declare `makewio` as the official primary project format.
   `wio.makewio` is now documented as the primary manifest and `wio.project.json` is explicitly legacy / compatibility only.
4. [x] Make the CLI first-class and freeze its behavior.
   `build`, `test`, `file`, `project`, `bind`, `env`, and `package` are now the documented stable command surface, and CLI help smoke coverage is in place.
5. [x] Remove PowerShell from the primary path completely.
   `scripts/*.ps1` are now documented and maintained as compatibility wrappers rather than the main user path.
6. [x] Formalize the Wio-written tooling model.
   The split between core CLI commands and `scripts/wio/*.wio` source tools is now explicit in the docs.
7. [x] Finalize the `file run` and tool-script output/cache policy.
   Single-file and tool workflows now avoid source-adjacent outputs and use hidden project/repo caches or user-cache fallbacks.
8. [x] Finish the generated C++ cleanup policy.
   Ordinary compiles now treat generated `.wio.cpp` files as backend intermediates near the output root, while `--emit-cpp` remains the explicit opt-in escape hatch.
9. [x] Finalize cache/output policy for packaged builds.
   Packaged `wio` now prefers safe user-cache output roots for single-file workflows instead of writing under non-writable install directories.
10. [x] Seal packaging and installation UX.
    `wio package`, installer wrappers, `env print/setup`, `QUICKSTART.md`, and the `WIO_ROOT`, `WIO_HOME`, and `PATH` story now share one documented model with smoke coverage.
11. [x] Validate the cross-platform flow for real.
    The release validation matrix is now defined as a real Windows/Linux GitHub Actions workflow, and the covered smoke set includes `project`, `bind`, packaged `file run`, and `std::process`.
12. [x] Stabilize the std surface.
    The stable vs experimental boundary is now documented for `std::Result<T>`, `std::console`, `std::io`, `std::process`, `std::fs`, `std::path`, `std::reflect`, `std::traits`, and `std::meta`.
13. [x] Fully seal the `Result` model.
    `std::Result<T>`, `Foo!()`, and `Foo?()` are now documented as the official `v1` error-flow model together with the existing test matrix.
14. [x] Freeze the native interop contract.
    `@Native`, `@CppHeader`, `@CppName`, POD/native enum/flagset handling, export rules, and ABI-safe surface rules are now documented as the intended `v1` bridge contract.
15. [x] Make `any / Box / opaque` boundaries official.
    Boxing rules, foreign-handle rules, the `is/fit` matrix, and native/runtime representation are now documented as one stable boundary.
16. [x] Finalize mutable data ergonomics.
    `ref values[i]`, nested `ref`, dict mutation, container mutability, and value-context auto-read are now documented as the intended `v1` behavior.
17. [x] Make enum/flagset fully first-class.
    Native support, const compatibility, helper APIs, and everyday state/kind/mode usage should be fully closed.
18. [x] Expand the tooling test suite.
    CLI smoke, package smoke, binding smoke, project smoke, Wio tool smoke, and packaged smoke now all have release-level coverage.
19. [x] Raise the docs to release quality.
    README, project system, language draft/spec, runtime type model, std, sdk, and traceability docs should tell one coherent story.
    `WIO_SDK.md` especially needs a full refresh so the current host/runtime/export surface, `WioEnum` / `WioFlagset`, field-kind coverage, and stale-wrapper / reload expectations are described completely instead of partially.
20. [x] Raise the example project set to release level.
    The release-facing examples now include plain app, native app, hybrid module, binding import, packaged quickstart, static CMake consumer, and the heavier hybrid arena companion sample.
21. [x] Write versioning and compatibility policy.
    Stable / experimental / deprecated boundaries and the post-`1.0.0` contract are now written down explicitly.
22. [x] Freeze the `v1`-scoped `const generics` / `std::meta` wave 3 slice.
    `v1` now includes the narrow compile-time index slice used by packs and `std::meta`: top-level `const` integer declarations in pack index positions, simple compile-time integer expressions over those constants, and the current `std::meta` wave 3 helpers such as `Second`, `Penultimate`, `SecondValue`, `PenultimateValue`, and the matching `Values<...>` helpers.
23. [x] Add a performance and memory story note.
    The intended `v1` cost model for `view/ref/value`, `Box`, `any`, containers, and native passing is now documented.
24. [x] Add first-class enum/flagset support on the SDK side.
    `WioObject` / `WioComponent` should preserve reflection identity through `WioEnum` and `WioFlagset`.
25. [x] Finish the normal Wio reflection API for enum/flagset.
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
5. [ ] Expand const generics beyond the v1-scoped pack/meta slice.
   This includes broader non-type generic use cases such as `Vector<T, N>`, arbitrary value parameters, and richer compile-time integer substitution outside the current pack-index model.
6. [ ] Continue beyond the v1 `std::meta` wave 3 slice with richer compile-time type/value tooling.
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
