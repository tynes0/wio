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
- VM-created `any`/objects are not interchangeable with foreign
  runtime/object handles. Native factories and their foreign values work;
  structural VM-to-producer materialization still needs adapters (`WVM1223`).
  Dynamic arrays/dictionaries now use the optional container adapters described below; this
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
supported dynamic-array/dictionary elements. `std::vector<bool>` uses explicit bool values,
not foreign proxy references. Unexposed POD fields survive array round trips via
the existing producer backing representation. Fixed arrays as sequence elements,
generic POD elements, arbitrary nullable elements, `any`, Wio objects
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

## Ordered and unordered dictionary producer adapters

The optional `WioGetNativeDictionaryApi` export uses dictionary-v1 metadata,
independent of sequence-v1 and POD layout/default sidecars. It maps Wio
`Dict<K, V>` to producer `std::unordered_map<K, V>` and `Tree<K, V>` to
`std::map<K, V>`. Existing wire-v2 layouts, bytecode formats and public SDK
registration symbols remain unchanged. Old producers can still round-trip
their foreign runtime handles; local VM dictionaries require adapters
(`WVM1223`).

```wio
using cpp::header("scores.h");
use std::attribute as attribute;

[Native, CppName(scores::Transform)]
fn Transform(values: Dict<string, i32>) -> Dict<string, i32>;
[Native, CppName(scores::Update)]
fn Update(values: ref Dict<string, i32>);

fn Entry() -> i32 {
    mut scores: Dict<string, i32> = { "health": 7, "armor": 11 };
    let armor: view i32 = ref scores["armor"];
    let copy = Transform(scores); // by-value isolation; returned keys may differ
    Update(ref scores);           // existing key set, in-place value updates
    return deref armor;
}
```

Each descriptor records exact dictionary, key and mapped-value identities,
ordered/unordered flags and producer pack/count/read operations. Entry buffers
alternate key and value cells; counts refer to pairs. The producer constructs
its concrete C++ map, including its real typed runtime box. Read fills the entire
buffer in one traversal, avoiding an indexed-map walk from the beginning for
each entry. It does not copy the complete map for count or read. Owned key/value
cells are independently released, including every partially filled failure.
Duplicate keys are rejected on both producer pack and VM decode, rather than
silently collapsing malformed producer output.

Portable keys are bool, signed/unsigned integers, byte, char, enum/flagset,
string and Unicode text. Floating-point, nullable, opaque and object/container
keys are not part of the automatic adapter contract. Values cover scalar,
string/text, opaque/nullable-opaque, registered non-generic native POD and
recursively supported dynamic array/dictionary shapes. Examples include
`Dict<string, i32[]>`, `Dict<string, Tree<i32, string>>` and
`Dict<string, i32>[]`. Arbitrary nullable wrappers, generic POD elements,
`any`, Wio objects and structured callbacks remain separate work.

Sequence/dictionary descriptors are staged together, then all key/value and
nested dependencies are validated before publishing the registry snapshot.
Mixed nesting therefore does not depend on descriptor order. Unknown flags,
wrong identities/order, incomplete descriptors, conflicting adapters and
missing dependencies reject registration atomically. Conversion has the same
depth-64 bound and one-million-entry limit per dictionary as sequence conversion.

Ref copy-back uses a temporary key index, not producer iteration positions.
Unordered rehashing cannot redirect one VM value borrow to another key. Updates
preserve each original VM value cell, including nested array elements and POD
fields/backing. Whole-container view/ref aliases share the existing producer
frame slot; repeated commits do not acquire a second ownership claim. Library
pinning now traverses dictionary keys/values, so escaped native POD/opaque
children keep producer code/data loaded after their VM is destroyed.

Current restrictions:

- Ref calls must preserve the key set and all nested container shapes. Adding,
  removing or renaming keys, even without changing the count, or changing a
  nested array length fails with `WVM1224` before VM replacements are applied.
- A container and its key/value/descendant cell cannot be passed as separate
  ref/view arguments (`WVM1225` before the thunk executes). Address-preserving
  container projections and borrow invalidation are still pending.
- By-value native calls may change keys/lengths and return a new container.
  Rejected copy-back cannot roll back independent native side effects.
  Materialized ref/view values remain call-scoped.

