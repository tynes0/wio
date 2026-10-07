# Wio VM Native Bridge

Status: Sprint 20 implementation in progress.

The VM consumes the same wire-v2 `WioNativeAbiRegistry` and concrete native
thunks as the Lowered-WIR C++ backend and host SDK. It does not use a second FFI
contract and does not attempt to reproduce C++ overload, template, or layout
rules at runtime. The compiler resolves those rules and emits one stable native
key and ABI plan before bytecode generation.

## Sprint 20.1 registry and synchronous values

`Machine::registerNativeRegistry(...)` validates and copies every descriptor.
The descriptor array may be released after registration, while the containing
native library and thunk code must remain loaded until the machine can no
longer execute those functions. Registration is idempotent for the same stable
key/thunk pair and rejects rebinding a key to different code.

`NativeInvoke` resolves its external bytecode declaration by stable key and
executes the registered wire-v2 thunk. This slice supports:

- bool, signed/unsigned integer, and floating-point scalars;
- byte-preserving UTF-8 `string` values;
- scalar-valid UTF-32 `text` values;
- nullable wire void only where the Wio declaration is nullable;
- immutable `view` and mutable `ref` tokens over VM places;
- alias-preserving reference tokens and mutable copy-back, including copy-back
  when a native thunk reports a translated failure;
- native failure category, message, and native-code capture;
- deterministic rejection of missing registrations, ABI-version mismatches,
  conflicting stable keys, unsupported marshalling, and incorrect result kinds.

The bytecode verifier requires every `NativeInvoke` to match one external
function declaration, parameter list, result type, and encoded ABI plan.

Current stable diagnostics are:

- `WVM1220`: invalid or unregistered native target;
- `WVM1221`: invocation shape does not match the ABI plan;
- `WVM1222`: native thunk reported failure;
- `WVM1223`: argument conversion or reference validation failed;
- `WVM1224`: result or mutable-reference copy-back conversion failed;
- `WVM1225`: the requested marshalling/passing mode belongs to a later bridge
  slice.

## Sprint 20.2 foreign storage and ownership

The VM now has explicit value kinds for native POD bytes, opaque pointers,
foreign objects, and foreign runtime values. Native POD results are copied into
VM-owned byte storage when the producer has no field adapter. With the additive
POD layout sidecar described below they become ordinary VM components instead.
The other kinds retain the producer-supplied wire-v2
handle operations and release them when the last VM `Value` storage reference
dies. The VM never directly deletes memory created by a native library.

POD, object, and runtime-value ABI plans require reflection metadata with a
non-zero stable type id. The bytecode verifier rejects a missing identity as
`WBC1074`; the bridge checks both the wire value's id and the producer handle's
`typeId` operation before admitting it. Opaque pointers remain pointer-shaped
and may optionally carry a producer owner handle.

Copying a VM `Value` only increments the VM storage reference count. One
producer retain is held by that shared storage, so the final VM copy performs
exactly one matching producer release. Borrowed arguments expose the stored
handle for the duration of the synchronous thunk without transferring
ownership. Owned results are retained before the native frame releases its
wire value.

## Sprint 20.3 callback entry

Scalar Wio function values can now cross a native parameter as wire-v2
callbacks. A callback invocation decodes its scalar arguments, re-enters the
owning machine through the callable's function and captures, and converts the
scalar/void result back to the wire contract. VM errors and conversion failures
are contained at the callback boundary instead of unwinding through native
code.

The bytecode verifier pins callback parameters to function types and validates
the `call`/`retained` lifetime and `caller`/`any` thread enums. Call-scoped
callbacks expire immediately after the native thunk returns, even if native
code incorrectly retained the descriptor. Retained callbacks keep their VM
callable alive, but return `WIO_NATIVE_ABI_STALE_HANDLE` after the owning
machine is gone. Caller-thread callbacks reject foreign entry before trying to
lock or execute the VM; any-thread callbacks serialize entry through the
machine execution lock. The callback ops flags carry this affinity into the
shared C++ wire codec as well.

## Sprint 20.4 native async adaptation

