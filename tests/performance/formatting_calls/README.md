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
Object-containing containers, async frames, native/generic hook declarations,
ambiguous/private hooks and unsupported mixed surfaces are not enabled by this
bounded pass. The user-container WVM1062 fixture stays an explicit negative gate.
