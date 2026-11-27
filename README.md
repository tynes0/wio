# Wio

[![release validation](https://github.com/tynes0/wio/actions/workflows/v1-validation.yml/badge.svg?branch=main)](https://github.com/tynes0/wio/actions/workflows/v1-validation.yml)
[Documentation](https://wio-web.vercel.app/) · [Downloads](https://github.com/tynes0/wio/releases) · [Language reference](docs/WIO_LANGUAGE_DRAFT.md) · [Examples](docs/WIO_EXAMPLES.md)

**A statically typed language for native applications, tools, and embedded scripting—with a direct bridge to C++.**

Wio combines value-oriented components, reference-counted objects, generics,
Unicode text, and async programming with two execution backends: generated C++
and verified bytecode. Write application logic in Wio, reuse native libraries,
or embed Wio modules in an existing C++ application.

> **Reading this README:** this is the product-oriented introduction to the
> intended Wio 1.0 surface, not an announcement that 1.0 has shipped. The release
> line covered here is **0.20.4**; see Downloads for published artifacts.
> Remaining 0.20.x family goals are not all completed by this first slice.
> Parenthetical **(v1 target; pending)** labels identify accepted work that is
> not yet complete. Examples use implemented syntax; performance figures are
> dated measurements, not promises about every program or the latest release.

## Contents

- [Why Wio?](#why-wio)
- [Install and run your first program](#install-and-run-your-first-program)
- [Language at a glance](#language-at-a-glance)
- [Native and VM backends](#native-and-vm-backends)
- [C++ integration](#c-integration)
- [Standard library](#standard-library)
- [Measured performance](#measured-performance)
- [Support and limitations](#support-and-limitations)
- [Build and contribute](#build-and-contribute)
- [Documentation](#documentation)

## Why Wio?

Wio is designed for projects where application-level ergonomics and native
integration need to coexist:

- **Keep the native code you already have.** Declare typed bindings to C/C++
  functions, POD components, opaque resources, and supported concrete templates.
  A C++ host can also load Wio modules through the SDK.
- **Separate data from shared identity.** Components provide value semantics;
  objects provide shared identity with intrusive reference counting.
  Extensions add operations to components without changing their data layout.
- **Make references and failure visible.** `view` is read-only access, `ref`
  permits mutation, and `Result<T>` supports explicit handling, propagation, or
  checked unwrapping.
- **Choose execution without redesigning the language.** Both backends consume
  the same analyzed, backend-neutral intermediate representation.
- **Use application-oriented building blocks.** Async functions, coroutines,
  attributes, reflection, and application/system lifecycles support more than
  command-line scripts.

Wio does not automatically import arbitrary C++ headers or turn native code into
sandboxed bytecode. Native ABI, ownership, and deployment contracts still matter.

## Install and run your first program

### Install a packaged toolchain

Download the archive or installer for your platform from
[GitHub Releases](https://github.com/tynes0/wio/releases). Use the installation
instructions included with that release; source builds and packaged installs
are both supported.

On Windows, an extracted package can register its toolchain for the current user:

```powershell
C:\Wio\bin\wio.exe env setup --wio-root C:\Wio --set-user --add-path
```

Open a new terminal, then inspect the installation:

```sh
wio --version
wio --help
wio env doctor
```

The native backend needs a compatible C++ toolchain. Packaged Windows releases
provide a portable GNU toolchain; check the selected release's prerequisites on
Linux and macOS. Running an already packaged pure-Wio VM application does not
require the Wio compiler or a C++ compiler.

### Hello, Wio

Save this as `hello.wio`:

```wio
use std::console as console;

fn Entry() -> i32
{
    console::PrintLine!("Hello, Wio!");
    return 0;
}
```

Check it, then compile and run it:

```sh
wio file check hello.wio
wio file run hello.wio
```

`PrintLine!` unwraps a successful Result and panics if printing fails.
For recoverable failures, handle the Result or propagate it from a Result-returning
function instead.

### Start a project

```sh
wio project new HelloWio --output-dir . --template wio-app
cd HelloWio
wio project describe
wio project check
wio project run
wio project run -- "two words" --verbose
```

`project check` requires Wio 0.20.0 or newer; it is not part of 0.19.0.

The primary manifest is `wio.makewio`. Project commands discover it in the current
directory or its ancestors. Arguments after `--` belong to your application;
the child process's exit code is preserved.

Use a project manifest for multi-file programs, native include directories,
sources, libraries, host applications, and packaging—not a growing collection
of single-file compiler flags.

See [Getting started](docs/WIO_GETTING_STARTED.md),
[CLI reference](docs/WIO_CLI_REFERENCE.md), and
[Project system](docs/WIO_PROJECT_SYSTEM.md).

## Language at a glance

### Inference, generics, and value-oriented components

`let` declares an immutable binding; `mut` permits reassignment. Generic functions
preserve concrete types, and component extensions distinguish reads from mutation.

```wio
use std::console as console;

component Counter
{
    public value: i32;
}

extension CounterOperations for Counter
{
    public view fn Read() -> i32
    {
        return self.value;
    }

    public ref fn Add(amount: i32)
    {
        self.value += amount;
    }
}

fn Identity<T>(value: T) -> T
{
    return value;
}

fn Entry() -> i32
{
    mut counter = Counter(Identity<i32>(40));
    counter.Add(2);
    console::PrintLine!($"count=${counter.Read()}");
    return 0;
}
```

Components have value semantics; eligible non-escaping storage can remain on the
stack. Objects have identity and intrusive reference-counted lifetime management.
`interface` provides behavioral contracts. Not every value is necessarily
stack-allocated, and reference counting alone does not collect ownership cycles.

Generics also cover objects, components, interfaces, aliases, constraints,
specialization, and supported const arguments. See the
[language reference](docs/WIO_LANGUAGE_DRAFT.md) for exact rules and boundaries.

### Unicode literals and interpolation

Use `string` for the byte-oriented string model and `text` for Unicode text.
A `u"…"` literal constructs text; `u$"…"` interpolates it.

```wio
use std::console as console;

const Greeting: text = u"Hello 🌍";

fn Entry() -> i32
{
    let count = 3;
    let message: text = u$"${Greeting} — ${count} messages";
    console::PrintLine!(message);
    return 0;
}
```

Unicode text is not a synonym for C++ `wchar_t*`. Encoding, scalar/grapheme
operations, normalization, and native conversions have explicit contracts.
See the [standard library](docs/WIO_STD.md) and
[Unicode text contract](docs/spec/WIO_LANGUAGE_SPEC_0_13.md).

### Result-based failure handling

A plain call returns the Result, `?()` propagates an error, and `!()` unwraps
success or panics. Propagation requires a compatible Result-returning context.

```wio
use std::console as console;
use std::result;

fn Wrap<T>(value: T) -> std::Result<T>
{
    return std::Ok<T>(value);
}

fn Answer() -> std::Result<i32>
{
    let value = Wrap?(42);
    return std::Ok<i32>(value);
}

fn Entry() -> i32
{
    let answer = Answer!();
    console::PrintLine!(answer);
    return 0;
}
```

`std::UnitResult` is the alias for a Result with no useful success payload.
`std::Option<T>` remains available for explicitly optional data.

**Result-first standard APIs** are the intended uniform contract
**(v1 target; pending across remaining std/intrinsic operations)**. Current
releases still contain Option-returning retrieval APIs and compatibility
`Try...`/Raw/Result variants; their removal is not implied by this example.

### Async functions and coroutines

```wio
use std::async as futures;
use std::console as console;

async fn AnswerLater() -> i32
{
    await futures::Sleep(10u64);
    return 42;
}

async fn Entry() -> i32
{
    let answer = await AnswerLater();
    console::PrintLine!(answer);
    return 0;
}
```

`await` suspends the current coroutine when its result is not ready; it does not
implicitly turn every operation into a background thread. The runtime also
provides explicit task start, cancellation, combinators, structured scopes, and
worker/main-executor operations. Blocking waits remain explicit. See the
[async model](docs/WIO_ASYNC_MODEL.md).

### Attributes, reflection, and application lifecycles

User attributes carry typed arguments and target/retention policies. This is a
declaration fragment to include in a program or library:

```wio
attribute Entity(name: string) for component retain runtime;

[Entity("players")]
component Player
{
    public score: i32;
}
```

Attributes also support composition and bounded validation, derive, and behavioral
processors—not unrestricted arbitrary compiler/AST mutation. Reflection exposes
supported type, field, method, generic, and attribute metadata.

Applications can use ordinary methods with lifecycle attributes instead of
writing an entry function and loop manually. This complete application exits
after three updates:

```wio
use std::console as console;

application Demo
{
    frames: i32;

    [Start]
    fn Boot()
    {
        console::PrintLine!("Starting");
    }

    [Update]
    fn Tick(delta: f64)
    {
        self.frames += 1;

        if (self.frames >= 3)
        {
            self.Exit(0);
        }
    }

    [Close]
    fn Shutdown()
    {
        console::PrintLine!("Closed");
    }
}
```

Systems, fixed stages, ordering, and main-thread affinity extend this model.
Attribute syntax in this README is exclusively `[Attribute]`; scoped
`using cpp::header(...)` is unchanged. Removal of legacy `@...` and postfix
attribute `with ...` acceptance is **(v1 target; pending)**.

See the [attribute contract](docs/WIO_ATTRIBUTE_SYSTEM_PLAN.md) and
[application/system model](docs/WIO_0_17_APPLICATION_MODEL.md).

## Native and VM backends

Both paths share semantic analysis, Typed WIR, and canonical Lowered WIR.
They do not run the same machine representation.

| | Native C++ backend | Bytecode / VM backend |
| --- | --- | --- |
| Execution | Generates C++20, then compiles and links machine code | Emits verified `.wiob` bytecode executed by Wio's interpreter |
| Project selection | Default; `--backend native` | `--backend vm` |
| Best fit | Native integration and optimized deployment | Bytecode execution, embedding, and a compiler-independent VM runner |
| Build dependency | Compatible C++ compiler/linker | Pure bytecode does not need C++ compilation; native bridge producers do |
| Outputs | Platform executable, shared library, static archive | Bytecode, embedded VM executable, and qualified shared/static Wio-module wrappers |
| Native libraries | Typed C/C++ bindings | Compiled native registry adapters; arbitrary C++ binaries are not directly interpreted |
| Runtime performance | Native code; optimization budgets still open | Non-JIT interpreter; separate heavy optimization pass still open |
| Debug/Release | Supported build configurations | Runner/runtime configurations; not a JIT optimization tier |

Inside a project:

```sh
wio project run --backend native
wio project run --backend vm
wio project package --backend vm --output-dir dist
```

A VM executable embeds the runner and bytecode; pure packages need no source
checkout or separate `.wiob` at runtime. Packages remain OS/architecture-specific.
Shared/static VM modules are native wrappers around VM execution, not
bytecode magically converted into optimized machine code.

Native dependencies must be declared and deployed explicitly. Full SDK value
coverage is separate from the currently qualified artifact paths
**(v1 target; pending)**. Native calls are privileged, not sandboxed.

See [C++ backend](docs/WIO_CPP_BACKEND.md),
[VM artifacts](docs/WIO_VM_ARTIFACTS.md), and
[VM native bridge](docs/WIO_VM_NATIVE_BRIDGE.md).

## C++ integration

### Call C++ from Wio

An existing header, `native_math.h`:

```cpp
#pragma once
#include <cstdint>

namespace native_math
{
    inline std::int32_t Multiply(std::int32_t lhs, std::int32_t rhs)
    {
        return lhs * rhs;
    }
}
```

A complete `bridge.wio`:

```wio
use std::console as console;

using cpp::header("native_math.h");

[Native, CppName(native_math::Multiply)]
fn Multiply(lhs: i32, rhs: i32) -> i32;

fn Entry() -> i32
{
    console::PrintLine!(Multiply(6, 7));
    return 0;
}
```

With both files in the same directory:

```sh
wio file run bridge.wio --include-dir .
```

For a real library, declare its headers, native sources, include paths, link
libraries, and runtime dependencies in the project manifest. Native POD
components and `opaque` handles cover data and resource bindings; extensions can
present C++ free functions as component operations. Templates require supported
concrete declarations/instantiations, not automatic translation of every C++
template into a Wio generic.

### Call Wio from C++

Save this library as `plugin.wio`:

```wio
[Export]
fn Add(lhs: i32, rhs: i32) -> i32
{
    return lhs + rhs;
}
```

For example, build a Windows shared module:

```sh
wio plugin.wio --target shared --output plugin.dll
```

A host using `sdk/include/wio_sdk.h` can load and invoke the logical export:

```cpp
#include <cstdint>
#include <iostream>
#include <wio_sdk.h>

int main()
{
    auto module = wio::sdk::Module::load("./plugin.dll");
    auto add = module.load_export<std::int32_t(std::int32_t, std::int32_t)>("Add");
    std::cout << add(20, 22) << '\n';
}
```

Use the corresponding shared-library filename and host build settings on
Linux/macOS. The SDK also provides commands/events, reflection, hot reload, and
qualified task/session hosting. Full parity for every stable v1 host-observable
value, ownership, async, application, and reload shape remains
**(v1 target; pending)**; do not assume an arbitrary nested signature crosses
every native or VM ABI today.

See [Interop guide](docs/WIO_INTEROP_GUIDE.md),
[Host SDK](docs/WIO_SDK.md), and
[SDK parity plan](docs/WIO_SDK_EVOLUTION_PLAN.md).

## Standard library

Wio's public library is largely readable `.wio` source under [std/](std), with
compiler intrinsics and native runtime helpers where necessary.

| Area | Representative modules / facilities |
| --- | --- |
| Everyday programming | Console, conversion, strings/chars/Unicode, builders, algorithms, collections |
| Data and storage | Byte buffers/pools, binary I/O, files, paths, JSON parsing/writing, serialization |
| Math and utilities | Numeric operations, hash (including FNV-1a/SHA-256), random generators, time, UUID, semver |
| Application work | Async/concurrency, processes, networking foundations, logging, regex, configuration |
| Measurement and introspection | Reflection, attributes, `std::benchmark`, `wio-bench` |

A module name is not a claim that every possible feature in that domain exists.
For example, networking foundations do not imply a complete TLS/HTTP/WebSocket
stack, and Unicode text does not imply full locale-sensitive collation.

API consolidation, removal of redundant helpers, consistent Result-based errors,
the agreed std formatting/access rules, and remaining performance work are
**(v1 target; pending)**. These improvements are coordinated with the native
optimization pass rather than treated as a second unrelated rewrite.

See [std reference](docs/WIO_STD.md),
[builtin method signatures](docs/WIO_BUILTIN_METHODS.md), and
[benchmark library/tool](tools/wio-bench/README.md).

## Measured performance

The following are **historical, reproducible optimization measurements**, not a
new benchmark of 0.19.0 or a prediction of 1.0 performance. Times are **medians**,
not best cases. Each comparison uses its own matched workload and session;
do not combine absolute times across sessions.

### Mandelbrot computation: scalar storage and direct control flow

800 × 800 pixels, maximum 100 iterations, double precision, no formatting or file
I/O inside the timed kernel. Windows build 26100, Intel Core i7-13620H, MinGW
GCC 15.2.0, C++20 `-O2 -ffp-contract=off -static`, no fast-math.
Fifteen interleaved measured rounds after warm-up; CPU/power affinity was not pinned.

| Implementation | Median kernel time | Observed range |
| --- | ---: | ---: |
| Original Wio generated at `ef3e53f` | 774.603 ms | 769.153–780.145 ms |
| Wio scalar-storage slice `722e836` | 23.163 ms | 22.548–24.583 ms |
| Equivalent handwritten C++ | 22.553 ms | 21.819–24.364 ms |

That slice is **33.44× faster than the original Wio output**, with **2.70% median
overhead versus C++ for this kernel only**. Every run verifies the same iteration,
weighted, and interior fingerprints.

A separate allocation-instrumented execution drops ordinary `new/new[]` calls
from **15,454,738 to 2**. This is an allocation-call count across entry/reporting,
not peak memory, allocated bytes, or instrumented kernel timing.

[Methodology, evidence, and reproduction](docs/WIO_NATIVE_SCALAR_OPTIMIZATION.md).

### Long-string interpolation: improvement with an open gap

Interpolating a 32-byte value and appending it 30,000 times produces the same
960,000 output bytes and hash. Fifteen interleaved measured rounds after warm-up,
on the same documented Windows/GCC setup:

| Implementation | Median | Ordinary allocation calls |
| --- | ---: | ---: |
| Previous Wio interpolation `61d43ef` | 4.016 ms | 60,020 |
| Wio string fast paths `d6cdbd3` | 3.241 ms | 30,020 |
| C++ direct-append reference | 0.576 ms | 17 |

Wio uses **19.30% less measured time** and halves allocation calls, but remains
**5.63× the direct-append reference**. The C++ reference constructs the same
bytes without Wio's intermediate value/call surface; it is a lower-bound
reference, not an identical algorithm.

[Methodology, limitations, and evidence](docs/WIO_NATIVE_STRING_FAST_PATHS.md).

These results do not establish universal C++ parity, whole-program memory
budgets, current VM speed, or C#/Python rankings. Native, non-JIT VM, and compiler
throughput have separate open acceptance gates **(v1 target; pending)**.
The native 5–10% overhead objective is a workload-dependent target, not a
measured language-wide guarantee.

## Support and limitations

| Surface | Available now | Before v1 |
| --- | --- | --- |
| Core language | Components/objects/interfaces, generics, references, containers, matching, interpolation, async | Remaining coherence/specification and conformance closure (v1 target; pending) |
| Attributes | Bracket syntax, user metadata, composition, bounded processors | Remove legacy attachment syntaxes and finish migration/tooling (v1 target; pending) |
| std errors | Result, propagation, checked unwrap, UnitResult, explicit Option | Uniform Result-first fallible APIs and removal of redundant families (v1 target; pending) |
| Backends | Native C++ and non-JIT VM; qualified executable/shared/static paths | Runtime, memory, generated-size and build-time budgets (v1 target; pending) |
| C++/SDK | Typed bindings, supported POD/opaque/callback paths, host loading/reflection/reload | Complete stable v1 SDK parity and boundary qualification (v1 target; pending) |
| Platforms | Focused Windows Debug, Linux Release, macOS Release backend gates | Full supported install/upgrade/toolchain/configuration matrix (v1 target; pending) |
| Editor support | VS Code integration; CMake playground/source targets | Compiler-owned language service and aligned Rider/CLion/VS clients, Rider first (v1 target; pending) |
| Developer tools | File/project checks and execution, manifests, build/package/env workflows | Deterministic formatter, baseline lint, unified structured diagnostics (v1 target; pending) |
| Distribution | Archives/installers and explicit native dependency packaging | Complete dependency/package workflow, reproducibility/provenance and clean-machine acceptance (v1 target; pending) |
| Documentation | Language/std/SDK guides, examples, builtin methods and issue/code references | Normative v1 consistency, versioned migration and real-project release gates (v1 target; pending) |

Important current boundaries:

- **No JIT today.** JIT, a full debugger, advanced refactoring, and Wio.NET are
  post-v1 work, not features promised by this README.
- **No automatic cycle collector.** Design owning object graphs accordingly;
  intrusive reference counting is not tracing garbage collection.
- **Native code is trusted.** Bytecode verification does not sandbox a native
  library, catch native undefined behavior, or make arbitrary pointers safe.
- **Interop is qualified by shape.** A supported value in Wio is not proof that
  every callback, borrow, nested SDK value, or VM bridge combination supports it.
  Consult the specific ABI contract and rejection diagnostics.
- **Windows DLL initialization:** thread-dependent global initialization can
  deadlock under the loader lock (KI-011). Start that work explicitly after
  loading; an interim-release limitation is not a v1 correctness sign-off.
- **Deployment is explicit.** Native dependencies are not discovered
  transitively, framework/bundle packaging is outside the current policy, and
  retained native generations are not automatically garbage-collected.
- **Platform tests have a scope.** A focused three-platform gate is not proof
  that every test, architecture, compiler, sanitizer, and configuration passed.

See [Known issues](docs/WIO_KNOWN_ISSUES.md),
[backend qualification](docs/WIO_BACKEND_CLOSURE_QUALIFICATION.md), and
[native distribution policy](docs/WIO_NATIVE_DISTRIBUTION_POLICY.md).
For release-specific fixes and caveats, use the selected release's notes rather
than assuming an older checkout describes a newer package.

## Build and contribute

A source build requires CMake and a supported C++20 toolchain:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`CMAKE_BUILD_TYPE` selects the configuration for single-config generators;
`--config` selects it for multi-config generators. The compiler executable is
under `build/app/`, with a configuration subdirectory where applicable.

The generated-program runtime has an independent profile:
`-DWIO_BACKEND_RUNTIME_PROFILE=Release|Debug|RelWithDebInfo|MinSizeRel`.
Release uses O2 without disabling runtime assertions or enabling fast-math.
Compiler-host configuration, runtime profile, and generated-program optimization
are separate settings—not interchangeable meanings of “Release.”

After bootstrap, the CLI also provides repository workflows:

```sh
wio build --build-dir build --config Release
wio test --build-dir build --config Release
wio package --build-dir build --config Release --output-dir artifacts/packages
```

For quick experiments, select the runnable `wio_playground` CMake target.
It launches the native project at `playground/wio.makewio`. Source-only solution
targets expose Wio tests/examples, std, CLI, CMake, and C/C++ headers/sources.
See the [playground guide](playground/README.md) for launcher settings.

Keep correctness, ownership, and diagnostics tests alongside changes. Benchmark
reports must identify the workload, compiler/runtime revisions, build settings,
sample counts, output checksums, and timing/memory scope.

## Documentation

| Start here | Go deeper |
| --- | --- |
| [Getting started](docs/WIO_GETTING_STARTED.md) | [Language reference](docs/WIO_LANGUAGE_DRAFT.md) |
| [CLI reference](docs/WIO_CLI_REFERENCE.md) | [Project and manifest system](docs/WIO_PROJECT_SYSTEM.md) |
| [Examples](docs/WIO_EXAMPLES.md) | [Interop](docs/WIO_INTEROP_GUIDE.md) and [Host SDK](docs/WIO_SDK.md) |
| [Standard library](docs/WIO_STD.md) | [Builtin methods](docs/WIO_BUILTIN_METHODS.md) and [Async model](docs/WIO_ASYNC_MODEL.md) |
| [Troubleshooting](docs/WIO_TROUBLESHOOTING.md) | [Error/status codes](docs/WIO_ERROR_CODES.md) |
| [V1 release plan](docs/WIO_V1_RELEASE_PLAN.md) | [Surface consolidation](docs/WIO_V1_SURFACE_CONSOLIDATION_PLAN.md) and [Post-v1 roadmap](docs/WIO_POST_V1_ROADMAP.md) |

The [documentation index](docs/README.md) routes the remaining specifications,
backend contracts, platform evidence, and design notes. Accepted plans are not
implementation claims; release notes and qualification records define what a
particular version actually ships.

## License

Wio is available under the [MIT License](LICENSE).