A native function may now return the canonical wire representation of
`coroutine<T>`. Registering its registry together with `WioNativeTaskApi` and
the additive `WioNativeTaskCompletionApi` makes `NativeInvoke` create a normal
VM task rather than exposing a foreign runtime handle. The VM subscribes once,
reads and converts `T` only after terminal completion, and releases the
producer-owned task handle exactly once.

Completion callbacks may arrive synchronously or on any foreign thread. The
callback completes the external VM task; existing VM await machinery then
schedules the suspended Wio coroutine on the executor recorded in its
coroutine state. Native code therefore never resumes a Wio frame directly on
its callback thread. Faults cross as `WVM1226`, and native cancellation maps to
the existing `WVM1165` VM cancellation contract.

Cancellation is bidirectional at the ownership boundary: cancelling an
adapted VM task invokes the producer's task API, while machine teardown
cancels all outstanding foreign tasks before dropping their handles. The
exactly-once settlement guard resolves completion/cancellation races without
double reads or releases. `bindMainExecutor()`, `drainMainExecutor()`, and
`wait()` also bind and pump every registered native task runtime, so tasks
whose producer targets its main executor make progress without a separate host
loop.

`WioNativeTaskCompletionApi` is a separate wire-v2 sidecar instead of an
in-place extension of `WioNativeTaskApi`; modules built before this slice keep
their original binary layout. Such modules can still be loaded and polled by
the SDK, but the VM deterministically rejects async adaptation without the
completion sidecar as `WVM1225`.

## Sprint 20.5 discovery, lifetime, and project execution

`Machine::loadNativeLibrary(path, &error)` loads a shared library and discovers
`WioGetNativeAbiRegistry` plus its optional task/completion sidecars. Paths are
resolved canonically; loading the same module into one machine is idempotent.
Machines share a process-wide library lease. Missing modules, missing registry
exports, incompatible wire versions, and invalid sidecars fail before execution.
Old wire-v2 modules without completion sidecars remain usable for synchronous
calls; async adaptation still requires both task APIs.

Foreign object, runtime-value, and owned opaque storage pins producer code
until its final foreign release. Pending native task adapters also pin their
producer. Destroying a machine therefore cannot unload a DLL underneath an
escaped native value or an outstanding completion callback. The final machine
registration shuts down the producer task runtime on the host teardown
thread while its code lease is still held. Escaped value leases may keep the
DLL loaded longer. Shutdown is not deferred to the final completion callback,
which could otherwise try to unload its own producer's return address.

Program projects can now select the VM explicitly:

```powershell
wio project build --backend vm
wio project run --backend vm -- "two words" --verbose
wio project run --backend vm --no-build
```

The default backend remains `native`. VM builds produce `<baseName>.wiob` and
`<baseName>.vm-native.dll` (`.so`/`.dylib` on other platforms). The bytecode and
native bridge are generated from the same analyzed and lowered program, using
the compiler's `--bytecode-sidecar` output. A failed native compilation does
not publish its staged bytecode. Both outputs participate in incremental
checks, including Wio modules, native sources/headers, the manifest, and the
compiler/runtime inputs. Manifest run arguments, repeated `--arg`, and values
after `--` keep their existing order and are delivered to `main(string[])` or
`Entry(string[])`; async entries are awaited with both main queues pumped.

Additional wire registries and output paths can be configured in the manifest:

```ini
[vm]
bytecodeOutput = ".wio-build/interop/demo.wiob"
bridgeOutput = ".wio-build/interop/demo.vm-native.dll"
nativeLibraries = ["native/lib/plugin.dll"]
```

Paths are relative to the project root. The runner loads the generated bridge
and then the additional libraries before binding the main executor. Native
archive dependencies continue to use `[wio].linkLibraries`; `nativeLibraries`
is only for dynamic modules exporting the Wio wire registry.

This slice deliberately requires `wio.target = "exe"` and
`host.enabled = false`. Hybrid SDK hosts and VM executable/shared/static
packaging are separate artifact work. The generated bridge currently uses the
canonical C++ backend for the full program with main emission disabled; it is
not yet a native-only pruned translation unit, so a cold VM project build still
incurs native C++ compilation. Incremental runs reuse both outputs.

