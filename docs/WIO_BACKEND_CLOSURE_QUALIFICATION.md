# Functional Backend Closure Qualification

This is the final functional pass before the three heavy optimization passes.
Full SDK kit parity remains a separate v1 blocker. Performance budgets are not
claimed by these correctness gates; framework/bundle packaging, automatic
dependency discovery and live-generation pruning are outside the frozen native
distribution policy.

## New qualification gates

- `wio_backend_workspace`: private workspace reservation, ASCII external-tool
  paths even in Unicode cwd, collision isolation, literal file names and bounded
  cleanup that preserves unknown user tool outputs.
- `wio_backend_unicode_outputs`: native executable/shared/static outputs with
  Turkish/emoji paths and cwd; executable launch, VM bridge + executable package,
  native/VM floating conversion, failed-rebuild preservation of prior exe/archive
  bytes, and temporary cleanup.
- `wio_float_parse_portability`: forced C-locale fallback versus native general
  parsing, bounded views, embedded NUL, range errors, invalid/trailing syntax
  and both f32/f64 entry points.
- `wio_vm_platform_qualification`: runtime-only installation and relocated SDK
  prefix, shared/static native-dependent VM hosts, failed-open rollback and
  lifecycle/ownership checks, private DLL/SO/dylib dependencies, Unix DESTDIR,
  relocated final distribution, shared/static project CLI build/run/package,
  and launch after original sources/bytecode/SDK prefix are unavailable.

The focused CI workflow `backend-closure.yml` runs only these new gates, with
Windows Debug, Linux Release and macOS Release jobs. It retains CTest logs as
qualification evidence. Adding the workflow is not proof that a platform passed;
actual results must be recorded before the functional milestone is closed.

## Native output publication

The compiler reserves an exclusive ASCII-named workspace under cwd, using
relative ASCII tool output paths without changing cwd or rebasing user compiler
arguments. If cwd cannot be written, it tries a compatible temporary directory.
Known object/artifact files are cleaned on success/failure. Unknown side outputs
from user tool flags are deliberately not recursively deleted.

Both native linker and static archiver write into the private workspace. Native
filesystem APIs copy the result to an exclusive publication directory adjacent
to the destination. Atomic replacement then publishes from the destination
filesystem; incomplete tool output never overwrites a prior archive/image.
Publication cleanup owns only its reserved files/directories.

This fixes the observed Windows MinGW NI-011 output-path failure without relying
on NTFS 8.3 names, lossy ANSI conversion, or changing relative argument semantics.
The Windows Unicode output gate has passed locally. macOS raw shared output also
uses an explicit `@rpath/<final-filename>` install identity, not the temporary
workspace filename. Mach-O private-dependency fixtures use `@loader_path` lookup;
ELF fixtures use origin-relative RPATH.

## Qualification status

Windows Debug local qualification: the four new gates passed 4/4 in 78.31
seconds. The strengthened native/VM floating-conversion Unicode gate passed
again in 10.00 seconds. Diagnostic reference validation passed (693 IDs).

Remote matrix qualification is in progress. The first runs exposed and led to
fixes for GCC dependent-template syntax, missing system-libc++ floating
`from_chars`, Darwin `fwrite_unlocked`, and Darwin `timeval.tv_usec` width.
The runtime/VM now share a bounded, C-locale fallback where floating
`from_chars` is unavailable; supported native implementations keep their direct
allocation-free path. CI results will be recorded here after the fixed matrix
completes. Older corpus results remain their historical qualification, not a
new full-corpus claim from this pass.
