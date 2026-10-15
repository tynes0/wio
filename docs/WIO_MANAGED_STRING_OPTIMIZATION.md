# Native String Storage and Adjacent Append Copies

Date: 2026-10-15. Implementation: `79568bd`. This is another bounded slice of
optimization pass 1, following the [builder/std work](WIO_STD_BUILDERS_CONSOLIDATION.md),
not completion of native, VM or compiler-throughput optimization.

## What changed

Proven nonescaping synchronous string cells now use `InlinePlace<std::string>`
instead of a shared heap-backed Place. The installed `wir_inline_place.h` owns
the string in a union with an explicit initialized bit. There is no optional,
function wrapper or shared allocation for this cell. Uninitialized reads still
fail; clear/drop, reinitialization and move-out preserve destruction semantics.
Clearing marks the cell empty before destruction; failed construction does not
publish a live cell. The helper is tested with throwing/resource-owning types,
but actual codegen promotion in this slice is restricted to strings.

The storage plan rejects escaping, borrowed, captured, projected, passed or
branch-transferred cells, heap places and async/coroutine functions. Owned
move-out is supported through `take()`. These are conservative proof rules,
not a new public string representation or lifetime-extension contract.
Escaping string references retain the full Place machinery. Object handles,
components, arrays, Unicode text and coroutine frames are not broadly promoted.

A second proof recognizes an adjacent borrowed string load, owned copy and
in-place string Append. If that copy has exactly one use, as the append suffix,
emission borrows the loaded string directly rather than materializing the owned
copy. Other calls/selectors, explicit cleanup users, intervening instructions,
multiple uses and other blocks are excluded. Reallocating self-append retains
the underlying string append semantics. Canonical WIR identities/metadata and
function signatures/ABI do not change; owned value snapshots remain independent.

This is not a blanket removal of owned string temporaries or `std::optional`
from generated code. Remaining optional SSA and full Place paths still exist
where this proof does not apply. No changes were made to user-owned playground
or unrelated std edits, including `std/buffer.wio`.

## Paired append evidence

The existing versioned benchmark appends a 32-byte fragment 30,000 times and
takes a final independent snapshot. All 64 executions (one warm-up and 15
measured rounds per variant) produce 960,000 bytes and FNV-1a
`2058931307218008101`. Cases are interleaved; native compilation, process startup,
checksum computation and reporting are outside the internal timer. Snapshot
construction and the ordinary global new/new[] counter are inside it. This
counter is not an allocator-byte, malloc, aligned-allocation or peak-RSS measure.

| Variant | Median | Range | new/new[] calls |
| --- | ---: | ---: | ---: |
| Original saved generated source, pre-builder slice | 543.757 ms | 519.181–557.940 ms | 120,004 |
| Previous std-optimized generated source, `ddfa2b2` | 5.095 ms | 4.852–5.444 ms | 90,020 |
| Current generated source, `79568bd` | 3.221 ms | 3.116–3.371 ms | 30,020 |
| C++ reference | 0.552 ms | 0.522–0.612 ms | 17 |

Current is **1.582x faster than the previous version** (36.78% less measured
time), and **168.81x faster than the original** within this paired session.
Allocation calls fall by 60,000 from the previous version, matching removal of
the parameter cell allocation and adjacent append snapshot for each call.
The final C++ gap is still **5.84x**: the 1.05–1.10x target is not met here.
An owning 32-byte argument still requires roughly one allocation per append;
literal/argument transport, further copy elimination and direct numeric/buffer
formatting remain hot-path candidates. Their semantics must be proven before
changing the public value-argument contract.

The old report's original 389.139 ms and previous 4.897 ms belong to another
session. Use the paired values above to calculate this slice's improvement,
not cross-session absolute timing changes. Scalar-kernel gains must not be
multiplied into these numbers. The full Mandelbrot PPM was not rerun in this
slice; its previous completion/hash evidence remains historical evidence for
that revision. No new C#/Python/VM throughput result is claimed.

