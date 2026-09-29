# Wio 0.18.1 Release Notes

Wio 0.18.1 is a focused correctness release for the default Lowered-WIR C++
pipeline introduced in 0.18.0. It restores source programs that were accepted
and generated correctly before Typed WIR became the release path.

## Typed WIR correctness

- `++` and `--` are first-class lexer and parser tokens instead of two adjacent
  unary operators that only happened to form valid generated C++.
- Prefix and postfix increment/decrement preserve their distinct result values
  while mutating the underlying place exactly once.
- Unary `+` now has an explicit Typed WIR lowering.
- Numeric binary operands use the same common-type promotion already accepted
  by semantic analysis, including `i32`/`usize` and `f32`/`f64` comparisons.
- An unqualified call to a method declared later in the same object retains its
  implicit `self` receiver and canonical dispatch metadata.

## Qualification

The hotfix adds focused Typed WIR and executable regressions for mutable places,
prefix/postfix results, forward method calls, numeric promotion, and parser
precedence. The original `console-loading` reproduction builds and starts with
the WIR backend.

The module ABI descriptor remains version 11. This patch changes no public ABI.