The internal runner protocol is
`wio --vm-run-bytecode FILE [--native-library PATH]... -- [args...]`.
Normal users should use the project commands above.

## Sprint 20.6 native differential qualification

`wio_vm_native_differential` compiles one Wio program as a native executable
and as paired bytecode/shared-bridge artifacts. It executes both with the same
arguments and requires a successful, independently checked result: two equal
failures cannot pass. Successful stdout/stderr must match exactly. Native
exceptions must fail on both backends with the original message and matching
pre-failure effects; the VM additionally preserves `WVM1222`. Diagnostic
wrappers currently use different output channels, so those are not compared
as application stdout.

The qualified paths include integer limits and floating values, enum/flagset
values, templates, native-produced POD and generic POD round-trips, extension
`ref`/`view` dispatch, mutable and mixed mutable/read-only aliases, embedded-zero
strings, borrowed native string results, nullable opaque copy-back and null
tests, native-produced `any` handles and mutation, captured callbacks,
call-scoped expiration, caller-thread rejection, retained any-thread callback
entry from a later native call, and native async scalar/runtime-value results.

String/text callback parameters and results now use the existing wire-v2
slice contract. VM results transfer an owned storage handle, not a pointer
into the callback's temporary execution frame. Empty strings, embedded zeros,
supplementary Unicode scalars, retain/release, frame destruction, and repeated
release are covered by `wio_vm_native_value_contract`. Scalar callbacks keep
their allocation-free result path. Width checks reject malformed narrow
integer values; `f32` crossing rounds as the C++ codec does. Native enum and
flagset codecs use their underlying scalar rather than accidental POD encoding.

The bytecode compiler records structural ABI identities for non-nominal
runtime values, including async payloads, using exactly the C++ emitter's
identity rule. No ABI or bytecode format version change is required.

Both new tests carry `native-differential` and `release-gate` labels. The
Windows/Linux validation workflow selects the gate by label:

```powershell
ctest --test-dir build -C Debug -L "^native-differential$" --output-on-failure
```

Local qualification was performed on Windows. Linux execution is delegated
to the existing CI matrix; a local Windows pass is not a Linux pass claim.

## Remaining VM/native parity and artifact work

This gate qualifies the implemented bridge; it does **not** assert that every
native C++ operation is supported by the VM. The remaining boundaries include
executable negative cases for local `any` and structured callbacks:

- Native PODs with fields outside the qualified
  scalar/pointer/fixed-array/nested-POD adapter surface still need qualification.
  Actual pack-storage fields are not POD fields. Concrete const-generic and
  variadic component arguments and producer-owned C++ defaults are qualified
  below, including overlapping aggregate/field/array-element references.
  A producer without the layout sidecar retains raw-POD round-trips but does not
  support local-component materialization (`WVM1223`) or ordinary field places.
- VM-created `any`/dictionaries/objects are not interchangeable with foreign
  runtime/object handles. Native factories and their foreign values work;
  structural VM-to-producer materialization still needs adapters (`WVM1223`).
  Dynamic arrays now use the optional sequence adapter described below; this
  does not imply arbitrary runtime-value, nullable-wrapper or object conversion.
- POD/object/container callback signatures, nullable callback signatures, and
  callback `ref`/`view` parameters need signature-specific type identities and
  borrow/copy-back plans. Scalar, enum/flagset, string, text, and void callbacks
  are implemented; the other shapes remain `WVM1225`.

Consumed native argument passing remains explicitly rejected as `WVM1225`.
Native-only bridge pruning, hybrid VM SDK hosts, and VM executable/shared/static
artifact packaging remain separate work. These boundaries must be removed or
explicitly qualified before advertising full native-language parity.

## Native POD layout and field adapters

Generated native modules now export the optional `WioGetNativePodLayoutApi`
sidecar declared in `sdk/include/wio_native_pod.h`. This is an additive API:
wire-v2 values and the bytecode format keep their existing versions. Existing
modules without the sidecar keep the previous opaque-byte POD round-trip path.

