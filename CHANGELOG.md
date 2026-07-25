# Changelog

All notable user-facing changes to Wio are recorded here.

## [Unreleased]

## [0.3.0] - 2026-07-25

### Added

- Added `std::hash` with default FNV-1a hashing and SHA-256 hex/byte digests.
- Added deterministic MT19937, xoroshiro128+, LXM, and Wichmann-Hill random
  generators, with MT19937 as `std::random::Random`.
- Added `ByteBuffer`, generation-checked `BytePool`, typed `Pool<T>`, and active
  `byte`/`bit` language aliases.
- Added queue, ordered/unordered set, heterogeneous tuple, regex, time,
  view-backed span-range, and explicit sorting modules.
- Array `Sort`/`Sorted` now use adaptive sorted/reversed/small/dense-integral
  paths before the general comparison-sort fallback.
- Expanded `std::traits` with category queries and constraints, including
  user-defined nominal generic-interface predicates in `@Apply`.
- Expanded reflection from enums/flagsets to component/object/interface type,
  field, method, access, base, size, and alignment metadata.
- Added the `std::convert` module with checked `Result<T>` parsers, `TryTo*`
  output-reference helpers, direct conversions, base-2-through-36 integer
  parsing/formatting, prefix auto-detection, and structured parse errors.
- Added `string.ToI8()` through `string.ToUSize()`, floating-point and boolean
  conversion members, plus generic `std::ToString<T>`/`convert::ToString<T>`
  formatting for standard values, containers, enums, and Wio objects with a
  `ToString()` method.
- Added `std::chars`, ASCII-aware string comparison/whitespace/composition
  helpers, and a generic algorithm utility wave covering distinct/filtering,
  chunking/windowing, flattening, ranges, repetition, sequence equality, and
  sums.
- Bare project actions discover manifests from the current directory and its
  ancestors, so `wio project describe`, `wio project build`, and
  `wio project run` work without `--project`.
- `wio run` is a shorthand for project execution.
- `wio help <command> [subcommand]` routes to contextual help across all CLI
  command families.
- Project execution accepts application arguments after `--`, repeated
  `--arg` values, `--no-manifest-args`, `--no-build`, `--cwd`, and
  `--print-command`.
- Command-specific `--version` and clearer group-specific help are available
  across the CLI.
- Bare command groups show successful help, and close subcommand misspellings
  receive suggestions for file, project, binding, environment, performance,
  and developer commands.

### Fixed

- File and project runners preserve spaces, flag-like values, shell-special
  characters, literal `--` values, and the child application's exact exit
  code.
- Project, package, and performance subprocesses use one direct cross-platform
  launcher instead of shell command strings.
- Empty command parsing, `--` positional parsing, parser reuse, built-in
  help/version handling, and conflicting argument declarations are hardened in
  Argonaut.
- Unknown top-level commands report a targeted error and suggest close command
  names instead of falling through to file compilation.

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
