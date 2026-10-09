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
are scalars, string, Unicode text, opaque, qualified native POD,
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
- Object/interface/Box, arbitrary nullable wrappers,
  nested `any` and any-bearing container adapters are not qualified here.
  Qualified synchronous object/interface payloads are described separately
  under "VM-created any object payloads" below.
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
- Native field projections/synchronization and inheritance/interfaces remain
  pending. Qualified caller-rooted `ref`/`view` results and concrete generic
  object routing are described in the later producer sections.
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

- Concrete Wio objects, including the fixed-arity type/const specializations
  qualified below; only the empty built-in `object` base is allowed. Custom
  inheritance and interfaces remain pending.
- Private scalar/string/text fields, with no public native projections. Other
  field shapes are excluded so dummy producer fields cannot run nested Wio
  lifecycle bodies during proxy construction/destruction.
- Non-abstract, synchronous Wio methods with scalar/string/text and qualified
  object and nullable-object values in their arguments/results (or void results), plus `ref/view`
  qualified-object parameters and proven caller-rooted object borrowed results.
  Scalar/container references, nullable scalar/container values, container,
  open generic method templates, native/external and async method signatures
  remain excluded.

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
  not inheritance, interfaces, open generic objects, public fields or general SDK
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
routing/field synchronization, unqualified borrowed-result origins,
inheritance and interfaces remain pending. Caller-rooted borrowed
results and concrete generic objects are qualified in the later sections below. VM method
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

## Object ref/view method parameters

Hosted synchronous methods can borrow eligible same-machine object handle cells:

```wio
public fn ReadFrom(source: view Counter) -> i32 {
    return source.Read();
}

public fn Replace(target: ref Counter, source: Counter) {
    target = source;
}

public fn MakeTarget(target: ref Counter, seed: i32) {
    target = Counter(seed);
}
```

The borrow is of the **handle cell**, not a copied object field projection.
Two parameters borrowing one cell share a VM place; `ref + view` and `ref + ref`
observe rebinding immediately within the method. Two distinct cells holding the
same object stay independent. The producer uses tagged reference wires and
keys aliases by stable handle-cell/local-place identity; raw receiver views use
object identity, not the address of a temporary lazily materialized handle.
`ObjectReferences<N>` holds the producer's reference cells; C++ method wrappers
keep the existing generated
`Place<runtime::Ref<T>>` surface. Method signature dependencies also follow
borrowed object types.

Only `ref` cells copy back. Replacements are encoded before any caller cell is
committed, so a later proxy factory failure leaves all caller cells unchanged.
Successful rebinding copies back even when the Wio method subsequently panics;
the native boundary preserves `PANIC`. The producer wrapper commits its local
handle cells on both normal return and exceptional unwind. Owned argument,
temporary VM-place, staged-output and execution-result claims are guarded.
Objects retired during external host dispatch are cleaned at the next VM safe
point or shutdown, consistent with the existing proxy lifetime contract.

`view` cannot be used as a mutable argument. Raw receiver places cannot be
rebound through this bridge. Null, undersized, misaligned, projected or invalid
flagged reference buffers reject before executing the method. Recovered object
arguments must still originate in the same machine; a reference does not grant
foreign-object field synchronization. Reference cells and VM places remain
call-scoped: no VM place or temporary tagged cell escapes. The later borrowed
result slice maps supported results back to original caller storage.
Scalar/POD/container reference method parameters
are separate remaining work. No wire-v2/object-sidecar-v1 ABI version changed.

The focused `wio_vm_native_object_method_references` gate compares generated C++
and actual VM CLI execution, with 30 rounds covering fresh-object rebinding,
`view` reads, distinct raw receiver views, mixed aliases, repeated mutable
aliases, distinct cells containing
one object, combined ref-parameter/object-result methods, and lifecycle counts.
The live host adds panic copy-back, readonly/raw C++ place rejection, invalid
reference rejection, partial proxy factory cleanup, and transactional multi-cell
copy-back failure. Only this new gate is run for this slice; no full-suite or
Linux pass is claimed.

The reference-method gate passed locally on Windows (1/1, 18.55 seconds),
including generated native execution, actual bytecode CLI execution and the
live host's negative/factory-failure scenarios.

## Nullable object method and free-native values

Eligible concrete objects now cross synchronous hosted method and free-native
boundaries as nullable values as well:

```wio
public fn MaybeSelf(enabled: bool) -> Counter? {
    if (enabled) {
        return deref self;
    }
    return null;
}

public fn ReadMaybe(value: Counter?) -> i32 {
    if (value == null) {
        return -1;
    }
    return value.Read();
}

[Native, CppName(native_api::Echo)]
fn NativeEcho(value: Counter?) -> Counter?;
```

The generated C++ native signature uses `std::optional<runtime::Ref<T>>`.
This is the existing nullable language representation, not an additional ABI
box: a present value uses the same ordinary object handle, codec operations and
canonical concrete-object type id as `T`. The nullable wrapper does not create
a second proxy identity or duplicate the object. A disengaged optional or an
engaged optional holding an empty C++ `Ref` writes canonical nullable `VOID`.
Wio-origin non-null values still recover their original same-machine object.
The restriction on foreign-machine and producer-only object values is unchanged.

