# Changelog

All notable user-facing changes to Wio are recorded here.

## [0.2.0] - 2026-07-19

### Added

- Explicit full specializations for generic objects and components with
  `@Specialize(...)`.
- Runtime and semantic support for returning `self` as `view`/`ref`, and for
  returning an object value through `deref self`.
- Release tests for generic specialization, constructor deduction, object
  self returns, SDK method overloads, and packaged file execution.

### Fixed

- Generic object and component constructor bodies are validated after their
  type arguments have been inferred.
- SDK descriptor validation now accepts legitimate method overloads while
  still rejecting duplicate signatures.
- `i8` and `u8` interpolation is formatted numerically instead of as a
  character.
- Windows subprocesses preserve all PATH entries when duplicate `PATH`/`Path`
  environment keys are present.
- Package smoke discovery ignores neighboring installer files and the package
  quickstart documents persistent environment setup.
- Windows GNU-host builds use wide filesystem APIs, and the deleted nested std
  import regression fixture is restored.

### Release

- CLI and compiler versions now derive from the CMake project version.
- Package, installer, documentation, and example surfaces are aligned on
  `0.2.0`.

## [0.1.0-rc.1]

- Initial public release candidate.
