# Wio Attribute System Plan

Status: accepted design direction for Wio 0.15.

This document supersedes the earlier postfix `with` attribute proposal. The
canonical source spelling is a declaration-leading bracket list:

```wio
[Export]
[CppName("CreateWidget")]
fn CreateWidget() -> i32 {
    return 0;
}
```

The goal is not merely to replace `@`. Attributes are typed language values
that may provide metadata, validation, controlled generation, and bounded
callee-side behavior while remaining deterministic, inspectable, and ABI-safe.

## 1. Application syntax

One attribute application is written as `[Name]` or `[Name(arguments)]`.
Qualified user/library names are ordinary realm paths:

```wio
[http::Route(method: HttpMethod::get, path: "/users/{id}")]
[security::Authorize(Role::admin)]
fn GetUser(id: u64) -> std::Result<User> {
    // ...
}
```

Applications may be stacked or grouped. Both forms have the same metadata
meaning; behavioral ordering never depends silently on source-list order:

```wio
[Export]
[CppName("AuditScore")]
fn AuditScore() -> f64;

[Export, CppName("AuditScore")]
fn AuditScoreCompact() -> f64;
```

The bracket list appears immediately before its target. Supported targets
include modules, imports/scopes, types, functions, methods, constructors,
fields, parameters, generic parameters, enum cases, extension members,
application handlers, and generated declarations:

```wio
[Derive(Json, Hash)]
component User {
    [JsonName("displayName")]
    displayName: string;

    [Secret]
    passwordHash: string;
}

fn Connect(
    host: string,
    [Range(1, 65535)] port: i32
) -> std::Result<Connection>;

enum NetworkMode {
    Offline,
    [Deprecated("Use Online instead")]
    Legacy,
    Online
}
```

Lexical activation deliberately keeps the existing `using` grammar. Brackets
mean "attach to this target"; `using` means "activate in this lexical scope".
`use` remains exclusively a module-import construct:

```wio
using cpp::header("raylib.h");

[Native, CppName("Vector2")]
component Vector2 {
    x: f32;
    y: f32;
}
```

A bounded activation block is also allowed for attributes that opt into
scoping:

```wio
using cpp::header("raylib.h") {
    [Native, CppName("Vector2")]
    component Vector2 {
        x: f32;
        y: f32;
    }

    [Native]
    fn DrawText(...);
}
```

## 2. User-defined attributes

`attribute` is the only new declaration keyword required by the model.
Uncommon policies are themselves ordinary, namespaced meta-attributes rather
than an expanding set of contextual grammar words:

```wio
[attribute::Targets(fn, method)]
[attribute::Runtime]
[attribute::Repeatable]
attribute Route(
    method: HttpMethod,
    path: string = "/"
);
```

Applications accept positional or named arguments. Positional arguments may
not follow a named argument. Defaults are folded and materialized before
validation/reflection, so tooling sees the complete effective value.

Safe defaults are:

- compile-time retention;
- one application per target;
- no inheritance;
- no lexical propagation;
- metadata-only behavior;
- no hidden allocation, blocking, I/O, unsafe access, or thread switch.

The core meta-attribute vocabulary is intentionally small:

- `attribute::Targets(...)` narrows valid targets;
- `attribute::Source`, `attribute::Compile`, or `attribute::Runtime` selects
  retention;
- `attribute::Repeatable`, `attribute::Inherited`, and `attribute::Scoped`
  opt into non-default propagation rules;
- `attribute::Conflict("group")` declares an exclusivity group;
- `attribute::Processor(T)` attaches a typed validation, derive, or behavioral
  processor implemented through the standard processor interfaces.

These are library-visible attribute declarations with compiler-known contracts,
not additional parser keywords. Attribute arguments support folded primitive,
enum, `string`, `text`, type, and other explicitly structural compile-time
values. Unsupported runtime expressions are rejected at the application site.

## 3. Why attributes exist

Every attribute declares one or more explicit purposes. A purpose determines
which compiler capabilities it receives:

1. **Metadata** records typed information for compiler services, reflection,
   documentation, serializers, bindings, tests, editors, and host SDKs.
2. **Validation** inspects a typed target and emits structured diagnostics but
   cannot mutate source.
3. **Derive** creates declarations through a checked builder API. Generated
   declarations are type-checked, attributed to their source application, and
   visible to tooling.
4. **Behavior** wraps a callee through fixed typed interception points. It may
   guard entry, inspect/transform a successful result, run guaranteed cleanup,
   or implement a typed `around` interceptor.

