# N04 ByteBuffer bulk writes

Implementation slice, 2026-10-20. N04 remains open for the remaining I/O/buffer
inventory; N14 timing acceptance and N05 representation work are not closed.

## API and semantic boundary

`ByteBuffer.WriteBlock(values: view byte[])` borrows a binary block and writes
it at the current position. It overwrites existing bytes, appends the remaining
tail, preserves untouched suffix bytes, and advances position by the count.
An empty block returns before any native call. The source is not modified or
retained. Reserved native storage is reused when the result fits.

```wio
let payload: byte[] = [1u8, 2u8, 3u8, 4u8];
let buffer = heap::ByteBuffer(1024usize);
buffer.WriteBlock(ref payload);
buffer.Rewind();
buffer.WriteBlock(ref payload);
```

The old `Write(byte[])` is **unchanged**: it takes an owned snapshot and calls
the virtual scalar `Write(byte)` for every element. A derived object can override
that scalar hook. Replacing the old loop unconditionally would silently bypass
valid user behavior, so this slice does not do that. WriteBlock explicitly has
block semantics and does not call per-byte hooks, including on derived receivers.
Existing Write callers do not automatically gain the new block performance.

`Clone` can safely use WriteBlock: its destination is a newly constructed exact
ByteBuffer, not a user-derived receiver. Clone still isolates storage, preserves
position and requests the source capacity. It no longer walks the scalar Write
loop when copying bytes. This is an implementation improvement for existing
Clone callers, without claiming an unmeasured Clone speedup percentage.

No RTTI check, owning self-handle guard, new ByteBuffer data field, object-layout
rewrite or native-object proxy requirement is introduced. Tests preserve legacy
derived hooks and exercise block writes during finalization with exactly-once
cleanup. Public new method surface is intentional; this is not a binary-ABI
compatibility guarantee for independently compiled generated object vtables.

## Native bulk algorithm

The helper takes a mutable vector, a position and a readonly source vector.
It checks the position and `count > max_size - position` before addition or
mutation. For distinct vectors it inserts only the known tail, completing any
throwing growth first, then memcpy overwrites the old prefix. It avoids
resize/zero-fill and per-element method calls. For native self-alias it captures
the original count, resizes, reacquires the pointer, and memmove copies snapshot
bytes even after reallocation. Empty input performs no pointer arithmetic.

Native allocation failure leaves destination bytes, size, capacity and pointer
unchanged; the Wio position assignment occurs only after a successful helper
return. Fault injection tests both overlapping alias and nonalias growth.
Unreserved growth policy can differ from repeated Push calls; no exact implicit
capacity-growth sequence is promised. Explicit reserve guarantees remain.

## VM reserve consistency fix

The new gate exposed an existing bridge issue: structural native array copy-back
replaced the VM array with a decoded array whose capacity equaled its new length.
Thus a native block write could lose a previous Reserve(16) despite writing only
four elements. Same-shape copy-back already preserved existing storage.

The copy-back planner now prepares at least the caller's old capacity in the
incoming shape-changing array **before commit**, detaching shared incoming
storage first. It does not change caller values, size, capacity or cell addresses
during planning. Shape-changing arrays have no child actions into that subtree.
Existing target exclusivity, depth, staged validation, projection retirement and
cleanup rules stay intact. New focused host cases check reserved growth, native
clear, shared-incoming isolation and unchanged caller storage before commit.

This is a VM correctness fix required by the new buffer path, not the heavy VM
optimization pass. The bridge still marshals arrays and may allocate replacement
storage; logical capacity preservation does not mean physical storage reuse or
zero-copy VM transport. No general transactional promise is added for arbitrary
VM marshalling/allocation failure.

## Correctness coverage

The new gate checks native O0/O2 and both bridge levels: fill, overwrite, extension,
empty input, reserve, unchanged source/tail, Clone isolation, legacy subclass
hooks, explicit block behavior on subclasses, finalizer calls and exactly-once
cleanup. The native helper is compared against the scalar algorithm for 23001
distinct-input combinations and 1122 alias cases, with and without reserve.
257 reserved helper cycles (three block writes each) perform zero allocations.
Separate bad_alloc injection verifies native growth failure without mutation.

The affected native-container-shape gate also passes locally (25.64s) after
updating the previously stale local Release compiler executable. An earlier
attempt used that stale executable and failed at its unsupported fit surface;
it was not a capacity-fix regression. The final new gate passes all 17 processes
locally, including both VM bridge levels; raw process records are retained in
`docs/evidence/n04-buffer-bulk-contract-2026-10-20.json`. Three-platform CI passes
after the driver correction below. Only new/affected local gates run.

## Measurement contract

The baseline was frozen before production changes. Its Wio fixture uses legacy
Write. The new fixture changes exactly three call sites to WriteBlock(ref source).
This is an **API-strategy/update comparison**, not an automatic unchanged-source
compiler speedup. Each iteration clears a reserved 2048-byte destination, writes
1024 bytes, seeks to 512 and overwrites/extends, then rewinds and overwrites again.
Final count/position are 1536/1024. Full output and state are checked; ToArray and
full-byte validation remain outside kernel/counter windows. Work input snapshots
are owned in all languages, while block arguments are borrowed. The C++/C# lanes
are current block references, not measurements of the old per-byte-hook API.