Dictionary decode, duplicate validation and copy-back key matching use hash
indices for expected-linear work. Large VM dictionary construction now uses the
same portable-key hash helper instead of scanning all preceding keys; existing
first-key-wins semantics and insertion order are unchanged. Small or unsupported
key sets retain the original path. Ordered dictionaries still perform their
normal key sort. This is an asymptotic improvement, not a measured end-to-end
compiler/runtime speedup claim.

Focused gates: `wio_vm_native_dictionary` and
`wio_vm_native_dictionary_differential`. They cover checked producer boxes,
malformed metadata, cross-container dependency ordering, key-based copy-back,
invalid/duplicate keys and buffers, owned partial failures, empty/nested maps,
embedded-NUL keys/strings, Unicode keys, unordered rehashing, view/ref aliases,
POD hidden fields, nullable pointers, by-value isolation and existing borrows.
The real generated-DLL host case checks escaped dictionary/POD library lifetime
and final lease release. Negative VM gates cover key insertion, same-size key
renaming, nested array growth, overlapping element borrows and producer
exceptions. Both new gates passed locally on Windows (2/2, 85.62 seconds),
including the real generated-DLL lifetime host case. Only the new focused
gates were run; no full-suite or Linux pass is claimed.

## VM-created any value payloads

The optional `WioGetNativeAnyApi` sidecar (`wio_native_any.h`, version 1)
materializes VM-created boxed values as real producer-owned `runtime::Any`
cells. Wire ABI v2 and the bytecode format are unchanged. The payload registry
maps stable Wio type ids to producer pack/read functions; C++ type-token
addresses never serve as cross-backend identities. Registration is staged
after POD/sequence/dictionary dependencies and published atomically.

The automatic descriptor closure comes from concrete `AnyBox`, `is` and `fit`
operations in the module, when native signatures use `any`. Eligible payloads
are scalars, string, Unicode text, opaque, supported non-generic native POD,
dynamic arrays and ordered/unordered dictionaries, including supported nested
container shapes. Null `any` has a separate producer operation, not a fake
boxed payload or nullable wire cell. VM default initialization and null type
tests now agree with the C++ runtime.

```wio
[Native, CppName(interop::MutateCopy)]
fn MutateCopy(value: any);
[Native, CppName(interop::Replace)]
fn Replace(value: ref any);

mut value: any = 7;
let alias: any = value;
MutateCopy(value); // a C++ Any copy shares its boxed cell
let changed = alias fit i32;
Replace(ref value); // replacing the Any itself does not replace alias
```

VM any copies share a producer backing handle once materialized. Native code
may mutate the boxed payload through a copied Any; subsequent VM `is`/`fit`
operations refresh from that producer cell. This also permits container growth
inside any, because `fit` recovers owned value copies, not live element borrows.
Whole-value ref replacement instead publishes a new VM any and leaves previous
copies bound to their old producer cell. Same-place view/ref arguments share a
typed producer frame slot. The sidecar reads producer storage without copying
the entire Any before dispatch; payload extraction still copies owned values.
Backing and decoded POD/opaque children pin producer code after the machine
dies, and the final release drops the library lease.
Concurrent first materializations converge on one installed producer cell;
VM extraction uses a local snapshot rather than rewriting shared payload data.

Limits remain explicit:

- A native payload absent from the concrete descriptor closure retains its
  producer handle for native-to-native forwarding. Trying to inspect/cast that
  payload in the VM fails with `WVM1224`; a VM payload without a supported
  adapter fails with `WVM1223`.
- Multiple Wio identities represented by the same producer C++ boxed type
  are rejected as ambiguous, not selected by hash-map iteration order.
  Source-alias canonicalization/tagging remains pending.
- Object/interface/Box, arbitrary nullable wrappers, generic POD payloads,
  nested `any` and any-bearing container adapters are not qualified here.
  Native structured callbacks and consumed parameters remain separate work.
- Producer code must not concurrently mutate the same cell during VM
  extraction. This does not introduce synchronization for native data races.
- Native modules without this optional API keep their old foreign-handle
  forwarding path; their any payloads are not structurally decoded.