An attribute receives only the capabilities required by its declared purpose.
A metadata attribute cannot open files or rewrite a body; a validator cannot
silently generate members; a derive cannot mutate an existing public
signature; a behavioral processor cannot alter overload resolution or argument
evaluation.

## 4. Attribute bodies and processors

Attribute declarations may contain ordinary Wio functions and may implement
standard processor interfaces. The processor interface, not a magic method
name, decides how the body participates:

```wio
[attribute::Targets(method)]
[attribute::Processor(CallbackMethodProcessor)]
attribute CallbackMethod;

object CallbackMethodProcessor : attribute::AroundProcessor {
    public fn Invoke<TSelf, TArgs, TResult>(
        call: ref attribute::Call<TSelf, TArgs, TResult>
    ) -> attribute::Flow<TResult>
    where TSelf: native::LivePeer
    {
        if (not call.Self().IsAlive()) {
            return call.Skip();
        }
        return call.Proceed();
    }
}

[CallbackMethod]
fn OnNativeEvent(event: view Event) {
    Handle(event);
}
```

The example syntax for the generic processor API remains subject to
implementation feedback, but the semantic rules are fixed:

- `Call` exposes only capabilities valid for the target: receiver, parameters,
  result/error state, attribute arguments, and `Proceed`;
- the original body is not exposed as mutable tokens or an arbitrary AST;
- `Proceed` is invoked at most once in the initial model;
- `Skip` is valid for `unit` functions, or when the attribute supplies a
  type-correct fallback; the compiler never invents a return value;
- processor code is specialized and type-checked in the target's generic,
  ownership, lifetime, async, and effect context;
- direct calls, callbacks, reflection, exported/native entry points, and
  function values all enter through the same callee-side contract;
- processor expansion is deterministic, cycle-checked, cacheable, and
  inspectable.

The initial behavioral layer contains four fixed capabilities:

- entry guard/precondition;
- successful-return postcondition/result mapping;
- guaranteed exit/finalization;
- typed `around` interception.

These are standard processor interfaces, not unrestricted macro hooks. Call-
site rewriting, token macros, arbitrary AST mutation, hidden overloads, and
silent public-signature changes are outside the system.

## 5. Ordering and composition

Attribute list order is metadata order only. Behavioral composition must be
declared by processor dependencies or an explicit pipeline:

```wio
[Pipeline(Authentication, Transaction, Audit)]
fn UpdateAccount(request: UpdateRequest) -> std::Result<Account> {
    // ...
}
```

The compiler rejects dependency cycles, ambiguous order, incompatible effects,
and conflicting attributes. A processor must declare allocation, blocking,
I/O, unsafe/native access, thread affinity, cancellation, and executor changes.
These effects appear in diagnostics, hover information, generated docs, and
reflection.

For async functions, invocation, coroutine start, successful completion,
failure, cancellation, and final cleanup are distinct phases. An attribute
must select supported phases explicitly; synchronous `finally` behavior is not
silently treated as cancellation-safe asynchronous cleanup.

## 6. Reflection, SDK, and tooling

The current `string[]` reflection surface is migration-only. It flattens an
application such as `[Route(method: Get, path: "/health")]` into display text,
which loses argument types, defaults, stable identity, origin, and processor
information. Wio 0.15 replaces that representation with two complementary
layers.

### 6.1 Typed queries

Code that knows the attribute type uses generic queries and receives the actual
normalized attribute value:

```wio
let type = reflect::Describe<User>();

if (type.Has<Serializable>()) {
    let serializable = type.Attribute<Serializable>().Value();
    console::PrintLine!(serializable.format);
}

let method = type.Method("GetUser").Value();
let route = method.Attribute<http::Route>().Value();
console::PrintLine!($"${route.method} ${route.path}");
```

The returned value contains all declared parameters after named arguments and
defaults have been normalized. There is no string parsing and a wrong query
type is rejected or returns `None`, depending on whether the target is known at
compile time.

### 6.2 Erased inspection

Editors, serializers, debuggers, generic frameworks, and hosts that do not know
the attribute type use descriptors:

```wio
component AttributeArgumentInfo {
    name: string;
    typeName: string;
    stableTypeId: u64;
    value: any;
    usedDefault: bool;
}

component AttributeInfo {
    name: string;
    stableTypeId: u64;
    retention: AttributeRetention;
    purpose: AttributePurpose;
    origin: AttributeOrigin;
    arguments: AttributeArgumentInfo[];
}
```

The exact storage type may use the existing checked dynamic-value machinery at
ABI boundaries, but its semantic requirements are fixed. `AttributeOrigin`
distinguishes direct, inherited, scoped, derived/generated, and compiler-
synthesized applications. Target descriptors exist for types, fields, methods,
parameters, return values, generic parameters, enum cases, and handlers.

