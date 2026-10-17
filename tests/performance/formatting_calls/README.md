# N03 shared scalar formatting calls

The workload and benchmark controller are Wio. Python performs serial builds and
computes independent UTF-8 FNV fingerprints; it does not own timing samples.

Two cases, with exactly one Counter object per kernel and stateful ToString:

- `single-hook`: `<item:n>`, one hook per iteration.
- `ordered-hooks`: `index:item:n|item:n+1:true`, two ordered hooks per iteration.

C++ uses a shared Counter with a virtual ToString and stack-buffer integer
conversion. C# uses a class override, invariant integer conversion and real
UTF-16/UTF-8 allocation costs. These are equivalent outputs and hook counts, not
identical ownership/allocator layouts. No formatter is permitted to precompute or
skip the stateful calls. Both references explicitly sequence their hooks.

```sh
python tests/performance/formatting_calls/prepare.py --output build/hooks-before --label before --count 10001 --wio /absolute/path/to/before/wio
build/tools/wio-bench/Debug/wio-bench run build/hooks-before/manifest.json --output build/hooks-before-results
python tests/performance/formatting_calls/prepare.py --output build/hooks-after --label after --count 10001 --sources build/hooks-before
build/tools/wio-bench/Debug/wio-bench run build/hooks-after/manifest.json --output build/hooks-after-results
```

Use the platform-specific compiler/VM/runtime/controller executable paths.
Keep failed baseline VM processes: unsupported WVM1062 is not a zero-time sample
or a valid before/after VM speedup denominator. The controller includes A/A
controls, reversed/rotating comparison rounds and a separate final native/VM
phase. All samples remain, without retries or trimming. Allocation diagnostics
are separate global-new/delete windows, not production timing or all malloc/VM
heap traffic. RSS and C# managed-byte counters have their own scopes.

New-only gates are `wio_n03_formatting_call_plan` and
`wio_n03_formatting_call_semantics`. Wio native O0/O2 and VM check evaluation
before formatting, exactly-once/left-to-right hooks, NUL, temporary receivers,
generic interpolation/std::ToString, view receivers and interface dispatch.
The IR proof checks null-safe CFG, cleanup, idempotence and conservative rejection.
The new `wio_n03_native_string_borrow_plan` proof and `borrow.wio` extend this
with bounded native string borrow transport: stack cells/private reference
adapters, mutable growth, NUL, independent snapshots, scalar arguments and
exception translation. Native function values retain the ordinary ABI and pass
O0/O2; VM indirect native invocation still reports WVM1003 (KI-009), an explicit
negative boundary separate from passing direct VM calls. Unicode hook output is
also checked. See [the transport report](../../../docs/WIO_NATIVE_STRING_BORROW_OPTIMIZATION.md).

`wio_n03_scalar_field_plan` proves bounded direct primitive field intervals, and
`fields.wio` checks object/component-extension mutation, bool/f64 fields and the
ordinary transport for a field ref passed to another function. It also checks long/repeated/NUL
string interpolation and Unicode Text after checked pre-reserve. The new
`wio_n03_formatted_size` unit covers maximum/overflow arithmetic. These are C++
emission optimizations; they add no WIR/VM instructions. See
[the field/reservation report](../../../docs/WIO_SCALAR_FIELD_OPTIMIZATION.md).

Object-containing containers, async frames, native/generic hook declarations,
ambiguous/private hooks and unsupported mixed surfaces are not enabled by this
bounded pass. The user-container WVM1062 fixture stays an explicit negative gate.
