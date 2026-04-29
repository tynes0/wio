# Wio Todo List

This file is the project-wide roadmap for the Wio language, compiler, runtime,
standard library, tooling, and documentation.

It is based on:

- the current compiler codebase,
- the current draft language reference,
- the intended Wio language design,
- and the gaps between syntax, semantics, and generated C++.

The goal is not just to list ideas. The goal is to make the next engineering
steps obvious.

Status markers used in this file:

- [x] Done foundations already landed in the repo and should not be planned as
      net-new work again.
- [~] Partial / hardening work exists in some real form, but the semantics,
      tests, diagnostics, portability, or docs are not frozen yet.
- [ ] Open / not started work still needs design or implementation.

This file stays exhaustive. The active, ordered execution roadmap now lives in
`ORDERED_TODOLIST.md`.

---

## 1. Project Priorities

This section is the top-level status summary. For the actual phase-by-phase
execution order, see `ORDERED_TODOLIST.md`.

### 1.1 Completed Foundations

- [x] Land the first generic slice:
      generic functions, generic aliases, generic `object` / `component` /
      `interface`, explicit generic calls, and interop-oriented
      `@Instantiate(...)`.
- [x] Land the first loop slice:
      `for`, `foreach`, `in`, range iteration, dictionary iteration,
      component-binding iteration, and parenthesized `for (...)` readability.
- [x] Land the first namespace/import slice:
      `realm`, basic `use`, basic `use ... as ...`, and initial multi-file user
      module resolution through configured source roots.
- [x] Land the first source-based std slice:
      `std::console`, `std::math`, `std::collections`, `std::strings`,
      `std::fs`, `std::path`, `std::algorithms`, and `std::assert` /
      `std::testing`.
- [x] Land the first interop/runtime/productization slice:
      `@Native`, `@CppHeader`, `@CppName`, `@Export`, `@Command`, `@Event`,
      module lifecycle/state ABI, shared/static outputs, packaged Wio
      distributions, `wio.makewio`, template generation, and the public host
      SDK.
- [x] Land the first serious test wave:
      positive feature tests, invalid-program coverage, and native/shared/
      static/SDK interop tests.

### 1.2 P0 - Foundation First

- [ ] Make the compiler trustworthy on valid input and graceful on invalid
      input.
- [ ] Eliminate parser crashes, undefined behavior, and fragile assumptions.
- [ ] Make semantic analysis stricter than codegen, not weaker than codegen.
- [ ] Make every documented feature either implemented, clearly partial, or
      removed from user-facing docs until ready.
- [ ] Stop adding new surface syntax faster than the test suite, diagnostics,
      and spec can keep up.

### 1.3 P1 - Freeze Shipped Semantics

- [ ] Harden `use`, `as`, `realm`, and multi-file module behavior.
- [ ] Freeze `ref`, `view`, `fit`, `is`, `match`, `when`, attribute, and ctor
      semantics.
- [ ] Finish the collection-language contract:
      arrays, static arrays, dictionaries, trees, strings, and iteration
      behavior.
- [ ] Make diagnostics deliberate and readable across parser, sema, backend,
      and runtime-facing failures.

### 1.4 P2 - Productize the Current Alpha

- [ ] Stabilize the std/runtime/host ABI contract around the features that
      already exist.
- [ ] Improve CLI ergonomics, help output, and debug output
      (`tokens`, `AST`, typed AST, generated C++).
- [ ] Harden the current project model, packaged workflow, and examples into a
      reliable user-facing alpha experience.
- [ ] Expand docs into a status-marked reference plus focused tutorials.

### 1.5 P3 - Long-Horizon Features

- [ ] Reflection for `enum` and `flagset`.
- [ ] Concurrency model (`async`, `await`, `coroutine`, `yield`, `thread`).
- [ ] Time/gameplay scheduling keywords (`every`, `after`, `during`, `wait`).
- [ ] Pipeline/data-flow operators (`|>`, `<|`).
- [ ] `system` and `program` level abstractions.

---

## 2. Immediate Stabilization Work

These items should be handled before large new language features are added.

### 2.1 Compiler Correctness

- [ ] Audit the parser for all unsafe `back()`, `front()`, and unchecked
      indexing patterns.
- [ ] Audit every place where an empty token list, empty module path, empty
      argument list, or empty attribute argument list can cause UB.
- [ ] Add a hard rule that syntax errors must always become diagnostics, never
      crashes.
- [ ] Audit semantic analyzer assumptions around missing types, missing symbols,
      and partially built AST nodes.
- [ ] Add regression tests for every previously fixed crash.

### 2.2 Backend Reliability

- [ ] Remove backend assumptions that depend on the current working directory.
- [ ] Make runtime/include resolution independent from where the executable is
      launched.
- [ ] Decide whether Wio is a source-to-C++ transpiler only or a compiler that
      also owns the native backend invocation experience.
- [ ] Make the backend compile and run path work consistently in Debug and
      Release.
- [ ] Make backend compiler invocation robust when paths contain spaces.
- [ ] Stop assuming Windows-only execution for important workflows.

### 2.3 Diagnostics Baseline

- [ ] Standardize all diagnostics to include file, line, column, error
      category, and a clear human-readable explanation.
- [ ] Differentiate parser errors, semantic errors, backend errors, and runtime
      generation errors.
- [ ] Add "expected X, found Y" style diagnostics to parser failures.
- [ ] Add suggestion-style messages where the language is intentionally strict.
- [ ] Make duplicate/ambiguous overload diagnostics readable.
- [ ] Make access-control diagnostics clearly name the target member, owning
      type, attempted scope, and blocked access level.

---

## 3. Language Specification Work

The language reference now exists, but it still needs to become a true spec.

### 3.1 Formalization

- [ ] Convert the draft reference into a versioned language spec.
- [ ] Split the spec into clear sections: lexical grammar, syntax grammar, type
      system, declarations, expressions, statements, object model, modules,
      attributes, and standard library surface.
- [ ] Separate implemented behavior from design intent everywhere.
- [ ] Introduce language status markers per feature: stable, experimental,
      partial, reserved.

### 3.2 Grammar and Parsing Rules

- [ ] Write an explicit grammar for declarations.
- [ ] Write an explicit grammar for expressions.
- [ ] Write an explicit grammar for statements.
- [ ] Write an explicit grammar for type syntax.
- [ ] Document precedence and associativity for every operator.
- [ ] Decide whether optional parentheses in `if` and `while` are permanent.
- [ ] Decide whether `else` should support single non-block statements
      everywhere or whether block-only `else` is deliberate.
- [ ] Decide whether semicolons will always be mandatory.
- [ ] Decide whether newlines will remain ignored or later become meaningful.

