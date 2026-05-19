# Wio Versioning And Compatibility Policy

This document defines the intended compatibility reading for Wio `v1.0.0`
and the releases that follow it.

It is intentionally short. The goal is to answer one practical question:

What can users safely rely on once we tag `v1.0.0`?

## 1. Surface Categories

Wio surfaces should be read in one of three categories.

### 1.1 Stable

Stable means:

- the feature is part of the published Wio contract,
- user code may rely on its broad meaning,
- and changes after `v1.0.0` should preserve source intent unless there is a
  serious bug or security issue.

Stable does not mean:

- every diagnostic string is frozen,
- every generated backend detail is frozen,
- or every corner-case bug is already gone.

### 1.2 Experimental

Experimental means:

- the feature may exist in tree,
- users may try it deliberately,
- but its shape may still change in source-visible ways.

Experimental features should be called out clearly in docs rather than being
silently treated as stable.

### 1.3 Deprecated

Deprecated means:

- the feature still exists for compatibility,
- migration should begin,
- and removal or stronger warnings may happen in a later release.

`wio.project.json` and the PowerShell wrapper layer are the main examples of
this compatibility class in the `v1` era.

## 2. What `v1.0.0` Freezes

The `v1.0.0` tag freezes:

- the intended meaning of the stable language surface,
- the intended meaning of the stable std surface,
- the intended meaning of the native bridge contract,
- the primary CLI and project-system model,
- and the current runtime/reference model described by the freeze snapshot.

It does not freeze:

- backend-generated C++ shape,
- internal runtime helper names,
- internal package layout details that are not documented as user-facing,
- or every current implementation quirk.

## 3. Source Compatibility Rule

For stable features after `v1.0.0`, the default rule should be:

- do not casually break working user source,
- do not silently redesign semantics under the same syntax,
- and prefer additive growth over replacement.

If we need to tighten behavior, the preferred order is:

1. improve diagnostics,
2. document the tighter rule,
3. preserve old behavior when it was clearly user-intended,
4. only break source when the old behavior was clearly incorrect or unsafe.

## 4. Project And Tooling Compatibility

The intended `v1` compatibility reading is:

- `wio` CLI is primary and stable,
- `wio.makewio` is primary and stable,
- source-based Wio tools under `scripts/wio/*.wio` are part of the intended
  workflow model,
- packaged install flows should preserve the documented `env print/setup`
  contract,
- compatibility PowerShell wrappers may remain, but they are not the primary
  surface we evolve around.

## 5. Migration Policy

When a stable surface truly needs a user-visible change after `v1.0.0`, prefer:

1. marking the old path as deprecated in docs,
2. keeping a compatibility path for at least one release cycle when practical,
3. providing an explicit migration note,
4. then removing it in a planned release instead of silently drifting.

## 6. Current `v1` Reading

For the `v1` cycle, use this simple reading:

- if a feature is in [`WIO_V1_FREEZE.md`](./WIO_V1_FREEZE.md) section 2 or 3,
  we should treat it as part of the intended compatibility contract,
- if a feature is listed as outside `v1`, it should not be relied on as a
  stable commitment yet,
- if a feature exists but is not documented clearly enough, documentation work
  is still part of finishing the contract.