Focused gates: `wio_vm_native_any` and `wio_vm_native_any_differential`.
They cover stable ids, atomic metadata rejection, malformed handles, unknown
and ambiguous payloads, owned partial failure cleanup, by-value shared mutation,
concurrent first materialization, unsupported native payload forwarding,
ref replacement and aliases, null/default behavior, embedded-NUL strings,
Unicode text, array/dictionary payload growth and fitted-copy isolation, POD
hidden fields, opaque payloads, and a real generated-DLL escaped-value lifetime
host. Only the new gates are run for this change; no full-suite/Linux parity
claim is made.
Both focused gates passed locally on Windows (2/2, 23.85 seconds), including
the generated native executable, VM/DLL differential, unsupported-cast
diagnostic, and escaped-value lifetime checks.

## Host-backed object contract foundation

The optional `WioGetNativeObjectApi` export (`wio_native_object.h`, sidecar
version 1) describes producer factories for typed proxies of VM-owned objects.
It is additive: wire ABI v2 and the bytecode format are unchanged, and existing
native registry registration overloads remain available. The loader discovers
the optional sidecar; descriptors are validated and published transactionally
with the other producer layouts. Only concrete reflected object types qualify.

A host token carries a stable type id, object identity, generation, retained
owner handle and method dispatch callback. The producer factory creates a real
`runtime::Ref<T>` whose native-visible methods can delegate to that callback.
There is no reinterpretation of VM object memory as a C++ object and no copied
VM instance. `ObjectHostLease` validates the token and acquires exactly one
host claim per proxy. Native Ref copies share the proxy and its host claim.
Dispatch currently requires the proxy's construction thread; foreign-thread
entry returns `WRONG_THREAD` without invoking the host.

`ObjectAdapter<T, Proxy, TypeId>` supplies factory, borrowed identity recovery,
owned weak-cache storage and owned strong-lock operations. Expired weak locks
return `STALE_HANDLE`. Weak storage does not own a strong host claim:
`HostObject<T>` releases its lease at `OnZeroStrong`, even when weak references
delay the proxy's physical C++ destruction. Constructor failures and partially
owned dispatch results unwind without leaking host/result claims. Method errors
preserve their status and message as `ObjectDispatchError` for the native thunk
exception boundary. A factory creates a new proxy; factory calls alone do not
canonicalize repeated object identities.

This is a contract/runtime foundation, **not automatic CLI object transfer**:

- Automatic VM materialization now uses the qualified compiler-generated
  producer surface below. Machine-owned identity caches use producer weak
  handles, validate generations and recover the original VM object on returns
  and mutable-reference copy-back without a strong cache ownership cycle.
- Native field projections/synchronization, `ref`/`view` receiver contracts,
  inheritance/interfaces and generic object routing are not qualified yet.
- Hosted proxies skip duplicate Wio construction/destruction. Final host
  release queues cleanup for a VM safe point; it does not execute a Wio
  destructor on an arbitrary native release thread. Machine shutdown invalidates
  retained proxies and cleans their VM objects. See lifetime limits below.

The focused gate `wio_vm_native_object_host` uses real encoded/decoded bytecode
and a live VM host behind a typed native proxy. Virtual native calls execute VM
field load/store functions and mutate the original object. It checks identity
recovery, native Ref aliases, host survival after the caller drops its machine
claim, strong/weak lifetime separation, exactly-once final host/proxy release,
void dispatch, owned partial failure cleanup, constructor exceptions,
wrong-thread rejection and malformed/conflicting/atomic descriptor registration.
This gate does not qualify generated proxies, real DLL object transfers or
full C++/VM object parity. Only the new focused gate was run locally on Windows;
no full-suite or Linux pass is claimed.

## Compiler-generated producer object proxies

`WirNativeObjectEmitter` now selects eligible objects from native parameter and
result signatures and emits concrete proxy classes plus `WioGetNativeObjectApi`
factory/identity/weak-lock descriptors. Objects that never cross a native
boundary do not emit this support. An object is selected only when the entire
method surface qualifies; unsupported methods do not get a partially functional
proxy. The initial automatic producer surface is deliberately narrow:

- Concrete non-generic Wio objects; only the empty built-in `object` base is
  allowed. Custom inheritance and interfaces remain pending.