### 3.3 Language Consistency Rules

- [ ] Define what counts as stable user syntax versus implementation accident.
- [ ] Remove accidental parser acceptance from the spec:
  - `[i32]` as zero-sized static array syntax,
  - float-sized static arrays,
  - untyped parameter edge cases,
  - partially initialized declarations that parse but should be rejected.
- [ ] Decide whether Wio wants an expression-oriented identity or a more
      statement-oriented one.
- [ ] Define a compatibility philosophy: C++-like and permissive, or safer and
      more explicit.

---

## 4. Core Type System

### 4.1 Built-in Types

- [ ] Freeze the built-in type list and naming conventions.
- [ ] Decide whether `object` is only the root object concept or also a
      user-visible universal base with explicit runtime semantics.
- [ ] Clarify the semantic role of `byte`, `char`, and `uchar`.
- [ ] Decide whether `string` is nullable by default.

### 4.2 Nullability

- [ ] Decide whether `null` should remain broadly compatible with almost
      everything.
- [ ] If not, design a nullability model before the language grows further.
- [ ] Decide how `null` interacts with `ref`, `view`, `object`, arrays,
      dictionaries, strings, and interfaces.
- [ ] Decide whether primitives may ever accept `null`.
- [ ] Add explicit negative tests for illegal null usage once the rules are
      frozen.

### 4.3 Type Aliases

- [x] Land the first source-level `type` alias syntax as
      `type Name = ExistingType;`.
- [ ] Decide whether aliases are pure aliases or new nominal types.
- [x] Land the first generic alias slice as
      `type Buffer<T> = T[];` with explicit type arguments only.
- [ ] Decide whether aliases can name function types, arrays, and dictionary
      types.
- [ ] Decide whether alias declarations should eventually accept attributes.

### 4.4 Generics / Templates

- [x] Freeze the initial surface syntax for generic functions as
      `fn Name<T, U>(...) -> ...`.
- [x] Land a first implementation for top-level generic free functions with
      parameter-based type deduction.
- [x] Support explicit generic call syntax for functions such as
      `Identity<i32>(42)` and `MakePair<i32, string>(...)`.
- [x] Lower the first generic slice through backend C++ templates rather than
      front-end monomorphization.
- [x] Add initial end-to-end tests for generic value passing, generic
      references, mismatch diagnostics, and unsupported generic methods.
  - [~] Freeze the v1 generic type-declaration surface.
    - [x] Support generic `object`, `component`, `interface`, and alias
          declarations.
    - [x] Keep generic `enum`, `flagset`, and `flag` declarations out of v1.
- [x] Broaden the first generic type-declaration slice beyond aliases:
      generic `component`, generic `object`, and generic `interface`
      declarations with explicit type arguments at use sites.
- [x] Support generic `@From(...)` inheritance for object/interface graphs,
      including generic base specialization such as `@From(Box<T>)` and
      `@From(Reader<i32>)`.
- [~] After the first generic struct slice lands, decide whether constructor
      calls may deduce generic type arguments or whether explicit
      `Box<i32>(...)`-style construction stays required.
  - [x] Allow constructor-based deduction for generic `object` construction
        when `OnConstruct(...)` parameters fully determine all generic slots.
  - [x] Allow constructor-based deduction for generic `component`
        construction when `OnConstruct(...)` parameters fully determine all
        generic slots.
  - [x] Allow zero-argument generic constructors to infer from expected type
        context in variable initialization, assignment, and return positions.
  - [x] Treat generic specializations as distinct runtime types so
        specialization-aware `is`, `fit`, export metadata, and hot-reload lookup
        stay sound.
  - [x] Support explicit type arguments at generic function call sites when
        inference is insufficient.
  - [~] Support generic methods after object/component/interface method lowering
        rules are clear.
    - [x] Allow generic methods on `object` declarations.
    - [x] Keep `component` generic methods out of v1.
    - [x] Keep generic `interface` methods unsupported in v1 unless the
          lowering model stops depending on C++ virtual templates.
    - [x] Keep generic constructors/destructors unsupported in v1.
  - [x] Keep v1 generic lowering backend-template-based only; no front-end
        monomorphization.
  - [x] Land the first light generic-constraint slice via `@Apply(...)` and
        predicate-based `@Instantiate(...)`, while keeping richer
        trait/expression systems out of v1.
  - [~] Broaden overload resolution tests for generic-vs-concrete preference,
        ambiguous generic overloads, and mixed namespace/module calls.
    - [x] Cover concrete-vs-generic preference.
    - [x] Cover specialized-generic-vs-broad-generic preference.
    - [x] Cover explicit type-argument calls preferring the generic overload.
    - [x] Cover broader generic preference over pure numeric conversion.
    - [ ] Add broader mixed realm/module and conversion-heavy overload cases.
- [x] Allow generic `@Native` functions only through explicit concrete
      `@Instantiate(...)` instance lists.
- [x] Allow generic `@Export` functions only through explicit concrete
      `@Instantiate(...)` instance lists.
- [x] Land a first source-level explicit-instantiation surface for interop via
      `@Instantiate(...)`.
- [x] Emit concrete exported generic wrappers and module metadata entries per
      `@Instantiate(...)` specialization.
- [x] Validate each generic `@Export` instantiation against the primitive C ABI
      surface before codegen.
- [x] Treat `@Instantiate(...)` as a hard call-site whitelist for generic
      `@Native` / `@Export` functions, so undeclared specializations are
      rejected during semantic analysis.
- [x] Require generic exported instances to derive concrete C ABI symbol names
      from the base export symbol plus concrete type mangling.
  - [x] Keep v1 explicit instantiation attribute-based via `@Instantiate(...)`,
        including multi-parameter specialization lists such as
        `@Instantiate(i32, bool)`.
  - [ ] Decide whether the long-term explicit-instantiation syntax should stay
        attribute-based or evolve into dedicated `instantiate` declarations.
  - [x] Assume header-only template definitions for v1 generic native bridging.
  - [ ] Design a stronger model for separately compiled template instantiations
        in v2+.
  - [ ] Define long-term ABI stability/versioning rules for exported generic
        instances, including what hosts may rely on in symbol names.
  - [x] Keep generic defaults and partial specialization out of v1.
  - [ ] Decide whether generic defaults and partial specialization will ever
        exist in later versions.
- [ ] Add end-to-end examples such as `Array<T>`, `Result<T>`, `Pair<K, V>`,
      generic utility functions, and container-oriented generic loops once the
      rest of the surface is frozen.
- [ ] Design variadic generics / generic packs for future versions, including a
      source-level spelling for forms like `Foo<Args...>(args: Args...)`.