Environment: Windows build 26100, Intel Core i7-13620H, MinGW GCC 15.2.0,
C++20 `-O2 -ffp-contract=off -static`, no fast-math. Every generated variant was
recompiled using the same current toolchain, headers and runtime archive;
saved sources were not run as stale binaries. CPU affinity/power policy was not
pinned. Local generation retains pre-existing unrelated std worktree changes;
clean remote correctness evidence below is separate from local performance.
Compiler/runtime/source/generated/binary hashes and all samples are in
[the raw evidence](evidence/managed-strings-2026-10-15.json).

```powershell
pwsh -File tests/performance/std_builders/run.ps1 `
    -OutputDirectory F:/Projects/wio/build/managed-strings-new-run `
    -BaselineDirectory F:/Projects/wio/build/std-builders-baseline `
    -PreviousDirectory F:/Projects/wio/build/std-builders-ddfa2b2 `
    -Repetitions 15
```

The two input directories must contain their saved `append.wio.cpp`. The runner
keeps all compiled variants and measurements in a fresh output directory and
rejects overwriting an existing directory. This local paired experiment does
not claim the ignored historical snapshots are a clean-checkout fixture bundle.

## Correctness qualification and remaining lifetime boundary

Only the two new tests run for this slice:

- `wio_wir_cpp_managed_plan`: conservative admission/rejection and append-copy
  proof; initialized/empty/reinitialized reads; move/take/self-move; exactly-once
  resource deletion through clear, transfer and unwinding; throwing construction
  and assignment without phantom live state.
- `wio_wir_cpp_managed_storage`: actual generated native O0 and O2, then actual
  bytecode plus native bridge. Native probes require zero allocation calls for
  1,000 short-string echo calls. Scoped ref/native mutation, returned owning
  capture snapshots, early exits, Result propagation, long payloads, builder
  snapshots/clear/self-append and reallocating direct self-append are checked.
  The native bridge counter does not measure VM heap allocation; this is not
  a zero-allocation claim for the VM.

Local Windows Debug: **2/2 passed in 29.92 seconds**. Clean CI builds the real
Wio CLI at `79568bd` and passes both new tests on all three platforms:

| Platform/configuration | New tests | CTest duration | Evidence |
| --- | --- | ---: | --- |
| Windows / Debug | 2/2 passed | 43.10 s | [job](https://github.com/tynes0/wio/actions/runs/38710994109/job/116342024018) |
| Linux / Release | 2/2 passed | 29.52 s | [job](https://github.com/tynes0/wio/actions/runs/38710994109/job/116342023990) |
| macOS / Release | 2/2 passed | 24.26 s | [job](https://github.com/tynes0/wio/actions/runs/38710994109/job/116342024129) |

The [complete run](https://github.com/tynes0/wio/actions/runs/38710994109) is
successful: **6/6 new test executions**. These durations are correctness evidence,
not remote performance measurements. The old full corpus is not rerun, following
the scoped testing policy. Diagnostic reference validation remains 693 IDs,
three dynamic-only entries and 49 committed std Result construction sites.

The tests also exposed [KI-006](WIO_KNOWN_ISSUES.md#ki-006--escaping-local-ref-capture-reaches-an-expired-cell-p1-diagnosticcontract-gap):
analysis accepts a returned lambda borrowing a local string, but reading after
the owner's scope ends throws `read from uninitialized Wio place`. The minimal
reproducer uses the preserved full Place path, not InlinePlace. Borrowing does
not promise to extend local lifetime; the open issue is the rejection/diagnostic
boundary. The valid scoped borrow and returned owning capture are tested
separately. This performance slice does not fix the escaping-borrow diagnostic
or claim memory corruption from that checked runtime rejection.

## Next boundaries

Continue pass 1 with measured argument/copy/formatting costs and coordinated
std modules. VM runtime remains pass 2 and compile/preparation throughput pass 3.
After all three, repeat the same Mandelbrot workload across C++, Wio native,
Wio VM, C# and Python, separating compute, formatting/buffer construction and
complete PPM output with matching hashes and versioned original/previous/final
results. That requested final comparison is an acceptance gate, not a fourth
optimization pass. No release or final performance closure is claimed here.