Null wires require exactly the `NULLABLE` flag, no owner state/operations and
either zero or the matching concrete-object type id. Methods emitting a null
result include that id. A non-nullable parameter rejects a null wire; malformed
null flags, identities or fake object handles also reject. Nullable-only method
dependencies participate in adapter discovery, whole-surface qualification and
transitive rejection, even without a free-native declaration for that type.
Neither wire-v2 nor object-sidecar-v1 changes version.

Two Typed WIR corrections are covered by this slice. Flow-narrowed nullable
storage is explicitly unwrapped before object identity comparison, preserving
verifier requirements and single evaluation. Nullable wrap/unwrap retains the
new result's ownership and releases its consumed owned temporary, rather than
holding an extra object reference until function exit and delaying destruction.

`wio_vm_native_object_method_nullable` runs generated C++ and actual VM CLI
execution against the generated producer, plus the live host. Forty rounds
cover absent/present receiver values, nullable arguments, free-native echo,
identity, nullable-only `Badge` dependencies, and exact lifecycle counts. The
host covers canonical null, malformed null, non-nullable rejection, ordinary
object wire compatibility, and partial proxy factory failure with exactly-once
returned-object cleanup at the next VM safe point.

Unqualified borrowed-result origins, nullable scalar/POD/container method
values, structured fields, open generic
objects, inheritance/interfaces and general foreign-object synchronization
remain separate pending work. This slice does not close native object parity.
Only this new focused gate is run; no full-suite or Linux pass is claimed.

The nullable-method gate passed locally on Windows (1/1, 10.76 seconds),
including generated C++, VM CLI and host-injected partial factory failure.

## Nullable object ref/view method parameters

Hosted synchronous methods can now borrow eligible nullable object handle cells.
This is `ref Counter?` / `view Counter?` (a reference to a nullable handle),
not `(ref Counter)?` (a nullable reference). For example:

```wio
public fn Clear(target: ref Counter?) {
    target = null;
}

public fn Make(target: ref Counter?, seed: i32) {
    target = Counter(seed);
}

public fn ReadFrom(source: view Counter?) -> i32 {
    let value = deref source;
    if (value == null) {
        return -1;
    }
    return value.Read();
}

mut target: Counter? = null;
owner.Make(ref target, 42);
let value = owner.ReadFrom(ref target); // mutable reference may bind to view
owner.Clear(ref target);
```

Read through `deref` before narrowing the nullable value; member access on a
reference to nullable storage is not itself a flow-narrowed object value.
The producer's C++ signature remains `Place<std::optional<runtime::Ref<T>>>`.
Borrowing uses the same tagged reference wire and concrete-object identity as
the existing object bridge, with canonical nullable `VOID` for an empty cell.
Generic local/borrowed places now expose stable cell identity and rebinding
capability. Readonly and proxy-projection places cannot become mutable object
borrow cells. This adds no native ABI box, wire version or sidecar version.

Null-to-object and object-to-null copy-back are supported. Repeated `ref/ref`
and `ref/view` borrows share one VM place, including view-first argument order;
distinct cells containing one object remain independent. Nullable and
non-nullable declarations cannot alias the same tagged cell, even though their
concrete-object wire type ids agree. A null cell cannot satisfy a non-nullable
reference. Malformed null tags and readonly/misaligned/projected reference
buffers reject before execution.

Copy-back stays transactional: all mutable replacement wires are staged before
any caller cell is committed. If a second proxy factory fails, a first null
replacement or first newly materialized object is discarded, leaving both old
cells intact. Owned partial factory output and staged objects are released
exactly once. Null copy-back also occurs before propagating a VM panic; the
producer's exception path commits the changed cells before rethrowing `PANIC`.

The gate exposed three ownership corrections beyond parameter qualification:

- VM synchronous direct, virtual/interface and indirect calls clear an owned
  argument's caller SSA claim after the callee frame acquires it. Borrowed
  receivers/captures are not consumed. Caller frame lookup remains safe if
  pushing the callee reallocates the frame vector.
- Replacing an object-valued VM place with null runs the old object's cleanup;
  borrowed self-assignment acquires its replacement claim before releasing the
  old one. Final-handle destruction no longer disappears on null replacement.
- Generated C++ indirect calls move owned operands, matching direct and virtual
  calls instead of retaining an extra nullable handle until the caller exits.

`wio_vm_native_object_method_nullable_references` compares generated C++ and
actual VM CLI execution against the generated producer, then runs the live
host. Thirty rounds cover nullable transitions, mixed plain/nullable parameters,
ref/ref and ref/view aliases in both orders, distinct cells, and shared local
places. Lexical counters cover owned direct/virtual/function-value arguments
and destruction of a final object claim through a nullable reference. The
VM-only panic entry verifies readonly rejection and null copy-back. The host
adds malformed null/reference rejection, mixed-type alias rejection, partial
factory failure, transactional null replacement rollback, and two-stage
null-to-object rollback with exact constructor/destructor counts.

Unqualified borrowed-result origins, escaping VM cells, scalar/POD/container method references,
open generic objects, inheritance/interfaces and foreign-object synchronization
remain pending. Async task-launch ownership is separate from this synchronous
call-frame correction and still requires its own lifecycle qualification.
Only the new focused gate is run; no full-suite or Linux pass is claimed.