- [ ] Decide whether the variadic-generic design also applies to generic
      `object` / `component` / `interface` declarations.
- [ ] Design how variadic generic expansion should interact with overload
      resolution, backend template lowering, and future constraint systems.
- [ ] Design const-generic / non-type generic parameters for future versions,
      especially numeric forms such as `Vector<T, N>` and their interaction
      with static arrays like `[T; N]`.

---

## 5. Variables, Constants, and Initialization

### 5.1 `let`, `mut`, and `const`

- [ ] Define precise semantics for immutable binding, mutable binding, and
      compile-time constant.
- [ ] Decide whether `const` should strictly mirror C++ `constexpr` or become a
      Wio-level constant model with backend lowering rules.
- [ ] Enforce constexpr-like restrictions in semantic analysis if `const`
      remains mapped to C++ `constexpr`.

### 5.2 Initialization Rules

- [ ] Decide whether `let x;` should be illegal, deferred-initialized, or only
      valid in certain scopes.
- [ ] Decide whether `mut x;` without type or initializer is ever legal.
- [ ] Define definite-initialization rules for locals.
- [ ] Define initialization rules for fields, globals, and constants.
- [ ] Ensure parser and semantic analyzer enforce the same rules.

### 5.3 Type Inference

- [ ] Document exactly when inference is allowed.
- [ ] Decide whether inference is allowed for fields, globals, lambda
      parameters, and function returns.
- [ ] Document integer literal default typing precisely.
- [ ] Document float literal default typing precisely.
- [ ] Add tests for mixed numeric literals, large literals, and suffix
      behavior.

---

## 6. References and Views

This is one of the language-defining features and needs to be extremely solid.

### 6.1 Surface Semantics

- [ ] Freeze the mental model of `ref`.
- [ ] Freeze the mental model of `view`.
- [ ] Decide whether `view` is a read-only reference, immutable borrow, or
      non-owning read-only handle.
- [ ] Decide whether `ref`/`view` can ever be null.
- [ ] Decide whether `ref`/`view` are first-class runtime objects or purely
      language-level reference semantics.

### 6.2 Chained References

- [ ] Formally document chained references such as `ref ref i32`.
- [ ] Decide whether reference collapsing rules should exist.
- [ ] Decide whether `view ref T` and `ref view T` are both valid concepts.
- [ ] Define how assignment, passing, and conversion work through nested refs.
- [ ] Add exhaustive tests for nested reference creation, passing, assignment,
      and codegen.

### 6.3 Function Parameters and Calls

- [ ] Define call-site rules for passing values to `ref` parameters.
- [ ] Define whether temporaries may bind to `view`.
- [ ] Define whether temporaries may bind to `ref`.
- [ ] Define whether implicit reference creation is ever allowed.
- [ ] Decide how overload resolution treats `T`, `ref T`, and `view T`.

### 6.4 Ownership and Lifetime

- [ ] Decide whether Wio will eventually expose ownership/borrowing concepts.
- [ ] If not, define the lifetime limits of references in a simpler model.
- [ ] Decide whether returning `ref` to locals must always be rejected.
- [ ] Add semantic checks for escaping invalid references.

---

## 7. Numeric Conversions and `fit`

### 7.1 Numeric `fit`

- [ ] Freeze the exact rules for integer-to-integer `fit`.
- [ ] Freeze the exact rules for float-to-integer `fit`.
- [ ] Freeze the exact rules for integer-to-float `fit`.
- [ ] Decide whether float narrowing also clamps.
- [ ] Decide how NaN and infinity behave under `fit`.
- [ ] Decide whether signed-to-unsigned negative conversion clamps or errors.
- [ ] Fix the current signed/unsigned clamp path so mixed-width `fit`
      conversions cannot trip backend/runtime assertions.

### 7.2 Object and Interface `fit`

- [ ] Define whether object/interface `fit` is a checked cast, static cast,
      dynamic cast, or backend-dependent behavior.
- [ ] Decide failure behavior for invalid object `fit` outside of
      `if (x is T fit y)`.
- [ ] Decide whether failed object `fit` yields `null`, a trap, a compile-time
      rejection, or a runtime check failure.
- [ ] Define constness/read-only interaction between `view`, interfaces, and
      cast results.

### 7.3 `fit` + `is`

- [ ] Freeze the semantics of `if (target is T fit t)`.
- [ ] Decide scope and lifetime of the bound variable `t`.
- [ ] Decide whether `else` branches may refer to negated type information.
- [ ] Decide whether `match` should eventually support `is`/`fit` patterns too.

### 7.4 Tests for `fit`

- [ ] Add tests for all integer boundary values.
- [ ] Add tests for all signed/unsigned boundary crossings.
- [ ] Add tests for float-to-int truncation/clamping.
- [ ] Add tests for interface downcasts and upcasts.
- [ ] Add tests for invalid casts and failure modes.

---

## 8. Arrays, Dictionaries, and Collections

### 8.1 Dynamic Arrays

- [ ] Finalize the public semantics of `T[]`.
- [ ] Decide whether dynamic arrays are owning arrays, vector-like containers,
      or runtime-managed sequence objects.
- [ ] Define indexing bounds behavior.
- [ ] Define value semantics, copy behavior, and equality.
- [ ] Define whether arrays of `ref`/`view` are allowed.

### 8.2 Static Arrays

- [ ] Freeze `[T; N]` as the only static-array syntax.
- [ ] Reject accidental forms such as `[T]` unless intentionally repurposed.
- [ ] Require integer compile-time sizes only.
- [ ] Define zero-sized array policy.
- [ ] Define implicit conversion rules between static and dynamic arrays.
- [ ] Allow non-`ref`, non-`view` dynamic-array parameters such as `T[]` to
      accept static-array arguments such as `[T; N]` through a well-defined
      value-copy or temporary conversion path.
- [ ] Keep `ref T[]` / `view T[]` parameter passing strict unless a separate
      borrow/view model for static arrays is designed explicitly.

### 8.3 Dictionaries

- [ ] Finalize the language-facing dictionary story.
- [ ] Decide whether both `Dict<K, V>` and `Tree<K, V>` remain public types.
- [ ] Decide whether `Dict` maps to hash-based storage and `Tree` to ordered
      storage permanently.
- [ ] Decide whether a generic `Map<K, V>` facade should exist.
- [ ] Finalize dictionary literal syntax and guarantee it in the spec.
- [ ] Define key equality and ordering requirements.
- [ ] Define iteration order guarantees.
- [ ] Define duplicate-key behavior in literals.
- [ ] Design the pair structure exposed to user code.

### 8.4 Collection APIs