Retention is enforced rather than merely reported:

- source-retained applications are available to source tooling only;
- compile-retained applications are available to validation/derive/behavioral
  processors and compiler inspection output but are not emitted as ordinary
  runtime user reflection;
- runtime-retained applications are emitted into module metadata and can be
  queried from Wio and the C++ SDK.

Behavior that survives lowering keeps a compact execution descriptor for stack
traces, diagnostics, and pipeline inspection even when its user metadata is
compile-retained. That descriptor does not expose arbitrary executable memory
or permit reflection to run a processor again.

### 6.3 Behavioral pipeline reflection

Methods expose their effective behavioral pipeline separately from metadata:

```wio
let method = reflect::Describe<Controller>()
    .Method("Update")
    .Value();

for (behavior in method.Behaviors()) {
    console::PrintLine!(
        $"${behavior.phase}: ${behavior.processorName}"
    );
}
```

Each entry records phase, stable processor identity, explicit order/dependency,
declared effects, source attribute, and whether it may skip/proceed/transform.
It is read-only metadata; reflection cannot reorder or mutate the compiled
pipeline.

### 6.4 C++ SDK view

The C++ SDK receives owned inspection snapshots analogous to `Module::inspect()`:

```cpp
auto type = module.load_object("Controller").info();
auto method = type.method("Update");

for (const auto& applied : method.attributes()) {
    std::cout << applied.name() << '\n';
    if (auto path = applied.argument("path"))
        std::cout << path->get_as<wio::sdk::WioText>();
}
```

Descriptors use stable IDs and checked typed/dynamic values; they never expose
Wio's internal C++ layout. Snapshots own names and argument values, while any
live target/invocation handle remains module-generation-bound and becomes stale
after reload.

Reflection and the host SDK therefore expose:

- stable attribute type identity and declaration information;
- declared and effective typed arguments;
- source/compile/runtime retention;
- target and propagation policy;
- processor purpose and declared effects;
- generated declarations and their source application;
- deterministic behavioral pipeline order and declared effects.

The compiler and editor provide:

- target-filtered completion inside `[...]`;
- signature help and named-argument completion;
- definition, references, rename, hover, and documentation;
- formatter support for stacked and grouped lists;
- an expansion/pipeline inspection command;
- compile-time diagnostics attributed to both processor code and application;
- `wio migrate attributes --check|--write` for source migration.

## 7. Compatibility and migration

Wio 0.15 parses the previous `@Name(...)` and postfix `with` forms as legacy
input while all formatters, generators, examples, SDK bindings, documentation,
and editor snippets emit `[Name(...)]`.

Migration examples:

- `@Native` or `with native` -> `[Native]`;
- declaration-level `@CppHeader("x.h")` or `with cpp::header("x.h")` ->
  `[CppHeader("x.h")]`;
- `use @CppHeader("x.h")` -> `using cpp::header("x.h");`; existing canonical
  `using cpp::header("x.h")` source does not migrate;
- `@CppName("Foo")` -> `[CppName("Foo")]`;
- `@Export` -> `[Export]`;
- `@Deprecated("...")` -> `[Deprecated("...")]`;
- `@From(Type)` -> `[From(Type)]` until conversion syntax is independently
  redesigned;
- `@MainThread` -> `[MainThread]`.

Legacy spelling receives an edition-aware warning after automated migration is
available and is removed only at an explicit language-edition boundary. ABI
identity is based on the normalized attribute declaration and arguments, not
the source spelling used during the compatibility window.

## 8. Delivery order and release gate

1. Parse `[Attribute]`, grouped lists, and all target positions while preserving
   legacy input and the existing scoped `using` grammar.
2. Normalize built-ins and user attributes into one typed AST/model.
3. Complete target, argument, default, repetition, inheritance, scope,
   conflict, retention, and constant-evaluation diagnostics.
4. Add validation and checked derive processors with deterministic expansion.
5. Add entry/exit/around behavioral processors, then freeze sync and async
   semantics with callback-liveness and contract examples.
6. Publish runtime reflection and ABI metadata plus C++ SDK inspection.
7. Migrate std, examples, tests, generated bindings, VS Code, and web docs;
   ship formatter/migration/inspection tooling.
8. Qualify Windows and Linux compiler/runtime/SDK matrices before release.

Wio 0.15 cannot close while built-in attributes use a privileged parallel
implementation, behavioral expansion is invisible to tooling, or ordinary
project code still needs legacy spelling.