The nullable-reference gate passed locally on Windows (1/1, 24.28 seconds),
including generated C++, actual VM CLI and the host's panic/factory-failure
qualification. This does not close the broader native interoperability sprint.

## Caller-rooted borrowed object method results

Hosted synchronous methods can now return `ref/view` of a qualified concrete
object, including a reference to a nullable object handle cell. The generator
proves that every return originates in the receiver or a compatible reference
parameter. Direct returns, readonly borrow forwarding and acyclic block-parameter
merges are supported; every incoming path must have a supported origin. Readonly
references cannot become mutable results. Global, local, field/projection,
nested-call and cyclic/unproven origins conservatively reject proxy eligibility.

```wio
public fn AsRef() -> ref Counter {
    return self;
}

public fn AsView() -> view Counter {
    return self;
}

public fn BorrowTarget(target: ref Counter?) -> ref Counter? {
    return target;
}
```

There are two distinct transports:

- A receiver result uses a temporary owned object wire. The generated wrapper
  validates the returned host's owner, object id, generation and dispatcher,
  then returns a raw borrow of its own receiver. Different C++ proxy instances
  for the same VM identity are accepted. The temporary wire is released before
  returning; no permanent strong claim is added to the borrow. Receiver borrows
  cannot rebind an object handle cell.
- A parameter result uses a borrowed `WIO_NATIVE_ABI_REFERENCE` pointing to the
  original caller's tagged cell, not the per-call VM `PlaceStorage`. It has the
  concrete object type id, no owner, and `MUTABLE` only for a `ref` result. A
  nullable result refers to an ordinary non-null cell whose contents may be null.
  The generated wrapper maps this token back to its original `Place` before the
  temporary argument guard expires. Result mapping does not preserve a pointer
  to the temporary ABI buffer. Copy-back completes before the result leaves the
  wrapper, so assigning a fresh handle and returning its alias observes the
  new handle.

The VM compares returned place identities against known live argument places;
it never dereferences an unmatched result that might point into a popped frame.
`Place::asView()` preserves cell identity while removing write/rebind access.
Distinct caller cells containing the same object remain distinct aliases.
Shared local places retain their original shared cell, not a copied handle.

Borrowing does not extend an externally borrowed source cell's lifetime. C++
callers must keep the referenced cell or receiver's owning handle alive while
using the result. This is not a persistent/global-reference or dangling-pointer
safety feature for arbitrary native code.

Typed WIR no longer retains an unnecessary owned receiver SSA copy for borrowed
method results on lexical object identifiers/self/super. Temporary receivers
retain their tracked backing-place behavior. Synchronous VM frames normalize
owned object inputs to non-owning receiver references when the formal is
borrowed, avoiding an extra strong claim in the returned borrow.

`wio_vm_native_object_method_borrowed_results` compares generated C++, VM CLI and
a live host. Thirty rounds cover self/ref/view results, nullable cells, readonly
write rejection, stable aliases after rebinding, assign-and-return copy-back,
shared local places and exact lexical constructor/destructor counts. The host
also exercises separately materialized proxies, wire cell identity, branch
selection, readonly-to-ref rejection and malformed/mismatched result rejection
through generated wrappers. Selection checks cover all CFG predecessors,
readonly merges and conservative rejection of unsupported origins.

The wire-v2/object-sidecar-v1 versions are unchanged. Global/projection/nested
borrowed results, broader method signatures, open-generic/inherited/interface objects,
foreign-object synchronization and async task ownership remain separate work.
Only this new focused gate is run; no full-suite or Linux pass is claimed.

The borrowed-result gate passed locally on Windows (1/1, 14.94 seconds), including
generated C++, actual VM CLI, host identity/cell tests and malformed-result
rejection. The broader native interoperability sprint remains open.

## Concrete generic object proxies

Fixed-arity Wio object specializations can now cross the automatic native object
boundary under the same field, method, lifecycle, and borrow restrictions as
non-generic objects. The focused surface includes `Box<i32, 7>`, `Box<i32, 9>`,
and `Box<string>` with a default const parameter. Type and const arguments remain
part of the stable object identity; distinct specializations cannot decode one
another's handles. Open type/const arguments, unresolved nested arguments,
packs, and open generic method templates are not eligible proxies.

For a primary fixed-arity declaration, semantic `self` compatibility compares
the symbolic declared parameters with explicit `Box<T, Tag>` arguments. It does
not make parameters wildcards: returning self as a different type or const
specialization remains a semantic error. Owned `deref self`, borrowed `self`,
and newly constructed concrete results preserve their selected specialization.