- [ ] Define what methods arrays should expose.
- [ ] Define what methods dictionaries should expose.
- [ ] Decide whether methods are language-provided, stdlib-provided, or both.
- [ ] Make arrays and dictionaries eventually feel class-like at call sites
      (for example `values.len()`, `values.contains(x)`, `map.get(key)`),
      even if the first implementation is lowered through stdlib helpers or
      compiler sugar.
- [ ] Define `len`, `contains`, `push`, `remove`, `clear`, `keys`, `values`,
      and iterator access.

### 8.5 Destructuring and Iteration

- [~] Freeze the first shipped `for` / `foreach` surface.
  - [x] Support `for` iteration over arrays, static arrays, ranges, and
        dictionaries.
  - [x] Support component-binding and index-binding loop forms.
  - [x] Support the parenthesized readability form `for (...)`.
  - [x] Keep a first working `foreach` surface in the language.
  - [ ] Decide whether `for` and `foreach` both remain long-term user-facing
        keywords.
  - [ ] Tighten diagnostics and edge-case semantics for all loop variants.
- [~] Design destructuring/binding policy for iteration.
  - [x] Ship initial dictionary key/value binding and component field binding.
  - [ ] Decide which binding forms become permanent syntax versus sugar.
- [ ] Design further destructuring forms such as:
  - `for (key | value in dict)`,
  - `for (x | y | z in transforms)`.
- [ ] Decide whether component auto-binding/destructuring is a first-class
      language feature or sugar lowered by the compiler.

---

## 9. Functions, Calls, and Overloads

### 9.1 Function Declaration Rules

- [ ] Freeze the function grammar.
- [ ] Decide whether parameter types may ever be omitted in user code.
- [ ] Decide whether return type omission always means `void`.
- [ ] Decide whether functions can be declared without bodies outside
      interfaces.

### 9.2 Overload Resolution

- [ ] Define overload ranking rules precisely.
- [ ] Define how numeric conversion participates in overload resolution.
- [ ] Define how `ref` and `view` participate in overload resolution.
- [ ] Define ambiguity diagnostics.
- [ ] Define whether default arguments will ever exist.
- [ ] If default arguments are accepted, design a first source surface such as
      `fn Foo(x: i32, y: i32 = 0)` and specify:
  - call-site omission rules,
  - interaction with overload resolution,
  - interaction with named/defaulted later parameters,
  - and lowering into generated C++.

### 9.3 Lambdas

- [ ] Stabilize lambda syntax and semantics.
- [ ] Decide whether lambda parameter types must always be explicit.
- [ ] Decide capture rules.
- [ ] Decide whether lambdas are lowered to plain function objects,
      std::function-like wrappers, or generated structs.
- [ ] Add tests for lambda return inference and block-bodied lambdas.

### 9.4 Entry Point

- [ ] Freeze the allowed `Entry` signatures.
- [ ] Decide whether `Entry()` without args is allowed.
- [ ] Decide whether only `i32` and `void` returns are valid.
- [ ] Add direct diagnostics for invalid entry signatures.

---

## 10. Control Flow

### 10.1 `if` / `else`

- [ ] Define allowed condition types.
- [ ] Decide whether only `bool` should be legal long-term.
- [ ] If numeric/object truthiness remains, define it explicitly.
- [ ] Make `if` and `while` use the same condition rules unless there is a good
      reason not to.

### 10.2 `while`

- [ ] Freeze `while` condition semantics.
- [ ] Freeze body grammar for single statements vs blocks.
- [ ] Add tests for nested loops and control-flow correctness.

### 10.3 `break`, `continue`, and `return`

- [ ] Ensure they are rejected outside valid scopes.
- [ ] Add explicit diagnostics for illegal usage.
- [ ] Add tests for nested loop/function interactions.

### 10.4 `when`

- [ ] Freeze the syntax of `when`.
- [ ] Decide whether `when` is a function contract, a runtime guard, or syntax
      sugar for an early-return precondition.
- [ ] Decide whether `when` may exist on methods, lambdas, and constructors.
- [ ] Decide whether `else` may be an expression only or a full block.
- [ ] Decide whether `when` may reference only parameters or any visible symbol.
- [ ] Add tests for void and non-void guarded functions.

### 10.5 `match`

- [ ] Freeze `match` as both statement and expression.
- [ ] Define arm typing rules rigorously.
- [ ] Decide whether block arms may produce values.
- [ ] Define whether arm fallthrough is impossible by design.
- [ ] Define the exact role of `assumed`.
- [ ] Require or forbid exhaustive matching in certain future cases.
- [ ] Define whether ranges may appear with non-integral types.
- [ ] Define whether overlapping match arms are allowed, warned, or rejected.
- [ ] Define case ordering significance.
- [ ] Add tests for multi-value arms, `or` arms, inclusive ranges, exclusive
      ranges, expression-valued matches, and mixed-type arm failures.

---

## 11. Operators and Expressions

### 11.1 Operator Table

- [ ] Freeze the operator set.
- [ ] Freeze precedence and associativity.
- [ ] Document unary vs binary `-`.
- [ ] Document `and`/`or`/`not` as aliases versus distinct operators.

### 11.2 Assignment Operators

- [ ] Verify codegen for all compound assignments.
- [ ] Add tests for ref-targeted compound assignments.
- [ ] Add tests for bitwise compound assignments.

### 11.3 Bitwise Operations

- [ ] Decide whether bitwise ops are legal for `flagset` only or also raw ints.
- [ ] Decide whether `~=` should remain a real operator or be removed if it has
      no clear semantic value.
- [ ] Document signedness behavior for shifts.

### 11.4 Range Operators

- [ ] Freeze `...` and `..<`.
- [ ] Decide whether they are only for `match` or general expressions too.
- [ ] Decide whether they should later integrate with loops and slices.

### 11.5 Flow Operators

- [ ] Decide whether `|>` and `<|` are worth keeping.
- [ ] If yes, define precedence, associativity, call-lowering behavior, and
      interaction with methods and lambdas.
- [ ] If no, remove them from the token set and docs until ready.

### 11.6 Member and Scope Access

- [ ] Clarify the semantic difference between `.` and `::`.
- [ ] Decide whether `::` is namespace-only or also usable for enum/flagset
      members.
- [ ] Add tests for nested namespace resolution.

---

## 12. Strings, Characters, and Formatting

### 12.1 Strings

- [ ] Freeze the `string` runtime representation.
- [ ] Decide whether `string` is UTF-8, UTF-16, UTF-32, or backend-native.
- [ ] Decide copy and ownership behavior for strings.
- [ ] Define literal escape behavior fully.

### 12.2 Interpolated Strings

