# Wio Attribute System Plan

Status: accepted design direction for Wio 0.15; foundation implementation is
in progress on `releases/v0.15.0`.

Implemented foundation checkpoint:

- declaration-leading stacked/grouped bracket applications;
- preserved application/system/handler/parameter/enum-case target identity;
- user composition with parameter substitution, defaults, and cycle checks;
- effective-set target, requirement, conflict, allow-list, exclusivity,
  cardinality, and ordering validation;
- `std::attribute` declarations and strict processor phase-interface checks;
- stable `std::attribute::*` identities shared by built-ins and user policy
  matching;
- bounded compile-time `Validator<TTarget>` execution with optional constant
  diagnostics;
- executable synchronous `PreProcessor.Before()`, `PostProcessor.After()`, and
  `FinallyProcessor.Finally()` hooks with callee-side lowering, reverse exit
  ordering, successful-return evaluation before post hooks, and exactly-once
  finalization on normal/exceptional exits;
- receiver-aware object-method pre hooks through `Before(receiver: any)`, with
  a boolean unit-return guard for callback-liveness and precondition patterns;
- deterministic topological processor order from `Before`/`After`, with source
  order as the stable tie-breaker and reverse-order exit unwinding;
- unit-returning synchronous `Around(proceed: fn())` lowering with zero-or-one
  Proceed, runtime duplicate-call protection, and escaped-capability rejection;
- method-level behavioral pipeline reflection exposing effective attribute,
  processor type, phase, hook, and mode in deterministic execution order;
- bounded checked derives for concrete component/object targets: a public Wio
  processor method marked `[std::attribute::DeriveMember]` becomes a typed
  member surface, receives the target as a hidden `any` receiver, and executes
  through an isolated default-constructed processor instance;
- structured runtime type-attribute descriptors with stable IDs, origin,
  normalized argument metadata, and default provenance.

Still open: target-aware validator contexts, typed/generic derive targets,
derived fields/properties and richer checked builders, typed argument/result
hook contexts, statically typed receiver contexts, typed-result around and
async behavioral lowering, the final removal of enum-only built-in lowering
paths, C++ SDK pipeline descriptors, source migration/formatting, editor/web
support, and Windows/Linux release qualification.

No processor capability is accepted as a silent no-op: behavioral processors
on non-callable targets and malformed or unsupported derive applications are
explicit compile-time errors.

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
standard processor interfaces. Pre- and post-processing are separate contracts;
an attribute does not implement a broad processor and then discover its phase
at runtime. The interface type, not a magic method name, decides how the body
participates:

```wio
[attribute::Targets(method)]
[attribute::Processor(CallbackMethodPre)]
attribute CallbackMethod;

object CallbackMethodPre : attribute::PreProcessor {
    public fn Before<TSelf, TArgs, TResult>(
        call: ref attribute::PreCall<TSelf, TArgs, TResult>
    ) -> attribute::PreFlow<TResult>
    where TSelf: native::LivePeer
    {
        if (not call.Self().IsAlive()) {
            return call.Skip();
        }
        return call.Continue();
    }
}

[CallbackMethod]
fn OnNativeEvent(event: view Event) {
    Handle(event);
}
```

The example syntax for the generic processor API remains subject to
implementation feedback, but the semantic rules are fixed:

- each phase context exposes only capabilities valid at that phase: `PreCall`
  has receiver/parameters but no result, `PostCall` has a successful result,
  `ExitCall` has success/error/cancellation outcome, and `AroundCall` owns the
  single `Proceed` capability;
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

The first checked derive slice intentionally adds methods rather than exposing
mutable syntax trees or source generation:

```wio
[From(attribute::DeriveProcessor<any>)]
object DescribeDerive {
    [std::attribute::DeriveMember("Describe")]
    public fn Build(receiver: any, prefix: string) -> string {
        return prefix + "<derived>";
    }
}

[attribute::Targets(object)]
[attribute::Processor(DescribeDerive)]
attribute Described();

[Described]
object User {}

let user = User();
let label = user.Describe("user:");
```

The target must currently be a concrete, non-generic component or object. A
derived method is public, synchronous, non-generic Wio code; its first explicit
parameter is `any`, and its remaining public parameters cannot use defaults or
packs. The processor must be default constructible. Constructors, operators,
native methods, name conflicts, ambiguous derives, and unsupported target
shapes are rejected during analysis. These bounds keep derive deterministic
and type-checked while typed target specialization and richer builders remain
future extensions.

The behavioral layer contains four independent interfaces:

- `attribute::PreProcessor` for entry guards and preconditions;
- `attribute::PostProcessor` for successful-return postconditions and
  type-compatible result mapping;
- `attribute::FinallyProcessor` for guaranteed exit/finalization;
- `attribute::AroundProcessor` for typed interception that may invoke the
  original body exactly once or produce a valid replacement outcome.

An attribute may attach more than one processor, for example a transaction may
use a pre processor to open state, a post processor to commit, and a finally
processor to roll back or release resources. Phase state crosses hooks only
through a declared typed state value; hidden thread-local or global coupling is
not part of the contract.

These are standard processor interfaces, not unrestricted macro hooks. Call-
site rewriting, token macros, arbitrary AST mutation, hidden overloads, and
silent public-signature changes are outside the system.

## 5. Ordering and composition

### 5.1 Composing existing attributes

Users may define an attribute from existing attributes without writing a
processor. `compose` is a contextual clause on attribute declarations, not a
general expression operator:

