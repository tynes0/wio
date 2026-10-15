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
  failed-rebuild preservation of prior exe/archive bytes, and temporary cleanup.
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

Implementation and local qualification are in progress. Remote matrix results
will be recorded here after the workflow runs. Older corpus results remain their
historical qualification, not a new full-corpus claim from this pass.