- [ ] Freeze `$"..."` as the interpolation syntax.
- [ ] Decide whether multiline interpolation remains supported.
- [ ] Decide formatting rules for embedded expressions.
- [ ] Decide whether custom formatting specifiers will exist later.
- [ ] Make sure generated backend code is safe and readable.

### 12.3 Characters and Unicode

- [ ] Clarify the difference between `char` and `uchar`.
- [ ] Decide whether character literals are ASCII-only or Unicode-capable.
- [ ] Add tests for escape sequences and invalid char literals.

### 12.4 Byte Literals

- [ ] Design byte literal syntax or remove the placeholder until ready.
- [ ] Add lexer, parser, sema, and backend support once syntax is frozen.

---

## 13. Object Model

This area defines Wio's identity more than almost anything else.

### 13.1 `component`

- [ ] Freeze the design goal of `component` as POD-like data.
- [ ] Enforce "no ordinary methods" if that remains the design.
- [ ] Decide whether helper/static methods are ever allowed on components.
- [ ] Decide whether `self` is valid inside component ctors/dtors only or more
      broadly.
- [ ] Decide whether `super` is completely illegal for components.
- [ ] Add semantic checks and tests for every illegal component construct.

### 13.2 `object`

- [ ] Freeze object inheritance semantics.
- [ ] Decide whether every object implicitly derives from root `object`.
- [ ] Decide how method overriding is validated.
- [ ] Decide whether explicit `override` syntax will ever be needed.
- [ ] Decide whether constructors participate in inheritance chains
      automatically or explicitly.
- [ ] Define base-constructor invocation rules.

### 13.3 `interface`

- [ ] Freeze interface method rules.
- [ ] Decide whether interfaces may ever contain default methods, properties,
      constants, or attributes.
- [ ] Decide runtime dispatch strategy for interface calls.
- [ ] Add tests for multi-interface implementation.

### 13.4 Access Control

- [ ] Freeze public/private/protected semantics.
- [ ] Decide whether module-level visibility should exist later.
- [ ] Ensure access control is enforced identically in sema and backend.

### 13.5 `self` and `super`

- [ ] Fully test `self` inside object methods, object ctors, component ctors,
      and destructors.
- [ ] Fully test `super` in derived object methods.
- [ ] Decide whether `super` may access fields directly or only methods.
- [ ] Decide whether `super` may be used inside constructors.
- [ ] Decide whether a component should support `self` as field shorthand even
      if it does not support methods.

---

## 14. Attributes

Attributes are one of Wio's best design hooks, but they need a much tighter
contract.

### 14.1 Attribute System Foundations

- [ ] Decide whether attributes are compiler built-ins only or eventually
      extensible/custom.
- [ ] Replace raw-token attribute argument parsing with proper expression or
      type-aware parsing where needed.
- [ ] Decide where attributes are legal.
- [ ] Decide attribute ordering and duplicate-attribute rules.
- [~] Harden the first generic-filtering attribute slice.
  - [x] Support `@Apply(...)` on generic functions, aliases, `object`,
        `component`, and `interface` declarations.
  - [x] Support concrete-type and simple predicate constraints such as
        `IsInteger<T>` and `IsNumeric<T>`.
  - [x] Allow predicate-based `@Instantiate(...)` expansion for generic
        `@Native` / `@Export` functions.
  - [ ] Design `@ApplyIf(...)` or a richer compile-time predicate system for
        future type-trait style checks.
- [ ] Decide whether `@Apply` / `@ApplyIf` remain attribute-based or evolve
      into a more explicit generic-constraint syntax later.

### 14.2 `@From`

- [ ] Freeze `@From` as the inheritance syntax or replace it with source-level
      inheritance syntax.
- [ ] Define ordering rules when multiple `@From(...)` attributes are present.
- [ ] Reject more than one object base with precise diagnostics.
- [ ] Make interface/object mixing rules explicit.

### 14.3 `@Default`

- [ ] Define exactly which declarations `@Default(...)` can target.
- [ ] Decide whether it affects fields only, methods too, nested declarations,
      or generated constructors.
- [ ] Add tests for conflicting explicit access modifiers.

### 14.4 `@GenerateCtors`

- [ ] Freeze which constructors are generated: empty/default, member-wise, and
      copy.
- [ ] Decide whether move constructors will ever exist.
- [ ] Decide whether member constructor generation depends strictly on field
      declaration order.
- [ ] Decide whether generated ctors should skip fields with default
      initializers.
- [ ] Decide how inheritance affects generated ctors.

### 14.5 `@NoDefaultCtor`

- [ ] Clarify whether it disables only the empty ctor or all implicit ctor
      generation.
- [ ] Define interaction with `@GenerateCtors`.

### 14.6 `@ReadOnly`

- [ ] Clarify whether it means externally read-only, globally immutable after
      construction, or setter-forbidden outside trusted scopes.
- [ ] Define interaction with constructors and trusted types.
- [ ] Add diagnostics for illegal writes.

### 14.7 `@Trust`

- [ ] Finish semantic enforcement of trusted access.
- [ ] Decide whether trust is symmetric, one-way, transitive, or
      non-transitive.
- [ ] Decide whether `@Trust` can target multiple types.
- [ ] Define interaction with inheritance.
- [ ] Add tests for trusted access to private and protected members.

### 14.8 `@Final`

- [ ] Enforce `@Final` in semantic analysis.
- [ ] Decide whether final methods will ever exist, not just final objects.

### 14.9 `@Type`

- [ ] Freeze allowed target declarations for `@Type`.
- [ ] Define allowed underlying types for `enum` and `flagset`.
- [ ] Reject invalid underlying types with clear diagnostics.

---

## 15. Enums, Flagsets, and Flags

### 15.1 `enum`

- [ ] Define whether `enum` is scoped or unscoped in user semantics.
- [ ] Define default underlying type.
- [ ] Decide whether implicit numeric conversions are allowed.
- [ ] Decide stringification/reflection strategy for the future.

### 15.2 `flagset`

- [ ] Freeze `flagset` as the bitmask enum model.
- [ ] Define supported operators for `flagset`.
- [ ] Decide whether mixed raw-int and flagset operations are legal.
- [ ] Add tests for bitwise combinations and comparisons.

### 15.3 `flag`

- [ ] Clarify the exact use case for `flag`.
- [ ] Decide whether `flag` is a marker type, zero-size type, tag type, or a
      structural compile-time concept.
- [ ] Design real examples that justify keeping it as a language feature.
- [ ] If the use case remains weak, consider demoting it from the core language
      to a library pattern.

### 15.4 Reflection

- [ ] Design default reflection metadata for `enum`.
- [ ] Design default reflection metadata for `flagset`.
- [ ] Decide whether reflection is compile-time only, runtime only, or both.

