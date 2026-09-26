# Wio 0.18.0 Release Notes

Wio 0.18 freezes the Lowered-WIR C++ backend as the release path. The compiler
now resolves language semantics into Typed WIR and canonical Lowered WIR before
C++ emission, while the backend consumes those decisions without returning to
the AST.

## Backend parity and generics

- Generic functions, objects, components, interfaces, extensions, operators,
  constructors, const generics, parameter packs, and exported specializations
  retain stable concrete identities through lowering.
- Concrete generic component cleanup is recomputed after substitution, so
  trivial specializations remain trivial while owned fields and destructors
  still receive exactly-once cleanup.
- Concrete reflection layouts now preserve method metadata and refresh fields
  from the specialized type layout.
- Native PODs, callbacks, `opaque`, ref/view adapters, module exports, SDK call
  tables, application schedules, async tasks, and behavioral attributes share
  the same canonical ABI metadata.

## Ownership and runtime hardening

- Places preserve owned, borrowed, mutable, view, move, and object-handle
  semantics across returns, field/index projections, early exits, and cleanup.
- Async process operations retain their native process state until completion;
  the runtime exposes a focused reference-count probe used by deterministic
  ownership stress coverage.
- Fuzz subprocess timeouts terminate the complete compiler process tree on
  Windows and the complete process group on POSIX hosts.

## Compile-time hardening

Deep structural types receive stable generated aliases instead of repeatedly
expanding the same nested C++ template spelling. Canonical optimization also
removes an unused default-local ownership chain only when construction and
destruction are proven unobservable. The original 32-level array regression
now completes backend syntax validation in seconds instead of exhausting the
fuzz gate.

## Qualification

The release candidate passes the complete 816-test Windows qualification,
including:

- differential legacy/WIR behavior and clean-project comparisons;
- self-hosted CLI, packaged install, SDK, native ABI, reflection, hot reload,
  application, async, Unicode, standard-library, and diagnostic matrices;
- generic construction, cleanup, reflection, operator, and export regressions;
- deterministic process ownership stress and compiler pipeline fuzzing.

`--cpp-backend legacy` remains available in 0.18 as an explicit differential
oracle and rollback switch. It is never selected automatically. Removal belongs
to the post-release cleanup after the cross-platform release workflow is green.