For concrete native components with scalar/enum/flagset, opaque/nullable-opaque,
supported fixed-array or nested native-POD fields, the producer emits its actual
`sizeof`, `alignof`, field `offsetof`,
field sizes, semantic kind/stable identities, and checked pack/read adapters.
The VM validates names, types, bounds, alignment, overlap, and repeated
registration before publishing an immutable layout snapshot together with the
native function registry. Adapter code remains pinned by the library lease.
No VM aggregate is reinterpreted as a C++ struct, and no public SDK reflection
export is added to an otherwise unexported Wio type.

VM-created components materialize into producer-owned POD wire values on calls;
native-returned PODs unpack into ordinary VM component fields. `view`/`ref`,
field reads/writes, nested components, concrete generics, by-value copies,
whole-component aliases, empty PODs, and native async POD results use this path.
Native aggregate construction now consumes its field arguments in the VM;
generated C++ construction assigns by binding field name rather than assuming
that Wio declaration order matches the C++ struct's order. Native `char` wire
values use unsigned byte payloads regardless of the host's `char` signedness.

An immutable copy of the original POD bytes is retained alongside unpacked
fields. Pack adapters start from that prototype and overlay Wio-visible fields,
so C++ fields absent from the Wio declaration survive field edits, copies,
native mutation/copy-back, and async completion. Producer-owned result storage
is released after unpacking. Mutable copy-back updates existing nested field
storage in place, keeping previously acquired field borrows valid.

The focused tests are `wio_vm_native_pod_layout` (malformed metadata, identity,
size, scalar range, registration conflicts, storage ownership/failure cleanup,
legacy fallback) and `wio_vm_native_pod_layout_differential` (one Wio program
through generated C++ and VM plus real generated native thunks, with independent
numeric oracles). They carry `native-differential;release-gate` labels. Local
qualification is Windows only; Linux remains a CI verification requirement.

POD adapters are internal ABI materialization, not the richer callback bridge.
Structured callbacks still need signature-specific borrow/alias plans.
Overlapping whole-component/field arguments use the projection contract below.
Native member defaults
use the optional producer factory described below; hidden prototype fields are
preserved.

## Fixed-array POD fields

The layout sidecar now includes native component fields of concrete `[T; N]`
types when `T` is a supported scalar/enum/flagset, opaque/nullable-opaque pointer,
nested native POD, or another
supported fixed array. Generated producers assert contiguous field storage and
use the existing typed POD codecs; no wire-v2, sidecar-version or bytecode-format
change is needed. Native enum identities are not mistakenly published as
component layout descriptors.

`NativePodArrayCodec` validates field extents against scalar storage widths and
registered nested layouts, checks element counts/ranges and recursion/size
budgets, and reconstructs independent VM arrays/components. It does not cast a
VM array or component into a C++ object. Array fields use their registered
stable field identity and actual producer byte extent. Empty `std::array<T, 0>`
storage is producer-specific: GCC and MSVC libraries can differ, including
inside multidimensional arrays, so the VM validates the supplied stride rather
than assuming one host library's empty-array size.

Native ref copy-back updates existing array elements recursively; previously
acquired element and nested-component field places remain live. Nested POD
elements retain hidden producer fields through the same immutable backing as
ordinary POD fields. Owned field results are released exactly once after
decoding or adapter failure.

The new `wio_vm_native_pod_array_codec` and
`wio_vm_native_pod_array_differential` gates cover malformed extents/types,
scalar ranges, bool representations, zero/nested-empty layouts, transactional
registration, owned-read failure cleanup, and real local/generic/nested/native
enum/async values through C++ and VM. Local qualification is Windows only;
Linux remains CI qualification. This is field-adapter support, not structural
materialization of arbitrary native container arguments or callbacks.

## Opaque pointer POD fields

Native components may expose `opaque` and `opaque?` fields (including aliases),
concrete fixed arrays of those types, and nested/generic PODs containing them.
Both representations use actual C++ `void*` storage. Nullable opaque values
map between Wio `null` and native `nullptr`; present values preserve pointer
identity. Typed C++ pointers, smart pointers and nullable object/function/ref
wrappers are not implicitly reinterpreted as `void*` POD fields.