---

## 16. Modules, Imports, and Namespaces

The first `use` / `as` / `realm` slice is now in the repo, but multi-file
semantics are still one of the biggest hardening gaps.

### 16.1 `use`

- [~] Finish semantic resolution for `use`.
  - [x] Support initial std and user-module lookup through configured source
        roots.
  - [x] Keep working import execution covered by end-to-end tests.
- [ ] Guarantee deterministic module lookup behavior.
- [ ] Define relative vs absolute import semantics.
- [ ] Define error messages for missing modules and duplicate imports.

### 16.2 `as`

- [~] Finish `use ... as ...` aliasing support.
  - [x] Support the first working alias surface for std modules and user
        modules.
- [ ] Define shadowing behavior for aliases.
- [ ] Define whether aliases can rename modules, namespaces, imported types, and
      imported functions.

### 16.3 Import Path Forms

- [~] Finalize support for import-path forms such as `use std::console;`,
      `use foo::bar;`, `use self::foo;`, `use super::bar;`, and relative
      multi-segment paths.
  - [x] Support working root-qualified user module imports.
  - [x] Support working std-module imports and aliases.
  - [ ] Define `self` / `super` semantics precisely.
- [ ] Decide whether wildcard imports will ever exist.

### 16.4 Multi-File Semantics

- [~] Establish the first real multi-file compilation model.
  - [x] The current project model already resolves multi-file user code and std
        modules through source roots and manifests.
  - [x] Keep end-to-end coverage for root imports, aliasing, module merge, and
        realm merge.
- [ ] Decide whether compilation is file-merging, module-compilation, or
      package-based.
- [ ] Define symbol visibility across files.
- [ ] Detect and report circular imports cleanly.
- [ ] Decide how the compiler should cache previously loaded modules.
- [~] Add end-to-end tests for multi-file projects.
  - [x] Keep the first user-module / std-module / alias / realm tests alive.
  - [ ] Broaden that coverage to circularity, shadowing, and larger layouts.

### 16.5 Realm and Namespace Semantics

- [~] Treat `realm` as the explicit Wio surface keyword for namespace-like
      scopes.
  - [x] Land the first working `realm` surface.
  - [x] Cover nested realm lookup and qualified access with tests.
- [ ] Decide whether modules implicitly create realms, remain flat, or can do
      both.
- [ ] Clarify how `::` lookup works for realms, std modules, imported modules,
      enums, flagsets, and nested declarations.
- [ ] Define whether nested `realm` declarations are fully supported and how
      name mangling stays stable across them.
- [~] Add positive and negative tests for realm collisions, nested realm lookup,
      and realm-qualified type references.
  - [x] Cover the first nested and qualified realm cases.
  - [ ] Add more collision and failure-mode coverage.

---

## 17. Standard Library and Runtime

### 17.1 Runtime Foundation

- [ ] Document the runtime ABI expected by generated C++.
- [~] Separate runtime concerns from compiler concerns more cleanly.
  - [x] Move the public host-facing ABI into `sdk/include`.
  - [x] Keep the runtime intentionally low-level.
  - [ ] Continue shrinking compiler-owned implementation detail leakage.
- [ ] Decide what is guaranteed to exist in every Wio program.
- [x] Keep `runtime/*` intentionally small and low-level: refcounting,
      exceptions, ABI glue, `fit`, low-level I/O hooks, and host/module support.
- [x] Treat `runtime/*` as the implementation substrate, not the primary
      user-facing standard library surface.
- [~] Define a clean boundary between:
  - compiler-private internals,
  - mandatory runtime support,
  - public host/embedding SDK,
  - and user-facing `std` modules.

### 17.2 `std::console`

- [~] Formalize the stable API of `std::console`.
  - [x] Move the primary user-facing console surface to `std::console`.
  - [x] Ship output and input helpers through `.wio` + `@Native`.
  - [x] Cover direct and surface console usage with tests.
  - [ ] Decide the long-term formatting and input contract precisely.

### 17.3 Core Library Modules

- [x] Make the public standard-library surface primarily `.wio`-first, with
      `@Native` used where the implementation must drop to runtime/C++.
- [~] Move high-level public APIs out of raw `runtime/*.h` exposure and into
      `std/*.wio` declarations/modules.
  - [x] Ship the first real source-based std modules.
  - [ ] Keep reducing remaining raw-runtime leakage.
- [ ] Classify each std module as one of:
  - pure Wio,
  - Wio surface backed by `@Native`,
  - or runtime-only/internal.
- [~] Freeze the first real `std` layout.
  - [x] Ship `std::console`, `std::math`, `std::collections`,
        `std::strings`, `std::fs`, `std::path`, `std::algorithms`,
        and `std::assert` / `std::testing`.
  - [ ] Decide whether `std::mem`, `std::time`, and `std::reflect` join the
        early core set.
- [ ] Decide which of these are language-critical versus optional.
- [ ] Keep the early stdlib intentionally small/medium sized rather than
      shipping a giant all-in-one runtime surface.
- [ ] Prefer pure Wio implementations for higher-level algorithms, helpers,
      and utilities once the language surface is stable enough.
- [ ] Grow the standard library toward richer object/component-based domains
      where it makes sense, rather than only free-function helpers.
- [ ] Design a future math/vector library with `Vec2`, `Vec3`, and `Vec4`
      component types plus companion helper/object APIs.

### 17.4 Collections Library

- [x] Expose array helpers.
- [x] Expose dictionary/tree helpers.
- [ ] Expose pair/iterator concepts if loops depend on them.
- [~] Harden the second wave of generic callback-oriented std helpers such as
      `CountBy`, `Filter`, `Map`, `Reduce`, and `GroupCountBy`.
  - [x] Ship the first callback-heavy algorithms wave.
  - [ ] Keep tightening the generic inference and diagnostics around them.
- [~] Teach generic std-heavy code paths to handle callback-centric local state
      cleanly:
  - generic local variable typing,
  - empty collection literals under expected-type context,
  - lambda-to-function parameter lowering,
  - and callback-heavy generic body analysis.

### 17.5 Reflection Runtime

- [ ] Plan the runtime/library side of reflection before exposing it in syntax.
- [ ] Decide how reflection metadata is emitted and accessed.

### 17.6 Library and Host Integration

