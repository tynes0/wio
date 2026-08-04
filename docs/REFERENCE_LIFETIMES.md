# Reference and view lifetimes

Wio has two non-owning reference types:

- `ref T` is a mutable borrow of a `T` value.
- `view T` is a read-only borrow of a `T` value.

Neither type owns or extends the lifetime of the value it points to. Object values remain owning runtime handles; component values remain stack-friendly values.

Borrow nullability is explicit. `ref T?` means a borrow of nullable storage,
whereas `(ref T)?` means that the borrow itself may be null. The same distinction
applies to `view`. A nullable borrow must be narrowed before it is read, and a
borrow of nullable storage observes later assignments to that storage.

## Borrow origins

The semantic analyzer tracks where every borrow originates:

- **static**: a global value whose lifetime covers the program;
- **caller**: `self`, a reference parameter, or a borrow derived from one;
- **local**: a local variable or value parameter;
- **temporary**: a constructor result or another temporary expression.

Member and array-element borrows inherit the origin of their containing value. A reference-returning method inherits the origin of its receiver. A free function returning a reference inherits the origin of its first reference argument; a function without reference arguments may only return a static borrow.

Local and temporary borrows cannot be returned because their owner would be destroyed before the caller could use them. A borrow of a temporary cannot be stored or assigned to a longer-lived reference variable. It may still be consumed during the same complete expression, such as a read-only extension call on a temporary component.

## `self` and `deref self`

Inside an object method or component extension, `self` is borrowed from the caller:

- returning `self` from a `ref`/`view` function returns that borrow;
- `deref self` in an object method produces the owning object handle;
- `deref self` in a component extension produces a component value copy.

Consequently, an owning return type uses `return deref self;`, while a `ref` or `view` return type uses `return self;`.

## Native boundary

Native functions may receive `ref` and `view` arguments. Those borrows are call-scoped and native code must not retain them. Native `ref`/`view` return values are rejected because Wio cannot prove the lifetime of memory owned by native code. A future explicit native-lifetime annotation can safely relax this rule; until then, return an owning value or an opaque handle.