```wio
using cpp::header("links.h");
use std::attribute as attribute;

[Native, CppName(native::Links)]
component Links {
    public required: opaque;
    public optional: opaque?;
    public slots: [opaque?; 2];
}
```

The corresponding C++ fields are `void* required`, `void* optional`, and
`std::array<void*, 2> slots`. The producer checks direct pointer-field types;
the VM rejects pointer-size mismatches transactionally before invoking adapter
code. Returned pointer fields must have the registered field's wire identity,
or a properly marked nullable-void value. The bridge then normalizes that field
identity to the unwrapped opaque payload identity. Pointer-array codecs check
each VM element rather than interpreting integers as pointer addresses.

These are **borrowed pointers, not ownership declarations**. Copying a raw C++
POD/array does not retain or delete pointees; callers must keep the resource
alive for its uses, including deferred native operations. If an individual
field read supplies a producer owner handle, the decoded VM opaque value
retains that handle until its last storage reference dies. Pack/read temporary
wire owners are released exactly once on success, malformed output or failure.
No ownership is inferred for pointers stored in raw array bytes.
Producer library pinning descends through decoded component/array values.
Escaped owned fields keep release operations loaded, and borrowed pointers to
native static data keep that module's storage loaded after machine teardown.
This pins code/data, not an externally managed pointee's allocation lifetime.

The `wio_vm_native_pod_pointer` gate covers null/present values, array type/size
checks, malformed identities/kinds, unsupported nullable shapes, transactional
registration and owned-pointer lifetime/failure cleanup.
`wio_vm_native_pod_pointer_rejection` qualifies frontend rejection of invalid
nullable scalar/object fields, including concrete generic instantiation.
`wio_vm_native_pod_pointer_lifetime` loads an independent DLL, destroys the
machine, then uses/releases an escaped owned field and keeps native static data
alive with only the escaped pointer array remaining.
`wio_vm_native_pod_pointer_differential` runs the same Wio program through C++
and VM with aliases, reordered fields, local/native/generic/nested construction,
pointer arrays, existing borrows, mutable copy-back, copy isolation, hidden
fields and async results. It also covers the corrected C++ nullable-wrap
emission: an opaque pointer is forwarded directly, not spelled `void*{...}`.
Qualification is local Windows only, with Linux selected by the existing
`native-differential;release-gate` CI labels. Wire-v2, the POD sidecar and
bytecode versions remain unchanged.

## Const-generic and variadic native POD components

Concrete native component bindings now qualify integer const arguments,
defaulted arguments, zero extents, partial/explicit specialization layouts,
variadic type arguments and an empty trailing pack. For example:

```wio
using cpp::header("vm_pod_template_fixture.h");
use std::attribute as attribute;

[Native, CppName(vm_pod_template::Buffer)]
component Buffer<T, const N: usize = 3> {
    public values: [T; N];
}

[Specialize(T, 4)]
component Buffer<T> {
    public values: [T; 4];
    public bonus: i32;
}

[Native, CppName(vm_pod_template::Echo), instantiate(i32, 3), instantiate(i32, 4)]
fn Echo<T, const N: usize>(value: Buffer<T, N>) -> Buffer<T, N>;

const Extent: usize = 1usize + 2usize;

fn Entry() -> i32 {
    let defaultSize = Echo<i32, Extent>(Buffer<i32>([1, 2, 3]));
    let specialized = Echo<i32, 4>(Buffer<i32, 4>([1, 2, 3, 4], 23));
    return specialized.bonus - 23;
}
```

Native generic functions still require an explicit `[instantiate(...)]`
allowlist. Equal evaluated integer const values match that allowlist even
when a literal and a const symbol have different integer widths; different
values do not match. This does not expand the language's integer const-argument
domain: the existing nonnegative-value restrictions still apply.

Template argument closure and field storage compatibility are separate checks.
A concrete `Marker<string, text>` can be POD-compatible when its C++ template
does not physically store either type. A `Head<string, i32>` with a stored
string field is rejected during analysis. Actual `pack` storage fields remain
unsupported; this is not a promise to reinterpret `std::tuple` as POD bytes.
The producer's usual trivial-layout checks still apply.