- Experimental progress:
  - `@Native`, `@CppHeader`, and `@CppName` now provide an initial top-level
    C++ function bridge.
  - `--include-dir`, `--link-dir`, `--link-lib`, and `--backend-arg` now allow
    backend compile/link customization from the CLI.
  - A real end-to-end native bridge test now exists, but the surface is still
    alpha and currently limited to top-level functions.
  - `--target exe|static|shared` and `--output` now exist as an initial build
    mode split.
  - `@Export` now provides the first host-callable Wio-to-C++ bridge for
    top-level functions.
  - Static-library host interop is now covered by an end-to-end test.
  - Shared-library host loading is now covered by an end-to-end loader test.
  - A first fixed module lifecycle ABI now exists through `@ModuleApiVersion`,
    `@ModuleLoad`, `@ModuleUpdate`, and `@ModuleUnload`.
  - Reload-oriented state handoff now has an initial fixed ABI through
    `@ModuleSaveState` and `@ModuleRestoreState`.
  - Safe rebinding and true hot-reload state handoff are still future work.

- [x] Plan a user-facing host/bridge SDK as a separate public surface instead
      of exposing compiler internals directly.
- [ ] Move runtime/std implementation code out of compiler-owned header-only
      internals where practical and into a separate packaged static library
      project.
- [ ] Make the compiler/backend link against that packaged Wio runtime/std
      library instead of embedding most implementation details directly through
      headers.
- [ ] Keep user-facing packaged distributions focused on compiled/linkable
      runtime artifacts rather than exposing all implementation details in the
      compiler project tree.
- [x] Package the project conceptually as:
  - `wio.exe` for compilation,
  - a small runtime support layer,
  - and a public host/embedding SDK for C++ integration.
- [x] Clarify the long-term packaging split between:
  - compiler toolchain,
  - low-level runtime library,
  - std implementation library,
  - and public host SDK headers/libs.
- [x] Define the public host SDK around embedding/interop concerns only:
  - module loading,
  - export discovery,
  - command/event invocation,
  - reload helpers,
  - and runtime/result/error helpers.
- [x] Expand `@Export` beyond top-level functions so exported `object` and
      `component` declarations can expose public members to C++ hosts.
- [x] Define the exact surface for exported public fields on `@Export`
      `object` / `component` types:
  - getter generation,
  - setter generation,
  - naming conventions,
  - and rules for unsupported field kinds.
- [x] Decide how `private`, `protected`, `@ReadOnly`, `ref`, `view`, arrays,
      dictionaries, strings, nested components, and object references behave
      when exported as field properties.
- [x] Add automatic getter/setter bridge generation for exportable public
      fields so hosts do not need handwritten binding code for simple property
      access.
- [x] Design high-level `WioObject` and `WioComponent` helper types in the
      public `wio_sdk` so hosts can bind/export public methods and public
      variables through a small, ergonomic API instead of raw ABI calls.
- [x] Define whether `WioObject` / `WioComponent` are static registration
      helpers, runtime reflection wrappers, or both.
- [x] Define host-side lifetime, ownership, and instance-handle semantics for
      exported objects/components, especially under shared-library reload.
- [x] Keep AST, parser, sema, and codegen internals out of the public SDK.
- [x] Decide which `runtime/*` headers stay internal and which become official,
      documented SDK headers.
- [x] Add end-to-end examples where a C++ app integrates Wio through the
      official SDK rather than ad-hoc helper code.
- [x] Add a first-class distinction between executable builds and library
      builds.
- [x] Support static library output for embedding Wio-generated code into larger
      C++ projects.
- [x] Support shared/dynamic library output for host-driven scripting workflows.
- [ ] Define a stable host ABI for calling Wio code from C++ and C++ code from
      Wio.
- [ ] Design native/foreign declarations plus backend link configuration so Wio
      code can consume existing C++ static and shared libraries directly.
- [ ] Design reload-safe boundaries for future hot-reload support.
- [ ] Decide what metadata or registration layer is needed so hot-reloaded
      modules can be discovered and rebound safely.
- [ ] Define what "game scripting mode" means in practice:
  - host-owned lifetime,
  - dynamic module reload,
  - event entry points,
  - safe state handoff,
  - low-friction C++ interop.

---

## 18. Code Generation and Backend Design

### 18.1 Generated C++ Quality

- [ ] Make generated C++ readable enough for debugging.
- [ ] Preserve stable naming for generated temporaries where possible.
- [ ] Add comments or line mapping support for easier debugging.
- [ ] Decide whether generated code is user-facing or purely an internal
      backend artifact.

### 18.2 Backend Semantics

- [ ] Ensure the backend does not silently change language meaning.
- [ ] Align semantic analyzer rules with the actual generated C++ semantics.
- [ ] Audit every place where backend code currently carries more logic than the
      semantic analyzer.

### 18.3 Portability

- [ ] Make backend invocation work on Windows, Linux, and macOS.
- [ ] Decide whether a specific compiler toolchain is required.
- [ ] Provide configurable backend compiler selection.

### 18.4 Feature Lowering Audits

- [ ] Audit lowering for `ref`, `view`, `fit`, `match`, lambdas, dictionary
      literals, generated ctors, and interface dispatch.
- [ ] Add goldens or snapshot tests for generated C++ output.

### 18.5 Performance and Transpilation Goals

- [ ] Keep the generated C++ close enough to idiomatic code that optimized
      builds can realistically match hand-written C++ performance in common
      cases.
- [ ] Track where Wio runtime abstractions add unavoidable overhead and where
      they can be zero-cost.
- [ ] Define which language features must remain performance-transparent for
      gameplay scripting use cases.
- [ ] Benchmark generated output against equivalent C++ for:
  - function calls,
  - object/interface dispatch,
  - arrays and dictionaries,
  - string formatting,
  - hot path arithmetic.

---

## 19. Tooling and Developer Experience

### 19.1 CLI

- [ ] Freeze CLI behavior and flags.
- [ ] Ensure `--help` is always accurate.
- [ ] Add explicit modes such as `--check`, `--emit-cpp`, `--show-tokens`,
      `--show-ast`, `--show-types`, and `--run`.
- [ ] Decide whether `--dry-run` means parse-only, sema-only, or no backend
      run.

### 19.2 Build System

- [~] Clean up CMake options and document them.
  - [x] Land the first docs and helper integration.
  - [ ] Keep tightening option naming and package-facing docs.
- [~] Make app/library/test targets predictable.
  - [x] The repo already exposes real app/test/package/project-model flows.
  - [ ] Keep making target naming and discovery more uniform.
- [~] Ensure tests are runnable from a fresh checkout.
  - [x] The current scripts and CTest surface cover the first real path.
  - [ ] Keep reducing environment-specific assumptions.
- [x] Design a first-class Wio project model/generator for mixed Wio + C++
      projects so users do not need long handwritten compiler/link commands.
- [x] Decide that the primary manifest is a Wio-specific declarative format:
      `wio.makewio`, with legacy JSON still accepted for compatibility.