The two native preparations retain their recorded runtime archive identities
under the same O2 profile; the header update caused an archive rebuild. Both
frozen bytecode versions run under the same capacity-corrected VM host. Keep
these input differences visible rather than attributing every delta to a single
compiler transformation. The new session uses 1000 native/C++/C# and 20 VM samples,
native A/A lanes, a separate final Wio phase and append-only raw rows. No retries,
trimming, threshold relaxation, power changes or overlapping heavy local jobs.

Fixture and preparation details: `tests/performance/byte_buffer_bulk/README.md`.
Uncommitted user std/playground edits are preserved and excluded from commits.

## Measured native allocation window

The separately instrumented generated images execute 17 iterations (51 block
writes), after two workload warmups. The destination is reserved before the
window; Work's owned 1024-byte source snapshot remains inside the window in
both images. Full output snapshots/checks remain outside it.

| Global new/delete diagnostic | Legacy Write | WriteBlock |
| --- | ---: | ---: |
| Successful allocations / frees | 105 / 105 | 3 / 3 |
| Requested / freed bytes | 55776 / 55776 | 1104 / 1104 |
| Peak live bytes | 2176 | 1104 |
| Peak live blocks | 5 | 3 |
| End live bytes / blocks | 0 / 0 | 0 / 0 |
| Allocation failures | 0 | 0 |

This removes 102 allocations and 54672 requested bytes from this exact window,
not from process startup or all heap activity. Ordinary global new/delete
accounting is not RSS, malloc traffic, a copy-byte counter or VM allocation
accounting. The reserved native helper itself has zero heap traffic in its
focused contract; the full Wio Work call still has the three allocations above.

## CI portability correction

The first production run on 14d381f passes the new N04 block gate on Ubuntu and
macOS; macOS completes successfully. Ubuntu subsequently exposes an existing
CMake differential-driver issue: in script mode CMP0054 was unspecified, so
older CMake interpreted the quoted `"native"` mode label as the variable holding
the output path and built the shared variant twice. The VM program succeeded,
but the native executable did not exist. This is not a passing full CI run.

Commit bf98249 makes CMP0054 NEW explicit within a pushed/popped policy scope
in `tests/cmake/vm_native_pod_layout_test.cmake`. It preserves the native/VM
comparison instead of dropping it or accepting a missing executable. The fix
does not rebuild or change this session's frozen measurement images. The
follow-up three-platform result is recorded with the final evidence below.