A trailing pack can be empty: Wio uses the type `Marker` and construction
`Marker(9)` for a pack-only component with one integer field. Its native type
spelling is `Marker<>`; Wio does not require an empty angle-bracket list.
Concrete variadic component types can be passed to ordinary native declarations.
Native generic function packs retain their existing trailing function-parameter
pack rule; this slice does not change that rule.

Semantic specialization selection is repeated from the primary declaration
when generic function signatures are instantiated. Typed WIR records selected
layout provenance so generic materialization cannot overwrite specialization
fields with the primary layout. This also applies to ordinary Wio components.
VM memberwise component construction now consumes explicit field arguments for
both native and Wio components. Wio component field initializers are now
lowered alongside object initializers, fixing defaults previously ignored by
both backends. No-argument field initialization and explicit `OnConstruct`
calls use their separate execution paths; memberwise arguments override the
corresponding defaults.
Default constructors are resolved from pinned lifecycle bodies, not virtual
method layouts (which exclude constructors). An initializer invokes a
parameterless `OnConstruct` once; the VM continuation invokes only a remaining
parameterized constructor after that initialization step.

The three focused gates are `wio_vm_native_pod_template_contract`,
`wio_vm_native_pod_template_differential`, and
`wio_vm_native_pod_template_rejection`. They cover adapter eligibility,
transactional rejection of open/cyclic/invalid template arguments, local and
native construction, specialization fields, array indexing, copy-back,
pre-existing borrows, hidden producer fields, nested arrays and async results.
The differential gate compares C++/VM output and checks independent numeric
oracles; rejection fixtures also qualify non-POD variadic/specialized fields,
pack storage and non-allowlisted extents. Qualification is local Windows only;
Linux remains selected by `native-differential;release-gate` CI labels.
Wire-v2, the layout sidecar and bytecode format versions are unchanged.

## Native POD producer defaults

Generated native modules export an optional `WioGetNativePodDefaultApi` sidecar
declared in `sdk/include/wio_native_pod.h`. Its version-1 descriptors map stable
type identities to checked default factories. This is independent of the
existing version-1 layout descriptor: layout structure/stride, wire-v2 and
bytecode versions are unchanged, and the original C++ registration overload
remains available.

Each factory evaluates the producer's C++ `T{}` and returns an owned POD wire
value. The VM validates identity, size and ownership, decodes visible fields,
and keeps an immutable byte prototype for hidden fields. Owned output is
released exactly once, including partial-output and malformed-output failures;
generated factories translate exceptions through the native failure boundary.
Registration validates layout/factory pairing and publishes changes atomically.

For example, the C++ producer can define:

```cpp
struct SettingsCpp
{
    std::int32_t count = 17;
    bool enabled = true;
    double scale = 2.5;
    std::int32_t hidden = 61;
};
```

The Wio declaration can expose only the public fields:

```wio
using cpp::header("settings.h");
use std::attribute as attribute;

[Native, CppName(SettingsCpp)]
component Settings
{
    public count: i32;
    public enabled: bool;
    public scale: f64;
}
```

`Settings()` and an implicitly initialized `mut settings: Settings;` then use
the producer defaults instead of guessed VM zeroes. Native memberwise arguments
override their corresponding visible fields after `T{}`, matching the C++
backend. The hidden value remains in the prototype. Decoding an existing
trivially-copyable POD uses a byte copy and `std::bit_cast`, not default
construction followed by overwrite; initializer side effects are not repeated
by packing, native calls or copying.

The qualified surface includes zero-argument construction, implicit defaults,
fixed arrays, nested/concrete-generic/zero-extent/empty native PODs, native
fields inside Wio components and objects, and native async results. A nested
native aggregate is defaulted as a whole by its producer, preserving outer C++
initializers. Wio memberwise fields supplied by arguments do not execute an
unused native default initializer before being replaced.

Legacy producers without the default sidecar still support fully specified
memberwise construction through the existing layout adapters. Construction
that needs a producer default reports `WVM1223` instead of silently guessing
zeroes. The default sidecar is emitted and discovered even when the module
contains no native function declarations.