- Private scalar/string/text fields, with no public native projections. Other
  field shapes are excluded so dummy producer fields cannot run nested Wio
  lifecycle bodies during proxy construction/destruction.
- Non-abstract, synchronous Wio methods with scalar/string/text and qualified
  object values in their arguments/results (or void results). Reference, nullable, container,
  generic, native/external and async method signatures remain excluded.

Proxies override both canonical WIR dispatch slots and the public `_WF_*`
wrapper methods used by native C++ libraries. Those wrappers are virtual only
for selected object types. Method routing uses canonical reflected stable
method ids, not source names or numeric bytecode function offsets. Argument
and result codecs use canonical stable type identities. The producer uses
`SkipConstructor` and marks the instance as hosted; its C++ destructor skips
the Wio destructor body. Ordinary C++ backend object instances retain their
normal constructor/destructor behavior. This does not yet add field access,
SDK/reflection-thunk routing. Automatic VM lifecycle ownership for this
qualified surface is described below.

The focused gate `wio_vm_native_object_proxy` builds a real generated C++
executable and a generated DLL with its bytecode sidecar. The native executable
checks ordinary object calls. A qualification host discovers the DLL's generated
object API and native registry, registers it through the real VM library loader,
creates the original object by running a VM constructor, and invokes producer
thunks against the generated proxy. It qualifies scalar field mutation,
embedded-NUL string results, non-ASCII Unicode text, void methods, native-held
Ref lifetime, identity recovery, weak expiry, and suppressed duplicate Wio
constructor/destructor bodies. This test host explicitly owns lifecycle cleanup;
it is separate from the Machine-managed host and identity-cache gate below.
Emitter selection tests also reject unsupported fields/signatures, abstract and
async methods, missing stable identities, custom inheritance, and unused types.

This producer gate does not by itself qualify automatic VM ownership. The
automatic integration has a separate focused gate below. No full-suite or
Linux pass is claimed.
The new gate passed locally on Windows (1/1, 12.68 seconds), including selection
checks, generated native executable behavior and the generated DLL/live-VM host.

## Machine-managed object materialization and identity

Native calls now materialize eligible VM-created objects through the producer
object sidecar. Each VM object receives a process-unique, non-reused identity.
The machine caches a producer weak handle keyed by that identity. Simultaneous
arguments for the same object reuse one live proxy. After its final native
strong release, a later materialization uses a fresh host generation instead of
resurrecting the expired proxy.

Producer object returns and whole-object `ref` copy-back recover the original
VM object, including its fields and identity. A proxy belonging to a different
machine, a stale generation, or a closed host is rejected. Genuine producer-owned
objects retain the existing foreign-object path. Mutable object handles can be
replaced without transferring field ownership to a copied producer projection.

Ownership follows WIR: borrowed object loads do not acquire strong references,
explicit retain creates an owned claim, and owned place writes transfer the SSA
handle. This prevents hidden VM register claims from delaying lexical Wio
destruction. A native-held `Ref` can keep the object alive after its Wio scope
ends, while the owning machine remains active.

Final proxy release queues a cleanup claim without allocating or executing Wio
code in the release callback. The machine drains it after native wire releases
and at invocation safe points. Wio lifecycle bodies execute once, independently
of producer weak-storage deallocation. Producer library leases protect proxy
code and weak-handle operations.

Lifetime limits are explicit:

- This slice supports the eligible concrete object/method surface listed above,
  not inheritance, interfaces, generic objects, public fields or general SDK
  reflection dispatch.
- Proxy method calls are caller-thread-only. Foreign-thread calls preserve
  `WRONG_THREAD` through the generated native exception boundary.
- Machine destruction invalidates hosts and cleans retained VM objects; it does
  not prolong VM execution. Calls through surviving native references return
  `STALE_HANDLE` until those references are released.
- A last proxy release after machine shutdown defers its producer code lease.
  Embedders call `Machine::collectNativeObjectRetirements()` from host code after
  producer calls have returned and concurrent producer releases have quiesced.
  This prevents unloading a DLL inside its own final-release stack frame.
- Passing a hosted object and a separate reference to one of its fields in the
  same native call rejects with `WVM1225`. Independent producer reference cells
  cannot yet synchronize mutations performed by hosted method dispatch.
