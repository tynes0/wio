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

---

## 1. Project Priorities

### P0 - Must become stable before the language grows

- [ ] Freeze the core language direction before adding many more keywords.
- [ ] Make the compiler trustworthy on valid input and graceful on invalid
      input.
- [ ] Eliminate parser crashes, undefined behavior, and fragile assumptions.
- [ ] Make semantic analysis stricter than codegen, not weaker than codegen.
- [ ] Make every documented feature either implemented, clearly partial, or
      removed from user-facing docs until ready.
- [ ] Stop adding surface syntax faster than the test suite can cover it.

### P1 - Make Wio usable as a real alpha language

- [ ] Finish the module/import system.
- [ ] Finish loop support.
- [ ] Stabilize `ref`, `view`, `fit`, `is`, `match`, attributes, and ctor
      rules.
- [ ] Improve diagnostics so language errors feel deliberate and readable.
- [ ] Make backend generation reproducible and less Windows-specific.
- [ ] Expand the standard library beyond the current `std::io` bootstrap state.
- [ ] Keep the compiler/runtime shape compatible with embeddable static/shared
      libraries, host-driven scripting, and future hot reload.

### P2 - Make Wio pleasant to use

- [ ] Improve CLI ergonomics.
- [ ] Improve debug output (`tokens`, `AST`, `typed AST`, generated C++).
- [ ] Write tutorials and example programs, not just a reference manual.
- [ ] Build confidence with positive and negative test suites.

### P3 - Long-horizon features

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
- [ ] Design generic type declarations for `object`, `component`, `interface`,
      `enum`, and aliases.
- [ ] Broaden the first generic type-declaration slice beyond aliases:
      generic `component`, generic `object`, and generic `interface`
      declarations with explicit type arguments at use sites.
- [ ] After the first generic struct slice lands, decide whether constructor
      calls may deduce generic type arguments or whether explicit
      `Box<i32>(...)`-style construction stays required.
- [ ] Define runtime type identity for generic objects/interfaces so
      specialization-aware `is`, `fit`, export metadata, and hot-reload lookup
      stay sound.
- [ ] Extend generic functions with explicit type arguments at call sites when
      inference is insufficient.
- [ ] Support generic methods after object/component/interface method lowering
      rules are clear.
- [ ] Decide whether Wio should ever support front-end monomorphization in
      addition to backend C++ template lowering.
- [ ] Define generic constraint syntax, if any, before exposing advanced type
      relations.
- [ ] Broaden overload resolution tests for generic-vs-concrete preference,
      ambiguous generic overloads, and mixed namespace/module calls.
- [ ] Decide whether generic `@Native` functions should:
      stay unsupported,
      map only to externally instantiated C++ templates,
      or require explicit Wio-side instantiation syntax.
- [ ] Decide whether generic `@Export` functions should:
      stay forbidden for ABI stability,
      require explicit exported instantiations,
      or expose a generated monomorphic wrapper list.
- [ ] Short-term rule: keep generic `@Native` and generic `@Export`
      deliberately rejected until an explicit instantiation surface exists.
- [ ] Design a source-level explicit-instantiation surface for interop, such as
      a future `instantiate` declaration or attribute-driven instance list.
- [ ] Decide whether exported generic instances must have explicit stable C ABI
      names via something like `@CppName`/`@ExportAs`.
- [ ] If generic interop is ever allowed, define how explicit instantiation,
      symbol naming, and host ABI stability interact.
- [ ] Decide whether generic defaults and partial specialization will ever
      exist.
- [ ] Add end-to-end examples such as `Array<T>`, `Result<T>`, `Pair<K, V>`,
      generic utility functions, and container-oriented generic loops once the
      rest of the surface is frozen.

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
- [ ] Define `len`, `contains`, `push`, `remove`, `clear`, `keys`, `values`,
      and iterator access.

### 8.5 Destructuring and Iteration

- [ ] Design `for` syntax.
- [ ] Design `foreach` syntax if it remains distinct from `for`.
- [ ] Decide whether `for (x in arr)` and `foreach (x in arr)` both exist.
- [ ] Add support for `in` loops over arrays.
- [ ] Add support for `in` loops over dictionaries.
- [ ] Design destructuring forms such as:
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

This is currently one of the biggest blockers to real multi-file projects.

### 16.1 `use`

- [ ] Finish semantic resolution for `use`.
- [ ] Guarantee deterministic module lookup behavior.
- [ ] Define relative vs absolute import semantics.
- [ ] Define error messages for missing modules and duplicate imports.

### 16.2 `as`