- [~] Decide whether the generator emits CMake, owns the build graph directly,
      or supports both a generated-native and generated-CMake workflow.
  - [x] The first slice already supports both a generated CMake path and a
        direct script-driven path.
  - [ ] Freeze the long-term contract and polish it.
- [~] Ensure the project model can describe:
  - Wio source roots,
  - C++ source roots,
  - include paths,
  - library links,
  - module/shared-library builds,
  - runtime/std packaging,
  - and hot-reload-friendly targets.
- [~] Keep the project generator focused on user convenience first, especially
      for game-scripting style projects that mix host C++ and Wio modules.

### 19.3 Example Programs

- [~] Add a curated examples directory:
  - hello world
  - arrays
  - references
  - objects and interfaces
  - attributes
  - modules
  - match
  - dictionaries
  - [x] Ship the first real examples through `hybrid_arena` and
        `static_cmake_consumer`.
  - [ ] Expand the examples set with smaller focused programs.
- [~] Ensure examples are exercised in automated flows.
  - [x] Keep `hybrid_arena` alive in the current test/demo flow.
  - [ ] Bring the whole examples directory under a stricter automated contract.

### 19.4 Editor Support

- [ ] Plan syntax highlighting.
- [ ] Plan formatter support.
- [ ] Plan LSP support for diagnostics and completion.

---

## 20. Testing Strategy

Wio still needs more tests than new syntax, but the first serious test wave is
already in the repo: feature runs, invalid-program coverage, interop coverage,
and SDK coverage.

### 20.1 Lexer Tests

- [ ] Add tests for every token type.
- [ ] Add tests for comments, escapes, string interpolation, durations, and
      future reserved keywords.
- [ ] Add tests for malformed literals and malformed comments.

### 20.2 Parser Tests

- [ ] Add positive parse tests for every major declaration kind.
- [ ] Add positive parse tests for every major expression family.
- [ ] Add negative parse tests for malformed syntax.
- [ ] Add parser recovery tests where possible.

### 20.3 Semantic Tests

- [ ] Add tests for symbol resolution.
- [ ] Add tests for overload resolution.
- [ ] Add tests for access control.
- [ ] Add tests for constructor generation.
- [ ] Add tests for inheritance rules.
- [ ] Add tests for interface implementation.
- [ ] Add tests for `fit`, `is`, `match`, `when`, `ref`, and `view`.

### 20.4 Codegen Tests

- [ ] Snapshot generated C++ for representative programs.
- [x] Add behavioral end-to-end tests that compile and run produced executables.
- [~] Separate language-failure tests from backend-compiler-failure tests.
  - [x] Cover the first backend compile-error and link-error cases explicitly.
  - [ ] Make the separation exhaustive and clearly documented.

### 20.5 Negative Test Suite

- [~] Build a dedicated invalid-program corpus.
  - [x] Land the first real invalid corpus covering parser/sema/interop/generic/
        loop/export failures.
- [ ] Ensure every language rule has at least one failing test.
- [ ] Ensure diagnostics are asserted, not just failure exit codes.

### 20.6 Spec-to-Test Traceability

- [ ] Link each spec section to concrete tests.
- [ ] Refuse to mark a feature stable until docs, positive tests, and negative
      tests all exist.

---

## 21. Documentation

### 21.1 Language Reference

- [ ] Continue expanding the draft into a true full reference.
- [ ] Add more examples per feature family.
- [ ] Add explicit edge-case sections throughout.
- [ ] Add design rationale notes only where they help future decisions.

### 21.2 Tutorials

- [ ] Write a getting-started guide.
- [ ] Write an object model tutorial.
- [ ] Write a references and `fit` tutorial.
- [ ] Write a modules tutorial once imports are stable.

### 21.3 Style Guide

- [ ] Write recommended coding style for Wio: naming, access modifier placement,
      attribute ordering, constructor conventions, and module layout.

### 21.4 Evolution Notes

- [ ] Keep a changelog of language-level decisions.
- [ ] Track feature promotions from partial to stable.
- [ ] Track removed or renamed features explicitly.

---

## 22. Reserved and Future Features

These should stay in the parking lot until the core language is stable.

### 22.1 Concurrency

- [ ] Design `async`/`await` semantics only after function and type semantics
      are already stable.
- [ ] Decide whether async is coroutine-based, thread-based, scheduler-based, or
      runtime-pluggable.
- [ ] Decide what `yield` means before exposing it.

### 22.2 Scheduling / Time Keywords

- [ ] Design `every`, `after`, `during`, and `wait` only after control-flow
      semantics are mature.
- [ ] Decide whether these belong in core language syntax or a library/runtime
      DSL.

### 22.3 `loop`

- [ ] Decide whether `loop` replaces `while`, complements it, or should be
      removed.

### 22.4 `system` and `program`

- [ ] Define whether these are high-level application constructs, ECS/gameplay
      constructs, or compilation-unit declarations.
- [ ] Do not lock syntax for them until Wio's identity is clearer.

---

## 23. Suggested Execution Order

The active execution order now lives in `ORDERED_TODOLIST.md`.

At a high level, the next sequence is:

1. Truth-audit this backlog against the shipped repo surface.
2. Stabilize parser/sema correctness and diagnostics.
3. Freeze the semantics of shipped language features.
4. Strengthen test coverage and spec-to-test traceability.
5. Harden std/runtime/host ABI around the features already in the repo.
6. Improve backend quality, portability, and generated-C++ debuggability.
7. Harden the project model, packaging, CLI, and examples into a stable alpha.
8. Expand the docs from draft/reference fragments into a status-marked manual.
9. Keep future features in the parking lot until the above is solid.

---

## 24. Shortlist for the Very Next Milestone

The next realistic milestone should be:

- [~] Backlog truth audit and roadmap cleanup.
  - [x] Stop treating already-landed loops, generics, std, SDK, and project
        model work as untouched backlog.
  - [ ] Continue cleaning detailed section status markers as the backlog evolves.
- [ ] Tighten parser/sema correctness and crash-proofing.
- [ ] Improve diagnostic quality and make invalid-program tests assert the
      diagnostics themselves.
- [ ] Finish the first real semantic-freeze pass for:
      `use`, `as`, `realm`, `ref`, `view`, `fit`, `match`, attributes, and ctor
      behavior.
- [ ] Freeze the collection contract around arrays, static arrays, dicts, trees,
      strings, and iteration.
- [ ] Keep hardening the landed host/runtime/std/project-model surface rather
      than jumping to concurrency, reflection, or new headline syntax.

That should move Wio from "feature-rich prototype" toward a more trustworthy
and internally coherent alpha.