- Cleanup queued by an external native release is observed at the next machine
  invocation safe point, or shutdown; it is not a background Wio executor.
- Deferred destructor failures are not currently propagated through an external
  native release. Richer safe-point/teardown cleanup diagnostics remain pending.

The focused `wio_vm_native_object_materialization` gate compares generated C++
and actual `--vm-run-bytecode` execution against the same generated producer DLL.
It covers proxy reuse, original-object returns and ref replacement, scalar/string/
text/void calls, scope destruction, retained references, wrong-thread and stale
host failures, foreign-thread final release, safe-point cleanup, overlap rejection
and failure unwind. Additional host checks cover
cross-machine identity rejection, generation validation, weak-cache recreation
and deferred code-lease collection. Only this focused gate is run for this slice;
no full-suite or Linux pass is claimed.

The automatic gate passed locally on Windows (1/1, 14.50 seconds), including
generated native execution, actual bytecode CLI execution, manager identity
checks, retained-proxy shutdown and foreign-thread final release.

## Object-valued hosted method signatures

Qualified hosted methods can now receive and return concrete VM-origin object
values. This includes returning the receiver, selecting an existing argument,
and constructing a different eligible object type:

```wio
public fn Same() -> Counter {
    return deref self;
}

public fn Choose(other: Counter) -> Counter {
    return other;
}

public fn MakeBadge(seed: i32) -> Badge {
    return Badge(seed);
}
```

Native C++ calls still use the real generated `runtime::Ref<T>` method surface.
Object arguments recover their original VM identity before method execution;
object results use the same machine-managed weak proxy cache as free native
calls. A returned receiver therefore uses its canonical live producer proxy,
not a copied field projection. Newly constructed results retain their VM object
until the final native claim retires at a VM safe point.

Proxy selection now follows method signature dependencies. A `Badge` appearing
only in a `Counter` method signature gets an adapter even if every free native
declaration mentions only `Counter`. Discovery uses a work queue, handles
mutually dependent signatures, rejects unsupported dependencies transitively,
and removes otherwise eligible dependencies that no surviving native root needs.
No partially functional method surface is advertised.

Every generated method's codec arguments are held in a non-copyable,
stack-allocated `ObjectArguments<N>` guard. Owned object, string and text wires
are released on normal return, host failure, or a later argument conversion
failure. Initialization is incremental so a conversion exception cannot strand
already acquired claims. This also fixes the previous scalar/string/text proxy
path's missing release of owned codec arguments. By-value C++ method arguments
move into their codecs, avoiding an extra string copy or object retain/release
pair. A separate execution-result guard preserves Wio lifecycle cleanup if a
returned object's proxy factory fails before installing its native claim; it
does not copy scalar/string results.

Object-valued method parameters must belong to the same live machine. A hosted
proxy from another machine, or an ordinary producer-created C++ object without
VM field storage, rejects with `TYPE_MISMATCH`. General foreign-object method
routing/field synchronization, nullable objects, object ref/view method
signatures, generics, inheritance and interfaces remain pending. VM method
failures preserve `PANIC` through the generated native boundary; closed hosts
continue returning `STALE_HANDLE`. This is an implementation extension to the
existing wire-v2/object-sidecar-v1 contracts, not a wire-version change.

The focused `wio_vm_native_object_method_values` gate compares the generated
C++ executable and actual VM CLI against one generated producer DLL. Fifty
constructor/return/argument/identity/destructor rounds exercise a method-only
dependency, receiver returns, other-object returns, multiple object parameters,
embedded-NUL strings and Unicode text. The host gate also covers same-machine
identity recovery, cross-machine and producer-owned argument rejection, bad
arity, VM panic propagation, shutdown, partial argument initialization cleanup,
owned argument weak-cache expiry after panic, partial proxy factory output
release, exactly-once returned-object destruction after factory failure, and
transitive/cyclic proxy selection. Only this new gate is run for this slice;
no full-suite or Linux pass is claimed.

The method-values gate passed locally on Windows (1/1, 24.38 seconds), including
generated native execution, actual bytecode CLI execution, and host-injected
factory failure qualification.
