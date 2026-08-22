# Wio 0.13.0 Release Notes

Released: 2026-08-22

Wio 0.13 is the language-coherence release. It freezes the rules that make
Unicode text, compile-time text, generic specialization, attributes,
extensions, matching, reflection, generated C++, and the public host SDK agree
with each other.

## Highlights

- `text`, `u"..."`, and `u$"..."` are first-class Unicode language features.
- `const string` and `const text` work as declarations and generic arguments.
- `[T; _]` infers fixed-array extents from its initializer.
- Compact typed attributes support named arguments and folded constants.
- Generic component extensions support deduction, explicit arguments,
  defaults, and `where` constraints.
- Partial specialization ordering and guarded/exhaustive matching are
  deterministic and specified.
- The C++ SDK reports Wio product version 0.13.0 and module ABI descriptor v7.
- The SDK includes host values for text, Option/Result, tuples, queues, sets,
  spans, buffers, pools, Box, any, and nullable values.
- Shared modules expose stable type IDs, concrete generic arguments, owned
  inspection snapshots, and typed/dynamic Unicode text fields.
- The compiler, runtime, std, CLI, SDK, VS Code extension, documentation, and
  release manifest now use the same release version.

## Source examples

```wio
const Greeting: text = u"Merhaba 🌍";
const Key: string = "primary";

fn Describe<const Name: text>() -> text {
    return Name;
}

let values: [i32; _] = [3, 1, 4, 1, 5];
```

```wio
attribute route(fn)(
    method: string,
    path: string = "/"
) with attribute::runtime;

fn Health() -> string
    with route(method: "GET", path: "/health") {
    return "ok";
}
```

```wio
component Point {
    x: f32;
    y: f32;
}

extension PointMath for Point {
    fn Scale<T>(self: ref Point, factor: T) where T: Numeric {
        self.x = self.x * factor;
        self.y = self.y * factor;
    }
}
```

## Compatibility

Canonical native and attribute metadata uses `with` and `using`, including
`using cpp::header("example.h");`. Legacy `@Attribute(...)` source remains
accepted in 0.13 for migration. Native function/component const parameters
remain integer-only; textual const generics are currently a Wio-owned
language feature and do not cross that ABI.

Module hosts must negotiate descriptor size and capabilities before accessing
ABI v7 fields. Product version and module ABI revision are intentionally
independent compatibility dimensions.

## Validation contract

The release gate covers Windows and Ubuntu, generated-C++ validation, changed
language positive/negative tests, SDK shared-module conformance, package smoke
tests, and installation-qualified SDK compilation. Remaining v1 work is kept
explicit in [`../TODOLIST.md`](../TODOLIST.md) and scheduled in
[`WIO_V1_RELEASE_PLAN.md`](./WIO_V1_RELEASE_PLAN.md).