The corrected [CI run 39348265979](https://github.com/tynes0/wio/actions/runs/39348265979)
finishes **success** on source bf98249 (production 14d381f plus the test-driver
fix). Windows Debug, Ubuntu Release and macOS 15 Release each pass the new N04
block gate, affected native-container-shape/reserve gate and sampling/journal
contract, along with their configured N03/N04 checks. The full historical suite
and performance timing acceptance are not claimed. The affected local shape
gate also passes again after the policy correction (25.06s).

| Platform | Job | Conclusion |
| --- | ---: | --- |
| Windows / Debug | 118386157102 | success |
| Ubuntu / Release | 118386156804 | success |
| macOS 15 / Release | 118386157164 | success |

[CI evidence](evidence/n04-buffer-bulk-ci-2026-10-20.json) retains both the first
run's failed/cancelled platform outcomes and the corrected source/run/job/step
identities. It does not relabel the original run green.

## Completed legacy/block measurement

The Wio-written controller finishes **10094/10094 correct raw rows**: comparison
6006 native/reference rows plus 42 VM rows, then a separate final phase with
4004 native rows and 42 VM rows. Totals include one excluded process warmup per
lane and the native A/A controls. Each measured native/C++/C# lane has 1000
samples; each VM lane has 20. Both frozen bytecode programs use the same updated
Release VM host. Native/bridge builds use O2 with FP contraction disabled.

Comparison observations, milliseconds, **not timing-qualified**:

| Implementation / API | Median | SD | MAD | p95 | p99 | Median process peak RSS, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Wio native / legacy Write | 12.80880 | 1.83956 | 0.62450 | 15.72685 | 20.17421 | 9192 |
| Wio native / WriteBlock | 0.02020 | 0.01070 | 0.00030 | 0.02570 | 0.05515 | 9168 |
| C++ / block reference | 0.00100 | 0.00082 | 0 | 0.00110 | 0.00140 | 5200 |
| C# / block reference | 0.00090 | 0.00077 | 0 | 0.00100 | 0.00140 | 24036 |
| Wio VM / legacy Write | 212.98645 | 13.07687 | 5.30495 | 234.55939 | 254.58060 | 18060 |
| Wio VM / WriteBlock | 13.65755 | 2.10465 | 0.89375 | 17.85929 | 19.83202 | 18172 |

Separate final Wio observations, also **not timing-qualified**:

| Implementation / API | Median, ms | SD, ms | p95, ms | p99, ms | Median process peak RSS, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Wio native / legacy Write | 12.81690 | 2.12190 | 15.63098 | 21.15175 | 9192 |
| Wio native / WriteBlock | 0.02010 | 0.04679 | 0.02121 | 0.05229 | 9168 |
| Wio VM / legacy Write | 207.47290 | 9.23677 | 222.44628 | 224.64990 | 18060 |
| Wio VM / WriteBlock | 13.02950 | 1.45372 | 16.40628 | 16.56094 | 18168 |

The median native process RSS difference is only 24 KiB, not a dramatic process
memory reduction. VM process RSS rises by 112 KiB in comparison and 108 KiB in
the final phase. These whole-process peaks do not count hot-window payload
copies or prove VM allocator savings; preserved reserve and bridge marshalling
remain visible costs. No VM allocation metric is invented from native counters.

Frozen artifact sizes from the endpoint records, bytes:

| Artifact | Legacy preparation | Block preparation | Change |
| --- | ---: | ---: | ---: |
| Generated C++ | 3516518 | 3522191 | +5673 |
| Native executable | 5721017 | 5729527 | +8510 |
| Bytecode | 2764249 | 2766281 | +2032 |

The added API/helper/metadata is not a binary-size optimization. These exact
image sizes include their recorded build/runtime inputs, not a claim about all
programs or an isolated helper's machine-code size.

The new native median remains about 20.2x the tiny C++ block reference and
22.4x the C# reference in the **same comparison phase**. This does not satisfy
the native 1.05-1.10x goal. Different object/struct, RC/GC, dispatch and capacity
machinery are explicit reference boundaries, not evidence that the remaining
gap is acceptable. Call/representation/RC costs still need their planned passes;
this session does not isolate each cost or prove an optimization for them.
Static inspection of the frozen generated WriteBlock wrapper still shows the
general Place/std::optional field-borrow path. Removing per-byte payload copies
does not remove that representation. This is a concrete N05/N08 follow-up, not
a measured attribution of the entire remaining gap to optional storage.

Affinity is applied to every child process, input hashes match at both ends,
and the power profile is unchanged. A/A median differences are below 0.64%;
all drift checks stay below 10%. Nevertheless **every summary fails the
unchanged 15% full-range gate**. The final block-native range is 72.38 times
its median, despite a 0.00020ms MAD: rare long interruptions and a very short
kernel matter. Their exact system cause is not established by these samples.
Clock quantization is visible in the sub-microsecond reference distribution;
zero MAD is not proof of zero noise. The controller correctly exits 2 for
unqualified timing, not incorrect output. No samples are trimmed, thresholds
relaxed, retries substituted or selected best-case ratios reported. More samples
alone did not qualify this environment. N14 acceptance remains open.

This result must not be compared numerically with the previous file/range-write
pass: that is a different workload. The frozen legacy image above is the
same-output before image for this API-strategy comparison. An unchanged-source
ByteBuffer.Write caller retains its old hook/copy behavior.

## Raw evidence and independent verification

- [Raw-data ZIP](evidence/n04-buffer-bulk-raw-2026-10-20.zip), 491395 bytes,
  SHA-256 `8d71c06e3be171f5c78a0db210be7f041f525f337dcc95aead4bc63c482e945b`.
- [Independent summary](evidence/n04-buffer-bulk-summary-2026-10-20.json) and
  [extracted-archive verification](evidence/n04-buffer-bulk-archive-check-2026-10-20.json).
- [Standalone verifier](evidence/n04-buffer-bulk-verify-2026-10-20.py).
- [New 17-process correctness gate](evidence/n04-buffer-bulk-contract-2026-10-20.json),
  [affected shape gate](evidence/n04-buffer-bulk-shape-2026-10-20.xml) and
  [shape gate after CMake policy fix](evidence/n04-buffer-bulk-policy-2026-10-20.xml).

The independent verifier recomputes the full output fingerprint, budgets,
warmups/controls/rounds, journal equality, medians, mean/SD/SE, MAD, quantiles,
range/drift/A-A qualification, RSS medians and endpoint conditions. ZIP CRC
testing and verification after fresh extraction also pass. Extraction additionally
checks both preparation manifests/header hashes, exactly three API replacements,
identical payload oracle and both balanced allocation windows. The initial
summary's memory field is null because the live result folder has no embedded
preparations; the extracted summary verifies those archived counters explicitly.

The ZIP retains raw results, samples, reports, frozen workload/reference sources,
build commands, preparation manifests/counters and helper-header inputs. It is
an audit bundle, **not a hermetic rebuild kit**: compiler/runtime binaries and
generated C++ are not embedded. Uncommitted user buffer text is represented by
its frozen hash, not redistributed. Original hashes/paths remain provenance,
not a claim that an extracted path has the original binaries.

## Remaining N04 scope

This closes the explicit ByteBuffer block/Clone/reserve slice, not all N04 work.
The existing inventory still needs byte-pool/ByteWriter snapshot and reuse
review, read-all/filesystem/encoding round trips, streaming/large-file and
batched-I/O failure/syscall checks, and the coordinated std Result-first/style
cleanup. The new byte path does not silently migrate those public APIs. N05
representation work has not started; N14 still owns timing acceptance.
