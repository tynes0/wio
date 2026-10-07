# Wio Built-in Method Reference

This document lists member methods recognized directly by the compiler whose
bodies or declarations are not visible in std `.wio` sources. It describes the
semantic signatures in the current source tree, not planned APIs or a
support/parity guarantee for every backend.

Source-defined methods on `std::Vector`, `std::Span`, `std::Option`,
`std::Result`, buffers, pools and other std types are outside this catalog.
See the [std reference](./WIO_STD.md) and the corresponding `std/*.wio` sources
for those APIs.

The accepted [v1 surface-consolidation plan](./WIO_V1_SURFACE_CONSOLIDATION_PLAN.md)
will replace implicit Option returns in standard checked operations with Result
and align fallible APIs with `!()`/`?()`. That migration is pending; the tables
below describe current implementation, not the proposed replacement signatures.

## Contents

- [Reading conventions](#reading-conventions)
- [string](#string)
- [text](#text)
- [Arrays: T[] and [T; N]](#array)
- [Dictionaries: Dict and Tree](#dictionary)
- [enum](#enum)
- [flagset](#flagset)
- [Async: coroutine](#async)
- [Generic/value pack members](#pack)
- [Examples](#examples)
- [Sources and maintenance](#sources-and-maintenance)

<a id="reading-conventions"></a>

## Reading conventions

- Signatures use `Method(parameter: Type) -> ReturnType`.
  Parameter names are descriptive labels, not a claim of named-argument support.
- Method names are case-sensitive: `ToI32`, `Count`, `GetOr`, etc.
- `void` indicates that no result value is returned.
- `T`: element type; `K`: key type; `V`: dictionary value type;
  `N`: fixed element count; `A`: the receiver's exact array type;
  `D`: the receiver's exact dictionary type; `E`: enum type; `F`: flagset type;
  `U`: the enum's declared underlying integer type.
- Methods in **mutable receiver required** tables require a mutable receiver.
  They cannot be called through a read-only `view` or an immutable variable.
  `ref`/`view`/aliases do not introduce a separate method set; the underlying
  type and access permissions determine availability. Nullable receivers must
  first be narrowed to non-null.
- `Get` and `At` are different: `Get` represents a missing value with
  `std::Option`, whereas `At` requires a value to exist. `GetOr` returns the
  supplied fallback when no value exists. Signatures returning `Option` require
  the `std::Option` declaration to be available; disabling builtin std does not
  remove this dependency.

<a id="string"></a>

## string

`string` is a byte string. It can carry UTF-8 text, but `Count`, indexing,
`Slice`, `First`/`Last`, `Reverse` and `char` operations work on bytes, not
Unicode characters or graphemes. Use `text` for Unicode-aware operations.

### Reading and searching — no mutable receiver required

| Signature | Description |
| --- | --- |
| `Count() -> usize` | Number of bytes. |
| `Empty() -> bool` | Whether the string is empty. |
| `Contains(value: string) -> bool` | Whether a substring exists. |
| `Contains(value: char) -> bool` | Whether a byte/char exists. |
| `ContainsChar(value: char) -> bool` | Explicitly named char search. |
| `StartsWith(prefix: string) -> bool` | Prefix check. |
| `EndsWith(suffix: string) -> bool` | Suffix check. |
| `IndexOf(value: string) -> isize` | Byte index of the first match, or `-1`. |
| `IndexOf(value: char) -> isize` | Index of the first char match, or `-1`. |
| `LastIndexOf(value: string) -> isize` | Byte index of the last match, or `-1`. |
| `LastIndexOf(value: char) -> isize` | Index of the last char match, or `-1`. |
| `IndexOfChar(value: char) -> isize` | First char index, or `-1`. |
| `LastIndexOfChar(value: char) -> isize` | Last char index, or `-1`. |
| `First() -> char` | First byte; an empty string is an error. |
| `Last() -> char` | Last byte; an empty string is an error. |
| `Get(index: usize) -> std::Option<char>` | `Some` for a valid index, otherwise `None`. |
| `At(index: usize) -> char` | Direct byte access; an invalid index is an error. |
| `GetOr(index: usize, fallback: char) -> char` | Fallback char for an invalid index. |

### Operations producing new values — no mutable receiver required

| Signature | Description |
| --- | --- |
| `Slice(start: usize) -> string` | From the starting index to the end. |
| `Slice(start: usize, count: usize) -> string` | At most `count` bytes; the second argument is not an end index. |
| `SliceFrom(start: usize) -> string` | From the starting index to the end. |
| `Take(count: usize) -> string` | At most `count` bytes from the beginning. |
| `Skip(count: usize) -> string` | Skips bytes at the beginning. |
| `Left(count: usize) -> string` | At most `count` bytes from the left. |
| `Right(count: usize) -> string` | At most `count` bytes from the right. |
| `Trim() -> string` | Removes whitespace at both ends. |
| `TrimStart() -> string` | Removes leading whitespace. |
| `TrimEnd() -> string` | Removes trailing whitespace. |
| `ToLower() -> string` | Byte-based lowercase conversion, not full Unicode case folding. |
| `ToUpper() -> string` | Byte-based uppercase conversion. |
| `Replace(oldValue: string, newValue: string) -> string` | Replaces matching substrings. |
| `ReplaceFirst(oldValue: string, newValue: string) -> string` | Replaces the first match. |
| `Repeat(count: usize) -> string` | Repeated string. |
| `Split(separator: string) -> string[]` | Splits on the separator. |
| `Lines() -> string[]` | Splits into lines. |
| `PadLeft(width: usize) -> string` | Pads with spaces on the left to at least the requested byte width. |
| `PadLeft(width: usize, fill: char) -> string` | Pads on the left with the supplied char. |
| `PadRight(width: usize) -> string` | Pads with spaces on the right. |
| `PadRight(width: usize, fill: char) -> string` | Pads on the right with the supplied char. |
| `Reversed() -> string` | New string with its byte order reversed. |

Slice/take/skip operations clamp to the available length; they do not create
extra elements. Cutting through a UTF-8 code point or reversing byte order can
turn valid Unicode text into invalid UTF-8 bytes.

### Type conversions — no mutable receiver required

| Parameterless signature | Overload accepting a base |
| --- | --- |
| `ToI8() -> i8` | `ToI8(base: i32) -> i8` |
| `ToI16() -> i16` | `ToI16(base: i32) -> i16` |
| `ToI32() -> i32` | `ToI32(base: i32) -> i32` |
| `ToI64() -> i64` | `ToI64(base: i32) -> i64` |
| `ToU8() -> u8` | `ToU8(base: i32) -> u8` |
| `ToU16() -> u16` | `ToU16(base: i32) -> u16` |
| `ToU32() -> u32` | `ToU32(base: i32) -> u32` |
| `ToU64() -> u64` | `ToU64(base: i32) -> u64` |
| `ToISize() -> isize` | `ToISize(base: i32) -> isize` |
| `ToUSize() -> usize` | `ToUSize(base: i32) -> usize` |

| Signature | Description |
| --- | --- |
| `ToF32() -> f32` | Floating-point parsing. |
| `ToF64() -> f64` | Floating-point parsing. |
| `ToBool() -> bool` | Boolean parsing. |

Parameterless integer conversions use base `10`. These methods do not return
`Option`/`Result`; invalid parsing or an out-of-range value is an error.
For checked parsing APIs, see the source-defined `std::convert` surface.

### In-place mutation — mutable receiver required

| Signature | Description |
| --- | --- |
| `Append(value: string) -> void` | Appends a string. |
| `Push(value: char) -> void` | Appends one byte. |
| `Insert(index: usize, value: string) -> void` | Inserts at the supplied position. |
| `Erase(start: usize, count: usize) -> void` | Removes the supplied range. |
| `Clear() -> void` | Empties the string. |
| `Reverse() -> void` | Reverses byte order in place. |
| `ReplaceInPlace(oldValue: string, newValue: string) -> void` | Replaces matches in place. |
| `TrimInPlace() -> void` | Removes whitespace at both ends in place. |
| `ToLowerInPlace() -> void` | Lowercase conversion in place. |
| `ToUpperInPlace() -> void` | Uppercase conversion in place. |

No built-in `Reserve` or `Capacity` method is registered for `string`.

<a id="text"></a>

## text

`text` is a validated UTF-8 Unicode value; its literal form is `u"Hello 🌍"`.
None of the following methods require a mutable receiver.

| Signature | Description |
| --- | --- |
| `Count() -> usize` | Number of Unicode code points. |
| `ByteCount() -> usize` | UTF-8 storage size. |
| `Empty() -> bool` | Whether the text is empty. |
| `Slice(start: usize) -> text` | From the code-point index to the end. |
| `Slice(start: usize, count: usize) -> text` | Slice by code-point index and count. |
| `ToString() -> string` | Produces a UTF-8 byte string. |
| `Utf8() -> string` | Same built-in conversion as `ToString`. |
| `Contains(value: text) -> bool` | Subtext search. |
| `StartsWith(prefix: text) -> bool` | Prefix check. |
| `EndsWith(suffix: text) -> bool` | Suffix check. |
| `GraphemeCount() -> usize` | Number of grapheme clusters. |
| `SliceGraphemes(start: usize, count: usize) -> text` | Slice by grapheme index and count. |
| `DisplayWidth() -> usize` | Display-column calculation using Unicode width rules, not font pixel width. |
| `CaseFold() -> text` | Unicode case folding. |
| `CodePoints() -> u32[]` | Numeric code-point values. |
| `Graphemes() -> text[]` | Grapheme fragments. |

Additional access: `value[index]` returns a read-only, single-code-point
**`text`**, not a `u32` or `char`. Use `CodePoints()` for numeric code points.
Code points and graphemes are different: combining marks and some emoji
sequences contain multiple code points. Currently, `text` has no built-in
`Get`, `At`, `Append`, `Trim` or `ToLower` method.

<a id="array"></a>

## Arrays: T[] and [T; N]

`T[]` is a dynamic array; `[T; N]` is a fixed-size array. Below, `A` denotes
the receiver's exact type. For example, on `[i32; 3]`,
`Clone() -> [i32; 3]` whereas `Slice(...) -> i32[]`.

### Shared by both array types — no mutable receiver required

| Signature | Requirement / description |
| --- | --- |
| `Count() -> usize` | Number of elements. |
| `Empty() -> bool` | Whether the array is empty. |
| `Capacity() -> usize` | Dynamic capacity; `N` for a fixed-size array. |
| `Contains(value: T) -> bool` | Requires intrinsic equality support for `T`. |
| `IndexOf(value: T) -> isize` | Requires equality support; first index, or `-1`. |
| `LastIndexOf(value: T) -> isize` | Requires equality support; last index, or `-1`. |
| `First() -> T` | First element; an empty array is an error. |
| `Last() -> T` | Last element; an empty array is an error. |
| `Get(index: usize) -> std::Option<T>` | `Some` for a valid index, otherwise `None`. |
| `At(index: usize) -> T` | An invalid index is an error. |
| `GetOr(index: usize, fallback: T) -> T` | Fallback value for an invalid index. |
| `Clone() -> A` | Copies the array value; does not automatically deep-clone elements. |
| `Slice(start: usize) -> T[]` | Dynamic array from the starting index to the end. |
| `Slice(start: usize, count: usize) -> T[]` | At most `count` elements. |
| `Take(count: usize) -> T[]` | Elements from the beginning. |
| `Skip(count: usize) -> T[]` | Skips elements at the beginning. |
| `Concat(other: A) -> T[]` | Concatenated dynamic array. |
| `Reversed() -> A` | New array with reversed order. |
| `Join(separator: string) -> string` | Only for `T = string`. |
| `Sorted() -> A` | Requires intrinsic ordering support; sorted copy. |

The `Concat` parameter uses the receiver's exact type: for a `[T; N]` receiver,
the other array must also be `[T; N]`. Slice/take/skip operations clamp to the
available elements. These slices return dynamic array values, not
`Span`/borrow views.

### Shared by both array types — mutable receiver required

| Signature | Requirement / description |
| --- | --- |
| `Fill(value: T) -> void` | Changes the values of existing elements. |
| `Reverse() -> void` | Reverses in place. |
| `Sort() -> void` | Requires intrinsic ordering support; sorts in place. |

### Dynamic T[] only — mutable receiver required

| Signature | Description |
| --- | --- |
| `Push(value: T) -> void` | Appends an element. |
| `PushFront(value: T) -> void` | Prepends an element. |
| `Pop() -> T` | Removes and returns the last element; an empty array is an error. |
| `PopFront() -> T` | Removes and returns the first element; an empty array is an error. |
| `Insert(index: usize, value: T) -> void` | Inserts an element at a position. |
| `RemoveAt(index: usize) -> void` | Removes the element at a position. |
| `Remove(value: T) -> bool` | Requires equality support; removes the first match and reports whether removal occurred. |
| `Clear() -> void` | Empties the array. |
| `Extend(other: T[]) -> void` | Appends the other array's elements. |
| `Reserve(capacity: usize) -> void` | Reserves capacity without changing the element count. |
| `ShrinkToFit() -> void` | Requests a reduction of excess capacity. |

### Equality and ordering restrictions

For equality, the current resolver accepts primitives other than `void`/unknown
types, `null`, generic parameters and arrays/dictionaries whose elements also
satisfy these requirements. For ordering, it accepts primitives and generic
parameters. Accepting a generic placeholder does not guarantee runtime support
for every concrete type.

This gate does not automatically admit every user-defined
`component`/`object`/enum. Defining a comparison operator alone does not change
the intrinsic resolver's restrictions. There is no built-in `Sort` overload
accepting a comparator.

<a id="dictionary"></a>

## Dictionaries: Dict<K, V> and Tree<K, V>

`Dict<K, V>` is an unordered dictionary; `Tree<K, V>` is an ordered dictionary.
`D` denotes the receiver's exact type; `Merge`/`Extend` take the same
dictionary type.

### Shared — no mutable receiver required

| Signature | Description |
| --- | --- |
| `Count() -> usize` | Number of entries. |
| `Empty() -> bool` | Whether the dictionary is empty. |
| `ContainsKey(key: K) -> bool` | Whether the key exists. |
| `ContainsValue(value: V) -> bool` | Requires intrinsic equality support for `V`. |
| `Get(key: K) -> std::Option<V>` | `Some` if the key exists, otherwise `None`. |
| `At(key: K) -> V` | A missing key is an error. |
| `GetOr(key: K, fallback: V) -> V` | Fallback value for a missing key. |
| `TryGet(key: K, outValue: ref V) -> bool` | Writes to the output and returns `true` if found; otherwise leaves the output unchanged. |
| `Keys() -> K[]` | Dynamic array of keys. |
| `Values() -> V[]` | Dynamic array of values. |
| `Clone() -> D` | Dictionary copy; does not deep-clone object handles stored as values. |
| `Merge(other: D) -> D` | Merged copy; `other` wins on duplicate keys. |

`TryGet` does not mutate the receiver dictionary, but its `outValue` argument
must be a mutable reference. Key/value iteration order is not guaranteed for
`Dict`; `Tree` uses key order.

### Shared — mutable receiver required

| Signature | Description |
| --- | --- |
| `Set(key: K, value: V) -> void` | Inserts or replaces a value. |
| `GetOrAdd(key: K, fallback: V) -> V` | Returns an existing value, or inserts and returns the fallback. |
| `Extend(other: D) -> void` | Merges in place; the other value wins on duplicate keys. |
| `Clear() -> void` | Removes all entries. |
| `Remove(key: K) -> bool` | Removes the key and reports whether removal occurred. |

### Tree<K, V> only — no mutable receiver required

| Signature | Description |
| --- | --- |
| `FirstKey() -> K` | Smallest key; an empty Tree is an error. |
| `FirstValue() -> V` | Value of the smallest key; an empty Tree is an error. |
| `LastKey() -> K` | Largest key; an empty Tree is an error. |
| `LastValue() -> V` | Value of the largest key; an empty Tree is an error. |
| `FloorKeyOr(key: K, fallback: K) -> K` | Largest key less than or equal to the supplied key, or the fallback. |
| `CeilKeyOr(key: K, fallback: K) -> K` | Smallest key greater than or equal to the supplied key, or the fallback. |

<a id="enum"></a>

## enum

Called on enum values; no mutable receiver is required.

| Signature | Description |
| --- | --- |
| `Name() -> string` | Name of the enum value. |
| `Value() -> U` | Value in the enum's underlying integer type. |
| `IsValid() -> bool` | Whether this is one of the declared enum values. |

`Value` does not always return `i32`; it uses the enum's declared underlying
type. Reflection APIs used on the enum type are a separate std reflection
surface, not substitutes for these instance methods.

<a id="flagset"></a>

## flagset

Called on flagset values. None of these operations require a mutable receiver.

| Signature | Description |
| --- | --- |
| `Name() -> string` | Name representation of the flagset. |
| `Has(flags: F) -> bool` | Whether all supplied bits are present. |
| `HasAny(flags: F) -> bool` | Whether at least one supplied bit is present. |
| `With(flags: F) -> F` | New value with the bits added. |
| `Without(flags: F) -> F` | New value with the bits removed. |
| `Toggle(flags: F) -> F` | New value with the supplied bits toggled. |
| `Clear() -> F` | New value with all bits cleared. |

Important: flagset `Clear()` **does not mutate the receiver**. Unlike
array/string/dictionary `Clear()`, it is not a mutating method returning
`void`. To change the receiver, assign the returned value back to it.
The enum methods `Value()`/`IsValid()` are not registered in this flagset
intrinsic list.

<a id="async"></a>

## Async: coroutine<T> and coroutine<void>

Timeout arguments are measured in **milliseconds**. The following methods do
not require a `mut` receiver; operations such as `Start`/`Cancel` can affect
shared task state. Not requiring `mut` does not mean these operations are pure.

| Signature: coroutine<T> | Description |
| --- | --- |
| `Start() -> coroutine<T>` | Starts the task and returns the task handle. |
| `Cancel() -> void` | Requests cancellation; does not forcibly terminate a thread. |
| `IsReady() -> bool` | Whether the task has completed. |
| `IsCancelled() -> bool` | Cancellation state. |
| `IsFaulted() -> bool` | Whether the task completed with a failure. |
| `WaitFor(milliseconds: u64) -> bool` | Timed wait on the calling thread; `true` if ready. |
| `Block() -> T` | Blocks the calling thread until the result is available. |
| `Poll() -> std::async::TaskPoll<T>` | Retrieves status, optional result and failure information without waiting. |
| `Within(milliseconds: u64) -> coroutine<std::Option<T>>` | Async operation producing `Some` on success or `None` on timeout. |
| `CancelAfter(milliseconds: u64) -> coroutine<void>` | Async operation requesting cancellation after the delay if the task is still incomplete. |
| `Detach() -> void` | Leaves the task running detached. |

For `coroutine<void>`, `Start`, `Block`, `Poll` and `Within` return the types
below. All other methods have the same parameter and return types as above:

| Signature: coroutine<void> | Description |
| --- | --- |
| `Start() -> coroutine<void>` | Starts the void task. |
| `Block() -> void` | Blocks until completion without producing a value. |
| `Poll() -> std::async::VoidTaskPoll` | Status and failure information without a result value. |
| `Within(milliseconds: u64) -> coroutine<bool>` | `true` if completed within the timeout, otherwise `false`. |

`Poll` and `Within` require the corresponding `std::async` declarations;
value-producing `Within` also requires `std::Option`. On timeout, `Within`
requests cancellation of the source task. Failure/cancellation is not the same
as a successful `None` result. `Within`/`CancelAfter` return tasks; do not confuse
them with synchronous `WaitFor`. Returned tasks follow the normal async
start/await rules; calling the method alone does not imply that the timer has
already started in every situation.

<a id="pack"></a>

## Generic/value pack members

This surface is different from ordinary array methods. Two **properties** and
one method are recognized specially on packs:

| Member | Parameters / result | Requirement |
| --- | --- | --- |
| `size` | No arguments; `usize`. | Generic/value/type packs and pack storage. |
| `array` | No arguments; a value-pack view or type-pack view. | Value view for value packs, type view for type packs. |
| `ToStaticArray<T>() -> [T; N]` | One explicit generic element type; no runtime arguments. | Value packs/views/storage; `N` is the pack element count. |

Use `args.size` and `args.array`, not `size()`/`array()`.
`array` can be a heterogeneous pack view; it does not automatically mean
`T[]`. `ToStaticArray<T>()` requires exactly one generic type argument.
Converting a type pack to a runtime array is unsupported. Values must also be
compatible with the target element type.

<a id="examples"></a>

## Examples

### string: copying versus in-place mutation

```wio
mut title: string = "  Wio  ";
let trimmed: string = title.Trim(); // title is unchanged
title.TrimInPlace();                // title is modified
title.Append(" language");

let number: i32 = "42".ToI32();
let hex: u32 = "ff".ToU32(16);
let maybeByte: std::Option<char> = title.Get(999usize);
let fallback: char = title.GetOr(999usize, '?');
```

### Dynamic and fixed-size arrays

```wio
mut values: i32[] = [3, 1, 2];
values.Push(4);
values.Sort();
let first: std::Option<i32> = values.Get(0usize);
let copied: i32[] = values.Slice(1usize, 2usize);

let fixed: [i32; 3] = [3, 1, 2];
let sorted: [i32; 3] = fixed.Sorted();
let dynamic: i32[] = fixed.Take(2usize);
// fixed.Push(4); // fixed-size arrays cannot be resized
```

### Dictionary: optional versus direct access

```wio
mut scores: Dict<string, i32>;
scores.Set("Ada", 42);
let maybeScore: std::Option<i32> = scores.Get("missing");
let knownScore: i32 = scores.At("Ada");
let fallbackScore: i32 = scores.GetOr("missing", 0);

mut output: i32 = 0;
let found: bool = scores.TryGet("Ada", ref output);
```

### Unicode: bytes, code points and graphemes

```wio
let label: text = u"Merhaba 🌍";
let points: usize = label.Count();
let bytes: usize = label.ByteCount();
let globe: text = label.Slice(8usize, 1usize);
let codes: u32[] = label.CodePoints();
let pieces: text[] = label.Graphemes();
let utf8: string = label.Utf8();
```

<a id="sources-and-maintenance"></a>

## Sources and maintenance

This catalog was prepared by reading the following sources together, not by
guessing from std sources. In particular, the resolver's initial types for
`Get`/`Poll`/`Within` are not the final user-facing signatures; the analyzer
completes them.

- [Intrinsic member resolver](../compiler/src/wio_compiler/sema/intrinsic_member_resolver.cpp):
  method names, overload parameters, return types and mutability requirements.
- [Access/call analysis](../compiler/src/wio_compiler/sema/detail/access_and_call_visitors.inl):
  `Option` returns, task results, pack members and access checks.
- [Intrinsic identities](../compiler/include/wio/ast/ast.h): intrinsic inventory.
- [Runtime intrinsic helpers](../runtime/include/intrinsics.h):
  slicing, searching, mutation and parsing behavior.
- [Unicode text runtime](../runtime/include/text.h):
  byte/code-point/grapheme distinctions.
- [Async std surface](../std/async.wio) and
  [async runtime](../runtime/include/std_async.h):
  polling, timeouts, cancellation and waiting.
- [Conversion std surface](../std/convert.wio):
  source-defined conversions including `std::ToString<T>(value)`.
  Do not assume that every type has a built-in `.ToString()`.

Update the corresponding table whenever a new intrinsic or overload is added.