Generic materialization now refreshes reflected method descriptors with the
concrete owner, parameter/result types, specialized function, declaration
attributes, and a signature-derived stable method identity. Open generic
declarations remain signature-only bytecode metadata; concrete method bodies
retain their origin identity and execute normally. See
[the bytecode declaration contract](WIO_BYTECODE_FORMAT.md#generic-declaration-records-11).

`wio_vm_native_object_concrete_generics` compares generated C++, actual VM CLI,
and the generated producer DLL/live host. Thirty rounds per specialization
exercise scalar/string reads, const/default arguments, owned and ref/view self
results, cloned objects, and exactly-once constructor/destructor counts. The
host checks concrete reflection/origin metadata, specialization isolation,
wrong-type handle rejection, template invocation rejection, malformed template
records, recursive type-openness propagation, and bytecode version compatibility.
The semantic negative fixture must reject both wrong-type and wrong-const self
returns.

Native wire-v2/object-sidecar-v1 remain unchanged; the bytecode writer now emits
WIOB 1.1 for template-declaration metadata. This does not qualify variadic or
partial-specialized objects, generic method templates, inherited/interface
objects, structured/public fields, general SDK reflection routing, or async
task ownership. Only the new focused gate is run; no full-suite or Linux pass
is claimed. The broader native interoperability sprint remains open.

The concrete-generic gate passed locally on Windows (1/1, 15.25 seconds),
including generated C++, VM CLI, live producer/host and negative qualifications.

## Concrete hosted inheritance and native interface views

An exact qualified concrete Wio object can now cross the native boundary even
when it inherits other Wio objects or implements interfaces. This extends the
earlier flat-object qualification; it does not make polymorphic ABI conversion
generally available. Every ancestor must be concrete, Wio-represented, and have
only non-public scalar/string/text fields. Existing whole-method and borrowed
result restrictions still apply. Cycles, open ancestors, public projections and
structured ancestor fields reject proxy eligibility.

For a `Leaf : Derived : Base` hierarchy, a native function receiving the exact
`Leaf` proxy can use C++ `Base*` and implemented interface pointers. Their
virtual method wrappers route back to the concrete VM implementation. An
unchanged inherited method keeps its declaration's stable identity across
different concrete reflection descriptors; the concrete type identities remain
distinct. `super` calls execute the selected base body without virtual
redispatch. Native template parameters absent from the argument types are
emitted explicitly rather than incorrectly relying on C++ deduction.

```wio
interface IRead { fn Read() -> i32; }

[From(IRead)]
object Base {
    seed: i32 = 5;
    public fn Read() -> i32 { return self.seed; }
}

[From(Base)]
object Derived {
    public fn Read() -> i32 { return super.Read() + 100; }
}

// Inspect receives exactly Derived; inside C++ it can use Base/IRead views.
[Native, CppName(engine::Inspect)]
fn Inspect(value: Derived) -> i32;
```

The VM still uses one intrusive object allocation. Cached owner-to-offset maps
translate owner-local WIR field indices, preventing a base field from aliasing
a derived field with the same local index. Repeated interface paths share one
logical owner. Base field initialization/default constructors run before the
concrete initializer; teardown runs in reverse owner order, releasing each
owner's fields after its destructor body. Base initialization plans are cached;
flat construction does not allocate the new inheritance plan.

Partial base construction tracks the completed-owner frontier in an existing
kind-specific storage slot, without another allocation. A failing base's body
is not destroyed, but completed ancestors and owned field values are cleaned.
Failure in the concrete object's explicit initializer/OnConstruct retains the
existing full-object cleanup contract. Hosted C++ placeholder construction uses
a distinct skip tag through all direct/virtual ancestors; creating or destroying
a proxy must not repeat the VM object's lifecycle. Ordinary generated C++ base
construction executes field initialization before OnConstruct.

The focused `wio_vm_native_object_hierarchy` gate compares 30 Derived and 30
Leaf lifetimes in generated C++ and actual VM CLI execution. The live host
checks inherited/overridden calls, base field separation, direct super calls,
stable declaration identities, secondary proxy identity, weak expiration and
exact lifecycle counts. Four VM constructor-failure cases cover incomplete
bases and concrete failure; these are VM-host checks, not differential C++
exception tests. Layout checks cover repeated interface paths, invalid/cyclic
bases, unpublished failed layouts and the 128-level VM boundary. All 33 existing
built-in alias spellings resolve to their canonical contracts; the Wio fixture
also mixes `[Native, CppName(...)]` and `[native, cpp::name(...)]`.
The combined compiler/runtime test translation unit also qualifies explicit
runtime Ref/WeakRef friend declarations independently of an outer `wio::Ref`
declaration and header include order.

Direct Base/interface-typed ABI parameters/results, arbitrary field
synchronization, broader signatures, foreign-object synchronization, SDK
reflection routing and async task ownership remain pending. Native wire-v2,
object-sidecar-v1 and WIOB 1.1 are unchanged. Only the new focused Windows gate
was run; no full-suite or Linux result is claimed. The native sprint remains open.

The final hierarchy/alias gate passed locally on Windows (1/1, 16.35 seconds).

## By-value hosted Base/interface ABI round trips

Qualified VM-created objects can now cross free-native parameters and results
declared as a Wio base object or implemented interface, including nullable
handles. This extends the concrete hierarchy slice above. Wio's explicit `fit`
conversion rule is unchanged; this does not add implicit subtype coercion.

```wio
[Native, CppName(engine::EchoBase)]
fn EchoBase(value: Base) -> Base;

[Native, CppName(engine::EchoReader)]
fn EchoReader(value: IRead) -> IRead;

let leaf = Leaf();
let base = EchoBase(leaf fit Base);
let reader = EchoReader(leaf fit IRead);
// Both returned handles still identify the same concrete Leaf.
if (base is Leaf) {
    let recovered = base fit Leaf;
    recovered.Set(23);
}
```

Boundary discovery walks a reverse inheritance graph to include qualified
concrete descendants even when no free-native declaration names those concrete
types. Abstract interfaces receive view codecs, not materialization adapters.
Whole-surface proxy restrictions are still applied to every concrete candidate.
Unrelated types are not included merely because an object boundary exists.

The wire continues to identify the concrete object, not the static Base/interface
view. Generated codecs validate the concrete producer handle operations before
upcasting its C++ Ref. Result encoding selects the most-derived qualified
concrete transport; it does not allocate a second object box, retag a Derived
pointer with a Base codec, or copy base fields. Exact-object transport remains a
separate `ExactObjectCodec` so polymorphic forwarding cannot recurse into its
own view codec. Codec definitions precede function/sidecar instantiations.

The VM checks cached concrete-owner layout membership against the declared
object/interface, selects the concrete adapter, and uses its existing host
recovery. Concrete stable ids have an indexed adapter lookup. The recovered
Value retains its dynamic aggregate type and original intrusive identity; the
weak proxy cache remains keyed by that identity rather than by each static view.
Non-null object decoding does not scan the reflection inventory for its static
view. Null remains tagged void with the nullable declaration's identity when
present, never a fabricated non-null object handle.

Native code can retain a Base Ref and an interface Ref beyond the caller's
lexical scope. These keep the same hosted object alive; releasing one must not
run lifecycle bodies while the other remains. Final release follows the normal
retired-host cleanup path and tears down the concrete hierarchy once.

`wio_vm_native_object_polymorphic` compares generated C++ and VM CLI execution
over 30 rounds of Base/interface and nullable round trips, dynamic type tests,
downcasts, shared pointer identity, interface writes, unrelated interface
implementations, null results and native-held claims. The live host verifies
unchanged VM identity/type, unchanged concrete wire operations/storage, virtual
dispatch, rejection of wrong siblings and incompatible native results, a missing
concrete adapter, and a forged typeId/operations pairing. The isolated producer
wire checks use a qualification host that explicitly owns hierarchy cleanup.

This slice is by-value transport of qualified hosted objects. Polymorphic
`ref/view` handle-cell rebinding and borrowed results need their own alias and
lifetime qualification. A returned producer-owned foreign subclass without a
recoverable VM host is rejected instead of silently becoming a sliced VM base;
existing exact foreign-object handling is unchanged. Cross-producer operation
identity, broader signatures, foreign field synchronization, SDK routing,
callbacks and async ownership remain separate work. Native wire-v2,
object-sidecar-v1 and WIOB 1.1 layouts are unchanged; older producers without
these codecs do not gain polymorphic support automatically. Only the new focused
Windows gate was run; no full-suite or Linux pass is claimed.

The polymorphic gate passed locally on Windows (1/1, 12.53 seconds), including
generated C++, actual VM CLI and the live producer/host checks.

## Polymorphic handle cells and caller-rooted method borrows

Qualified hosted objects can now inhabit `ref/view Base` and `ref/view IRead`
cells, including their nullable variants, while the inner object wire continues
to name the concrete Leaf/Sibling. Hosted methods can rebind such cells and
return a borrow rooted in a compatible reference parameter:

```wio
object Owner {
    public fn AssignReader(target: ref IRead, source: IRead) -> ref IRead {
        target = source;
        return target;
    }
    public fn ViewReader(target: view IRead) -> view IRead {
        return target;
    }
}
```

The returned reference wire identifies the **static contract** and points back
to the original caller's tagged-value cell. Its inner object wire identifies
the **concrete object** and retains the concrete producer operations. The
producer maps the result back to the caller's original Place; no local VM Place
or transient dispatch buffer escapes. Borrow provenance still requires matching
parameter storage and mutability, rather than accepting arbitrary references.

Repeated same-contract `ref/view` arguments share one cell, including view-first
then ref argument order. Distinct cells holding the same object stay distinct.
A shared cell cannot be interpreted simultaneously as Base and IRead, or as
nullable and non-nullable storage. Producer reference cells use a per-static-
storage contract key instead of comparing their concrete wire id with the
static declaration id. Native Frame storage mismatches report type mismatch,
not an incidental `std::bad_any_cast` exception. A view cannot gain rebinding
permission by being returned from a method.

Interface method signatures participate in whole-surface qualification. A
method-only Base/interface dependency discovers its concrete descendants;
abstract interfaces receive codecs but no materialization adapter. The VM
caches registered concrete adapters' ancestor contracts, avoiding an adapter
inventory scan for every hosted method parameter.

Inherited Base methods returning `ref/view Base` from `self` are supported on
a Leaf receiver. The owned temporary transport still carries Leaf identity;
the producer validates its host and maps it to a non-rebindable raw Base Place.
The temporary transport is released before the result leaves the wrapper.
Borrowing the receiver does not add a lasting intrusive claim.

The new `wio_vm_native_object_polymorphic_references` gate compares generated
C++, actual VM CLI execution and live hosted dispatch. It covers 30 rounds of
Leaf/Base and Leaf/Sibling rebinding, same-cell ref/view aliases in both orders,
independent cells, caller-rooted borrowed results, nullable/null transitions,
inherited receiver ref/view returns and unchanged strong counts,
readonly and malformed cells, incompatible/forged concrete transport, mixed
static contracts, mixed native nullable storage and exactly-once final cleanup.

This does not qualify explicit covariant receiver borrow declarations (for
example a method declared on Leaf returning `ref Base` from Leaf-typed self),
free-native reference results, foreign subclass state
synchronization or general callback/async borrowed lifetime support. Wire-v2,
object-sidecar-v1 and WIOB 1.1 formats are unchanged. Only this new focused gate
is run for this slice; no full-suite or Linux pass is implied.

The gate passed locally on Windows (1/1, 20.30 seconds), including generated
C++, actual VM CLI execution, selection checks and live host dispatch.

## VM-created any object payloads

Direct synchronous native `any`, `ref any` and `view any` boundaries now
support payloads whose Wio object family passes whole-surface hosted-object
qualification. This includes base/interface-typed payloads and concrete generic
objects with type and const arguments. An `any`-only native signature discovers
object adapters from module boxing, type-test and checked-cast instructions;
it does not require a redundant direct native object parameter.

```wio
[Native, CppName(host::Echo)]
fn Echo(value: any) -> any;
[Native, CppName(host::Rebind)]
fn Rebind(target: ref any, source: any);

let leaf = Leaf();
mut value: any = leaf fit IRead;
let alias: any = value;
let echoed = Echo(value);
// Downcasts recover the same concrete Leaf, not an interface placeholder.
let sameLeaf = (echoed fit Leaf) == leaf;
Rebind(ref value, Sibling() fit IRead);
let replaced = value is Sibling;
let aliasStillLeaf = (alias fit Leaf) == leaf;
```

The producer cell keeps its exact static payload storage type (Leaf, Base or
IRead). Its inner object wire still carries concrete Leaf identity and operations.
Payload descriptors match the exact producer cell type rather than competing
dynamic casts, so Base and interface adapters cannot both claim the same cell.
Decoded objects reconnect to their original VM host. Native-held Any cells keep
that host alive until their final release, and object mutation remains shared.

Object conversion context is explicit and call-local; it is not installed as
global state or retained by Any adapters. Registration requires the matching
object contracts before accepting object payload descriptors. Missing adapters,
forged concrete identities and incompatible producer operations fail instead of
creating foreign/sliced stand-ins.

Borrowed VM Any loads no longer acquire an owning register claim. Place writes
transfer an owned Any value, and replacement releases the previous value.
Owned `fit` operands are released after an independent owned cast result is
created; reference-valued casts keep their source lifetime dependency. These
rules keep lexical cleanup aligned with generated C++ instead of deferring it
until frame teardown.

The focused `wio_vm_native_object_any` gate compares generated C++ and actual VM
CLI execution, then exercises the live producer from a C++ host. It covers 30
rounds of concrete/base/interface payloads, type and const generic objects,
shared mutation, ref/view cell identity, whole-value replacement, self-assignment,
native retention, escaping values, missing object registration, forged wire
identity and exactly-once derived/base/generic destruction.

This does not qualify `std::Box<T>` payloads (the fixture's `Box` is an ordinary
user-defined object), arbitrary nullable payloads, nested Any or Any/object-bearing
containers, alias canonicalization, consumed arguments or async/callback
ownership. Native Step 8 remains open. Wire-v2, Any-sidecar-v1, object-sidecar-v1
and WIOB 1.1 formats are unchanged. Only the new focused Windows gate is run;
no full-suite or local Linux pass is implied.

The object-Any gate passed locally on Windows (1/1, 25.29 seconds), including
generated C++, actual VM CLI execution and live-host negative/lifecycle checks.

## Concrete generic POD payloads and containers

Any payload and dynamic array/dictionary adapter selection now shares
`WirNativePodEmitter::eligibleTypes` with physical POD layout emission. A POD
specialization is eligible because its arguments are concrete and its fields
are supported physical storage, not because its generic argument list is empty.
The container emitter collects this inventory lazily, only when a native
Any/container boundary actually needs a POD payload.

Qualified shapes include type/const arguments, default arguments, zero extents,
selected partial specializations, concrete variadic/empty argument packs, unused
string/text template arguments and nested/fixed-array POD fields. Open type or
extent parameters, pack-storage fields and string/non-POD physical fields remain
excluded. Unused template arguments are not treated as physical fields.

```wio
// Buffer<T, N> is a native POD component with values: [T; N].
let record = Buffer<i32>([1, 2, 3], Token());
mut boxed: any = record;
let alias: any = boxed;
Mutate(boxed); // changes the shared producer Any payload, not record
let changed = alias fit Buffer<i32, 3>;
let records: Buffer<i32>[] = [record];
let returned = EchoArray(records); // producer/VM value copies remain independent
```

Existing producer codecs and VM registered-layout checks materialize these
values. There is no generic-only wire representation and no runtime type guessing.
Stable specialization identities, opaque fields and immutable native prototypes
preserve visible and hidden producer state. Direct container ref updates retain
the existing same-length/same-key-set rule and update existing descendant cells;
growth inside an Any payload is observed through an owned `fit` copy instead.

The focused `wio_vm_native_any_generic` gate compares generated C++ and VM CLI
execution over 30 rounds: shared boxed mutation, ref replacement, specialization
identity, nested/zero-extent/variadic PODs, array/dictionary value isolation and
ref/view aliases, existing field borrows and hidden native state. C++ selection
checks reject unsupported/open POD Any and container adapters. Live-host checks
verify that an escaped Any, decoded POD, opaque child and backing-only prototype
each pin the producer library, with no lease left after final release.

The nested-constructor fixture also exposed and fixes a Typed WIR stale-pointer
bug: `buildExpressionAs` now reacquires its destination type after recursive
expression building can grow/reallocate the type table. A valid nested generic
POD-to-Any conversion must not fall into a spurious numeric-promotion diagnostic.

This does not qualify arbitrary nullable payloads, Any-bearing containers,
object containers, std::Box or callbacks/async ownership. Existing ABI/sidecar
and bytecode format versions are unchanged. Only the new focused Windows gate
is run; no full-suite or local Linux pass is implied.

The generic-POD Any/container gate passed locally on Windows (1/1, 30.78 seconds),
including generated C++, VM CLI, negative selection and live DLL-lease checks.

## Nullable owning handles in Any

Direct synchronous native Any/ref-Any/view-Any boundaries now qualify nullable
owning objects, interfaces, concrete generic objects and opaque handles. The
existing generated-C++ erasure rule is preserved: a null handle becomes null Any,
not a boxed typed-null payload. A present nullable handle uses the same payload
descriptor as its ordinary non-null type. A zero opaque pointer also becomes
null Any, even when returned through a non-nullable native `opaque` signature.

```wio
let missing: Leaf? = null;
let absent: any = missing;       // null Any
let present: Leaf? = Leaf();
let boxed: any = present;        // ordinary Leaf object payload
let returned = Echo(boxed);     // native Echo(any) -> any
let handle = returned fit Leaf?;
// absent is Leaf? is false; absent fit Leaf? fails rather than returning null.
```

Base/interface payloads retain their exact static producer cell type while the
object wire retains concrete identity. Nullable and ordinary spellings do not
emit competing descriptors for one cell. Checked casts/type tests canonicalize
the handle target, including `opaque?`; a null Any checked cast still reports
`WVM1184`. Semantic Any eligibility follows the same owning-handle rule.
Nullable reference/callback payloads remain rejected: boxing them would need a
separate borrow/callback lifetime contract. Scalar/string/component nullability
is not introduced by this change.

The fixture exposed two Typed WIR lowering gaps. Object `fit` now materializes
flow narrowing before casting an SSA value from nullable storage. Equality
between a nullable opaque handle and its inner type uses a common nullable
representation. Verifier requirements remain strict.

The focused `wio_vm_native_any_nullable` gate compares generated C++ and VM CLI
execution over 30 lifecycle rounds: null/present transitions, generic objects,
shared identity/mutation, base/interface payloads, ref rebinding, native retention
and opaque pointer round trips. Live-host checks cover escaping original VM
objects, null-Any cast failures, missing adapters, forged concrete identity and
final lifecycle balance. Two new semantic rejection fixtures keep nullable
reference/callback storage outside this surface.

The new Windows gate passed (1/1, 25.39 seconds). Existing ABI, sidecar and
bytecode format versions are unchanged. Any/object-bearing containers, nested
Any, std::Box, alias canonicalization, consumed arguments and broader foreign,
callback/async ownership remain separate work. Native Step 8 remains open;
no full-suite or local Linux pass is implied.

## Any and hosted object container values

Direct synchronous native dynamic arrays and dictionaries can now materialize
qualified Any cells and hosted object/interface/concrete generic object handles.
Nullable object array elements and nested Any arrays use the existing codecs;
dictionary keys retain their existing scalar/string/text/enum/flagset contract.
Object qualification still rejects open generics, public fields requiring native
synchronization and unsupported method/physical layouts. This is not an arbitrary
foreign object marshaller.

Object/Any discovery walks native container signatures iteratively, including
container-only boundaries. Recursive sequence/dictionary conversion forwards the
call-local `NativeObjectAccess` already used by direct Any payloads. Registration
merges container, object and Any sidecars into one candidate and validates the
complete graph before publishing it. A container of Any and an Any containing a
container can therefore reference each other's type-level adapters without a
partially registered graph.

Container value copies remain independent, but copied Any cells and object
handles retain their usual shared identity. Replacing an element in a copied
container does not replace the source element; mutating the shared payload does.

```wio
// Copy is the fixture's native Copy(any[]) -> any[] function.
let number: any = 20;
let label: any = "local";
mut values: any[] = [number, label, null];
let copied = Copy(values);
// Copy mutates the shared i32 cell and replaces only its own second element.
// number fit i32 == 30; values[1] fit string == "local";
// copied[1] fit i32 == 99.
```

Native container ref copy-back still requires matching sequence lengths and
dictionary key sets, and commits into existing cells so pre-existing element
borrows remain live. A container boxed in Any can grow; an owned `fit` observes
the changed producer payload without resizing the original value-copy source.
Independent container/element projection arguments remain a separate contract.

The qualification uncovered cleanup gaps in both backends. Typed WIR now releases
owned array/dictionary literal operands after the aggregate has acquired copies.
VM place initialization/replacement transfers owned array/dictionary values and
releases the overwritten container. Borrowed loads use live cell references,
not intrusive claims retained until frame teardown. Caller-rooted projections
are non-owning and are invalidated on root cleanup/unwind; an intrusive linked
list tracks only each root's projections with no extra allocation. Temporary-root
projection lifetime rules remain outside this new caller-rooted qualification.
Nullable object equality uses `IdentityEqual`; its verifiers accept nullable
object/interface operands without widening cast/type-test validation.

The focused `wio_vm_native_handle_containers` gate compares generated C++ and
actual VM CLI execution over 30 lifecycle rounds. It covers Any/nested arrays,
ordered/unordered dictionaries, object/base/interface/generic/nullable elements,
shared mutation versus container replacement, ref/view alias identity, existing
element borrows, boxed growth and native retention. A live host checks
container-only discovery, unsupported/open object selection, missing sidecars,
atomic malformed registration, forged second-element wire/partial decoding,
expired root borrows, original VM object recovery and final lifecycle balance.
Escaped Any children and backing-only handles also retain and finally release
their producer DLL lease.

Boxed Any-in-Any, std::Box, alias canonicalization, consumed arguments, arbitrary
foreign ownership and container/ref callback or async lifetime contracts remain
separate work. Existing ABI, sidecar and bytecode versions are unchanged; Native
Step 8 remains open. The new Windows gate passed (1/1, 35.08 seconds);
no full-suite or local Linux qualification is claimed.

## Free-native caller-bound ref/view results

A synchronous free native function can return a reference to an exact-compatible
caller reference parameter. The result's inner type must match the input cell's
semantic/ABI identity; numeric promotion, Base/Derived substitution, nullable
shape changes and readonly-to-mutable upgrades do not qualify.

```cpp
template<class T> T& Borrow(T& value) { return value; }
inline void Set(std::int32_t& target, std::int32_t value) { target = value; }
```

```wio
using cpp::header("my_borrow.h");
use std::attribute as attribute;
[Native, CppName(Borrow)]
fn Borrow(value: ref i32) -> ref i32;
[Native, CppName(Set)]
fn Set(target: ref i32, value: i32);

fn Forward(value: ref i32) -> ref i32 {
    return Borrow(value);
}

fn Entry() -> i32 {
    mut number = 7;
    let alias: ref i32 = Forward(ref number);
    Set(alias, 19);
    return number - 19;
}
```

Explicit ref/view variable annotations keep the borrow; ordinary inferred value
variables retain the language's automatic reference-read behavior.

Generated C++ evaluates the native call once and compares the returned address
against eligible input storage before returning the original caller Place.
Raw receiver/proxy storage does not qualify as a caller cell. Returning unrelated
producer/global/local storage throws instead of leaking its address into Wio.
The ABI thunk similarly checks the returned Place identity, then returns a
borrowed `WIO_NATIVE_ABI_REFERENCE` naming the input token. After copy-back, the
VM validates exact type/flags/size and a zero owner, matches the token address
without dereferencing foreign result pointers, and reuses the original caller
Place. It does not return a temporary thunk cell, create an owning value copy or
allocate a separate result Place. A result forwarded through Wio frames therefore
continues to observe the caller cell, including subsequent rebindings.

Native and indirect call lifetime analysis conservatively combines all matching
reference candidates. Returning a conditional native borrow is unsafe if any
eligible input is local/temporary, even if a constant condition appears to select
the caller-owned input. Declarations without a matching parameter or with only
readonly candidates for a mutable result fail semantic analysis.

The focused `wio_vm_native_free_borrow` gate compares C++ and actual VM execution
over 30 rounds, covering scalar/string/text, native POD hidden state, Any and
dynamic containers, hosted object/interface/concrete generic and nullable
object/opaque cells, conditional/same-cell aliases and Wio forwarding. Independent
field-only reference arguments can name caller storage. The live host rejects
unrelated storage, projected-overlap results, forged addresses/type IDs/flags/
size/owner generation and reads after explicit caller root cleanup. Nine new
semantic fixtures cover missing candidates, readonly upgrades, scalar/nested
contract mismatch, direct/indirect local candidate escape, narrowed nullable/
covariant cells and implicit native extension receivers. Bytecode verification
also rejects owned-result-style view narrowing on borrowed-result calls.

Projected overlap tokens still require a separate result projection mapping:
an input POD root and overlapping field can be passed together, but its projected
field token cannot be returned through this new free-native result contract.
Implicit native receivers, async borrowed results, arbitrary foreign storage and
SDK-exported ref/view results remain unsupported. Existing wire/sidecar/bytecode
versions are unchanged; this does not close Native Step 8 or claim full-suite/
Linux qualification.

Only the ten new focused tests were run locally on Windows: 10/10 passed in
18.32 seconds total; the C++/VM/live-host gate took 17.12 seconds.