- [ ] Finish `use ... as ...` aliasing support.
- [ ] Define shadowing behavior for aliases.
- [ ] Define whether aliases can rename modules, namespaces, imported types, and
      imported functions.

### 16.3 Import Path Forms

- [ ] Finalize support for `use std::io;`, `use self::foo;`, `use super::bar;`,
      and relative multi-segment paths.
- [ ] Decide whether wildcard imports will ever exist.

### 16.4 Multi-File Semantics

- [ ] Decide whether compilation is file-merging, module-compilation, or
      package-based.
- [ ] Define symbol visibility across files.
- [ ] Detect and report circular imports cleanly.
- [ ] Decide how the compiler should cache previously loaded modules.
- [ ] Add end-to-end tests for multi-file projects.

### 16.5 Realm and Namespace Semantics

- [ ] Treat `realm` as the explicit Wio surface keyword for namespace-like
      scopes.
- [ ] Decide whether modules implicitly create realms, remain flat, or can do
      both.
- [ ] Clarify how `::` lookup works for realms, std modules, imported modules,
      enums, flagsets, and nested declarations.
- [ ] Define whether nested `realm` declarations are fully supported and how
      name mangling stays stable across them.
- [ ] Add positive and negative tests for realm collisions, nested realm lookup,
      and realm-qualified type references.

---

## 17. Standard Library and Runtime

### 17.1 Runtime Foundation

- [ ] Document the runtime ABI expected by generated C++.
- [ ] Separate runtime concerns from compiler concerns more cleanly.
- [ ] Decide what is guaranteed to exist in every Wio program.

### 17.2 `std::io`

- [ ] Formalize the API of `std::io`.
- [ ] Decide whether `Print` is overloaded, variadic, or formatting-based only.
- [ ] Add tests for string interpolation through `Print`.
- [ ] Decide what input APIs should exist.

### 17.3 Core Library Modules

- [ ] Design a minimal `std` layout:
  - `std::io`
  - `std::math`
  - `std::mem`
  - `std::time`
  - `std::collections`
  - `std::strings`
  - `std::reflect`
- [ ] Decide which of these are language-critical versus optional.

### 17.4 Collections Library

- [ ] Expose array helpers.
- [ ] Expose dictionary/tree helpers.
- [ ] Expose pair/iterator concepts if loops depend on them.

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

- [ ] Add a first-class distinction between executable builds and library
      builds.
- [ ] Support static library output for embedding Wio-generated code into larger
      C++ projects.
- [ ] Support shared/dynamic library output for host-driven scripting workflows.
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

- [ ] Clean up CMake options and document them.
- [ ] Make app/library/test targets predictable.
- [ ] Ensure tests are runnable from a fresh checkout.

### 19.3 Example Programs

- [ ] Add a curated examples directory:
  - hello world
  - arrays
  - references
  - objects and interfaces
  - attributes
  - modules
  - match
  - dictionaries
- [ ] Ensure every example is compiled in CI/test flows.

### 19.4 Editor Support

- [ ] Plan syntax highlighting.
- [ ] Plan formatter support.
- [ ] Plan LSP support for diagnostics and completion.

---

## 20. Testing Strategy

Wio needs far more tests than new syntax right now.

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
- [ ] Add behavioral end-to-end tests that compile and run produced executables.
- [ ] Separate language-failure tests from backend-compiler-failure tests.

### 20.5 Negative Test Suite

- [ ] Build a dedicated invalid-program corpus.
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

The best next sequence is probably:

1. Stabilize parser and semantic analyzer correctness.
2. Finish diagnostics and negative tests.
3. Finish module/import semantics.
4. Freeze `ref` / `view` / `fit` / `match` / ctor generation behavior.
5. Finish loops and collections.
6. Expand stdlib and examples.
7. Revisit advanced/reserved features only after the above is solid.

---

## 24. Shortlist for the Very Next Milestone

If we want a realistic next milestone, it should include only these:

- [ ] Finish `use` + `as` for real multi-file code.
- [ ] Add `for` / `foreach` with `in`.
- [ ] Finalize dictionary and pair iteration design.
- [ ] Tighten `ref` / `view` semantics and tests.
- [ ] Tighten `fit` numeric and object-cast semantics.
- [ ] Make `match` typing deterministic.
- [ ] Finish semantic enforcement for `@Trust`, `@Final`, and `@ReadOnly`.
- [ ] Add a serious invalid-program test suite.
- [ ] Expand docs with more examples and more formal rules.

This should be enough to move Wio from "interesting prototype" to
"internally credible alpha language."