```wio
attribute PublicApi(symbol: string)
    compose [Export, CppName(symbol)];

[PublicApi("CreateWidget")]
fn CreateWidget() -> i32 {
    return 0;
}
```

Composition may itself use another composite attribute. Expansion is
compile-time, recursive, cycle-checked, and atomic: either the complete
normalized set is valid for the target or the application is rejected. The
source application remains visible as the parent; expanded applications record
`origin = composed` and the parent attribute stable ID. Runtime reflection
retains only applications whose effective retention requires it, while
compiler inspection can display the full expansion chain.

An attribute may combine composition and processors:

```wio
[attribute::Targets(method)]
[attribute::Processor(AuthorizePre)]
attribute AuthorizedExport(role: Role, symbol: string)
    compose [Export, CppName(symbol), Audit(role)];
```

Composition cannot capture runtime locals, mutate the target, synthesize
arguments by reflection, or defer constraint checking until runtime. Composite
arguments are folded structural compile-time values.

### 5.2 Combination limits

Attribute compatibility is declared with standard meta-attributes. This keeps
the grammar small while giving the analyzer a complete constraint graph:

```wio
[attribute::Targets(fn)]
[attribute::Requires(Export)]
[attribute::Conflicts(Native, Event)]
[attribute::OnlyWith(Export, CppName, Deprecated, Audit)]
[attribute::Cardinality(1)]
attribute Command(name: string);
```

The constraint vocabulary is:

- `Targets(...)`: valid declaration/member/parameter target kinds;
- `Requires(...)`: all listed attributes must be effective after composition;
- `RequiresAny(...)`: at least one listed attribute must be effective;
- `Conflicts(...)`: listed attributes may not be effective together;
- `OnlyWith(...)`: when present, every other non-policy attribute must be in
  the allow-list;
- `Exclusive("group")`: at most one effective attribute in the named group;
- `Cardinality(min, max)`: effective application count on one target;
- `Before(...)` and `After(...)`: behavioral processor ordering dependencies;
- `Implies(...)`: a metadata-only logical implication used for validation and
  querying; unlike `compose`, it does not create an application or arguments.

Constraints are evaluated on the normalized effective set after scoped,
inherited, composed, generated, and direct applications have been collected.
Diagnostics identify both sides, the composition/inheritance origin chain, and
the rule declaration. `OnlyWith` is intentionally uncommon; default behavior
allows unrelated metadata attributes unless a conflict or exclusivity rule
forbids them.

Core examples include:

- `Native` conflicts with `Export` on the same function;
- `CppHeader` requires `Native` at declaration scope but may activate lexically
  through `using`;
- `CppName` requires one of `Native` or `Export`;
- `Command` and `Event` require `Export` and conflict with each other;
- one module-lifecycle role is allowed per function, and fixed-symbol lifecycle
  attributes conflict with `CppName`;
- `Specialize` is valid only on matching generic object/component primaries;
- behavioral processors that can block conflict with main-loop/update targets
  unless the target explicitly permits blocking.

### 5.3 Ordering

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

## 6. Standard compile-time attribute interfaces

Every compiler-recognized attribute and processor contract is declared for
users in `std::attribute`; compiler knowledge is bound to stable declaration
identity rather than hidden spelling checks. The surface includes `Native`,
`Export`, `CppHeader`, `CppName`, module lifecycle attributes, generic
specialization/instantiation constraints, access/layout attributes, command/
event attributes, threading attributes, and all meta-attributes in this plan.

The standard module also declares the compile-time interfaces:

```wio
realm std {
    realm attribute {
        interface Validator<TTarget>;
        interface DeriveProcessor<TTarget>;
        interface PreProcessor;
        interface PostProcessor;
        interface FinallyProcessor;
        interface AroundProcessor;

        attribute Targets(targets: Target...);
        attribute Requires(attributes: AttributeType...);
        attribute RequiresAny(attributes: AttributeType...);
        attribute Conflicts(attributes: AttributeType...);
        attribute OnlyWith(attributes: AttributeType...);
        attribute Exclusive(group: string);
        attribute Cardinality(min: usize = 0usize, max: usize = 1usize);
        attribute Processor(type: type);

        [Targets(fn, method)]
        [Conflicts(Export)]
        attribute Native;

        [Targets(fn, object, component)]
        [Conflicts(Native)]
        attribute Export;
    }
}
```

The snippet is an interface sketch; exact pack/type-value spelling follows the
language's structural const/type metadata rules. A minimal compiler bootstrap
recognizes only the foundational meta-attribute declarations by stable ID so
that the file can describe itself. All ordinary built-ins use the same
resolution, normalization, constraint, reflection, and tooling pipeline as
user attributes. The std file contains declarations and contracts, not native
compiler implementation bodies.

Users may import/alias these declarations normally, while short canonical
forms such as `[Native]` resolve through the attribute prelude. Shadowing a
prelude attribute is diagnosed rather than silently changing compiler meaning.

## 7. Reflection, SDK, and tooling

The current `string[]` reflection surface is migration-only. It flattens an
application such as `[Route(method: Get, path: "/health")]` into display text,
which loses argument types, defaults, stable identity, origin, and processor
information. Wio 0.15 replaces that representation with two complementary
layers.

### 7.1 Typed queries

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

### 7.2 Erased inspection

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

### 7.3 Behavioral pipeline reflection

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

### 7.4 C++ SDK view

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

## 8. Compatibility and migration

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

## 9. Delivery order and release gate

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