Raw pointer defaults are borrowed; byte copying does not infer pointee
ownership or destruction. Decoded opaque fields and immutable POD backing
storage both pin the producer library, including hidden pointers into producer
static data after machine teardown. This does not turn arbitrary pointers into
owned resources or extend externally managed resource lifetime.

The four focused gates are `wio_vm_native_pod_default`,
`wio_vm_native_pod_default_differential`,
`wio_vm_native_pod_default_lifetime`, and `wio_vm_native_pod_default_only`.
They cover registration/ownership failures, independent value and initializer
count oracles, C++/VM output parity, default-only modules and escaped DLL values.
All four passed locally on Windows (4/4, 23.66 seconds). No full-suite or local
Linux pass is claimed; existing `native-differential;release-gate` labels select
CI coverage. Overlapping whole-component/field arguments are qualified below.

## Overlapping POD references and field projections

Native calls now preserve actual alias identity when a POD and one of its
fields, nested PODs or fixed-array elements are passed together as `ref`/`view`.
For example, both arguments below designate the same producer field:

```wio
Whole(ref packet, ref packet.count);
Reversed(ref packet.count, ref packet);
Nested(ref packet.inner.value, ref packet.inner, ref packet);
```

VM places are deduplicated by the referenced storage cell, not by their place
wrapper. Two independently acquired field places therefore share one token,
while equal-valued independent copies do not. After collecting arguments, the
VM selects an outermost native POD root and finds child offsets through
registered producer layouts and checked concrete fixed-array strides. It never
casts a VM component to a C++ struct or guesses Wio/C++ field-order equivalence.

The additive `WIO_NATIVE_ABI_VALUE_PROJECTED` reference flag selects a
call-scoped `WioNativeAbiProjection` descriptor containing the root token,
root/field type identities, offset and extent. Generated thunks bind all
unprojected POD roots before reading their arguments, so parameter order does
not change aliasing. Projected borrows refer directly into the aligned typed
producer-frame object. A nested POD argument shares that object too, rather
than materializing a second nested copy.

The frame validates descriptor extent/alignment, bound POD root, root/field
identities, field size, overflow-safe bounds, alignment and mutable access.
Bytecode pins otherwise non-nominal ref-parameter codec identities in existing
reflection records, including primitive multidimensional-array elements;
the VM does not reconstruct compiler type spellings. Traversal is bounded and
does not descend through cyclic object/reference graphs.

Mutable projections mark only their outer root dirty, even if that root was
passed as `view`. This does not make the C++ view writable: the mutable field
argument must independently reference mutable VM storage. The root is committed
once and decoded/copy-backed once. Existing nested field/element places survive
in-place copy-back; hidden producer fields and immutable backing bytes survive
as before. Generated exception boundaries commit dirty slots on failure too.
Raw pointer fields remain borrowed, with no new ownership-transfer semantics.

Generated native descriptors advertise
`WIO_NATIVE_ABI_FUNCTION_PROJECTED_REFERENCES` in the existing flags field.
The VM rejects overlapping calls to producers without this bit as `WVM1225`,
before invoking the thunk, instead of silently passing independent copies.
Ordinary legacy reference calls remain supported. Wire-v2 value/function
structure sizes, layout-v1 descriptors and bytecode format versions are
unchanged. Projected arguments are call-scoped and are not a retained-reference
or structured-callback lifetime contract. Whole native container references are
not implied by fixed-array element support.

The two focused gates are `wio_vm_native_pod_alias` and
`wio_vm_native_pod_alias_differential`. The contract gate checks malformed
metadata, read-only rejection, repeated commit, exception copy-back and
storage-identity lookup. The differential gate checks real C++ address identity,
forward/reversed/mixed view/ref calls, nested and multidimensional arrays,
bool/f64/nullable-opaque fields, hidden fields, pre-existing borrows, independent
copies and a real generated registry with the capability bit removed.
Both passed locally on Windows (2/2, 15.25 seconds); no full-suite or local Linux
pass is claimed. Existing `native-differential;release-gate` labels select CI
coverage. The gate also instantiates native POD codec/frame operations under
MSVC, qualifying the named owner-operation definitions that avoid incomplete
template instantiation in inline static lambda initializers.

