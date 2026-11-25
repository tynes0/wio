# Wio 0.19.0 Interim Release

**Release preparation: not yet published.** This checkpoint brings the native
C++ backend, bytecode/VM work and runtime improvements since 0.18.1 into one
versioned package. It is not the completion of the native optimization plan,
the VM optimization pass, SDK-kit parity, or the v1 blocker list.

## Backend and runtime progress

- Typed WIR and canonical Lowered WIR feed the C++ and bytecode backends.
- Bytecode execution supports native bridge producers, standalone artifact
  packaging, project execution, and the qualified shared/static hosting paths.
- Ownership, callback, native resource, container copy-back and Unicode path
  handling have dedicated regression gates. See the
  [backend qualification record](WIO_BACKEND_CLOSURE_QUALIFICATION.md) for the
  exact supported shapes and platform evidence, rather than assuming universal
  C++/VM/SDK interchangeability.
- Native runtime work includes conservative scalar/place storage reduction,
  capacity-preserving string builders, concat/append reuse, numeric formatting,
  borrowed byte-array reads, range writes and ByteBuffer block operations.
- The Wio-written benchmark library and `wio-bench` controller retain per-run
  evidence, per-implementation sampling, correctness checks and memory probes.
  These tools do not make historical timing samples a new performance guarantee.

## Correctness fixes prepared for this release

### Windows process lookup and package CLI

`Run`, `Capture` and `Spawn` now resolve bare executable names through PATH
before passing an explicit application path to `CreateProcessW`. This repairs
`wio package` invoking a PATH-installed CMake. Explicit paths and argument
quoting remain unchanged; no implicit shell is introduced. `Spawn` also
validates UTF-8 working directories through a wide filesystem path, like the
other two APIs. The updated process regression failed before the fix and passes
with an executable in a PATH directory containing spaces, Unicode arguments
and working directory, captured output, and preserved exit codes.

### Operand snapshots across side effects (KI-007)

Value operands are evaluated left-to-right. A borrowed managed LHS is copied
before a later side-effecting RHS can replace its storage. For example:

```wio
fn Change(value: ref string) -> string
{
    value = "changed";
    return "tail";
}

fn Entry() -> i32
{
    mut value = "original";
    let answer = value + Change(ref value);
    return answer == "originaltail" ? 0 : 1;
}
```

The same rule applies to the tested string/text, field and indexed-element
cases. Pure RHS expressions do not acquire an unconditional snapshot copy.
This is value evaluation, not a promise that an explicit `ref` or `view`
argument stops observing its referent.
Compound assignment still evaluates a destination place: `value += Mutate(ref
value)` appends to that destination after the RHS, unlike taking a value
snapshot for binary `+`.

### Native function values in the VM (KI-009)

A native declaration used as a function value or callback now enters the same
validated `NativeInvoke` path as a direct native call. This preserves native
registration requirements, argument checks, counted strings, `ref`/`view`
adapters, translated failures and cleanup. It does not execute an unbound
external declaration as an unchecked VM frame.

### Global initialization in the VM (KI-010)

Globals initialize in module declaration order, once per machine, on its first
entry. Native libraries must be registered before that entry. Ordinary
concurrent host entries wait for initialization; synchronous initializer
reentry observes the state reached so far. An initializer failure is retained,
constructed globals are cleaned up, and later entries return that first failure
instead of retrying side effects. Recreate the machine after a failed load.

Global initialization does not topologically sort declarations, merge a C++
producer's independent static globals with VM storage, or add tracing cycle
collection. Do not rely on later resource globals being constructed early.
Thread-dependent global initializers must not run during Windows DLL loading:
full C++ producers still use static initialization under the loader lock
(KI-011). Use an explicit post-load lifecycle entry for those native modules.
The separate callback-in-global fixture qualifies native executables and the
pruned VM producer only; it does not hide that full-producer limitation.

## Version and ABI

Compiler, CLI, runtime, std, bundled SDK version headers, VS Code metadata and
documentation are prepared for **0.19.0**. Module ABI descriptor **11** and the
existing bytecode format remain unchanged by these three fixes. Product-version
alignment does not mean that every v1 SDK-kit adapter is complete.

The C++ backend remains the default. VM execution is explicit; this release
does not silently switch existing native projects to an interpreter.

## Qualification and remaining work

The new `wio_release_019_semantics` gate exercises native `-O0`/`-O2`, full and
pruned VM producers, indirect native calls, callback entry, exception handling,
operand snapshots and the `std::io` global-mode regression. Host tests cover
initialization concurrency/reentry, failure caching, bytecode validation and
exactly-once native global cleanup. Local and CI results are tracked separately
in the [release checklist](WIO_0_19_RELEASE_CHECKLIST.md).

The Windows installed-package gate passed with 60 public std modules checked
independently and together, an external project build/run/test/package lifecycle
and native interop. VS Code's 32 unit tests and nine compiler integration tests,
VSIX packaging and the web production build also passed locally. This is not
yet cross-platform candidate CI acceptance or a published release.

Open work remains in the [known-issues register](WIO_KNOWN_ISSUES.md), including
escaping local-borrow diagnostics, code/build scalability, formal performance
budgets, SDK-kit parity, IDE expansion and the std consistency pass. No
cross-language speed ratio or full-suite acceptance is inferred from the new
focused correctness gates.