## Dynamic array producer adapters

The optional `WioGetNativeSequenceApi` sidecar (sequence-v1) bridges Wio dynamic
arrays to their concrete producer `std::vector<T>` type. Wire-v2 value/function
layouts, existing SDK registration overloads and bytecode format versions remain
unchanged. Existing producers without this export can still pass through their
foreign runtime handles; VM-created arrays require an adapter (`WVM1223`).

```wio
using cpp::header("numbers.h");
use std::attribute as attribute;

[Native, CppName(numbers::Sum)]
fn Sum(values: view i32[]) -> i32;
[Native, CppName(numbers::Transform)]
fn Transform(values: i32[]) -> i32[];
[Native, CppName(numbers::Mutate)]
fn Mutate(values: ref i32[]);

fn Entry() -> i32 {
    mut values: i32[] = [2, 5, 11];
    let copy = Transform(values); // producer result becomes a normal VM array
    Mutate(ref values);          // same-shape updates preserve element places
    return Sum(ref values);
}
```

Descriptors contain stable sequence/element identities and three producer
operations: pack, count and indexed read. The producer copies tagged elements
into its typed container and returns its own owned `Box<T, Id>` handle. The VM
never fabricates producer handle operations or casts VM aggregate memory into
C++ storage. Count and read access the checked producer box by reference rather
than copying the entire vector on every indexed access. Each read returns an
independent wire value. VM decoding creates local values before releasing those
outputs; strings, text and nested arrays never borrow a released producer box.

Automatic adapters cover dynamic arrays with scalar/enum/flagset, string, text,
opaque/nullable-opaque, registered non-generic native POD, or recursively
supported dynamic-array elements. `std::vector<bool>` uses explicit bool values,
not foreign proxy references. Unexposed POD fields survive array round trips via
the existing producer backing representation. Fixed arrays as sequence elements,
generic POD elements, arbitrary nullable elements, `any`, Wio objects, dictionaries
and structured callbacks are not part of this slice. Existing fixed-array POD
field support is independent.

Registration validates descriptor completeness, exact element identities,
container shape, nested adapter availability and conflicts before publishing an
immutable snapshot with the native registry. Producers may list descriptors
unused by a consuming module. Recursive conversion is bounded to depth 64 and
one million elements per sequence. Owned outputs and partial failure outputs are
released through producer operations, including failed pack and indexed read.
Decoded foreign POD/opaque children use the existing recursive library pinning.

By-value calls have copy isolation. Whole-array view/ref aliases share one
call-scoped producer slot, even when the view is passed first. Same-shape mutable
copy-back updates VM element cells in place, including nested arrays and PODs.
Two restrictions remain explicit:

- Length-changing ref copy-back (including nested sequence lengths) fails with
  `WVM1224`. All replacements are validated before any VM ref update is applied.
  A by-value native function may freely resize its private array and return it.
- Passing an array and one of its element/descendant cells as separate ref/view
  arguments fails with `WVM1225` before the native thunk executes. Sequence
  projections need a separate address/invalidation contract; independent copies
  would silently break alias semantics.

These failures cannot roll back external native side effects. Ref/view arguments
and their materialized containers are call-scoped, not retained-reference APIs.

Focused gates: `wio_vm_native_sequence` and
`wio_vm_native_sequence_differential`. The contract gate exercises malformed and
conflicting registration, nested adapter ordering, missing adapters, null/opaque
elements, owned partial failures, invalid kind/identity/index/count and repeated
slot commits. The differential gate compares real generated C++ and VM programs
with numeric oracles, empty/nested arrays, embedded-NUL strings, Unicode text,
bool proxies, hidden POD fields, by-value isolation, pre-existing element borrows
and mixed view/ref aliases. It also checks real VM diagnostics for length-changing
ref copy-back, array/element overlap and producer exception diagnostics.
Both focused gates passed locally on Windows (2/2, 35.70 seconds). Only these
new gates were run; no full-suite or local Linux pass is claimed.
