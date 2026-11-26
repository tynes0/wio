# N03 legacy stream-result ownership

This is deliberately the native compatibility `wio::runtime::Format("{}", ...)`
template, reached by a test-only native Wio wrapper. It is **not** the normal
interpolation path or the indexed `std::console::Format("{0}", ...)` API. The VM
lane invokes the same native helper through the bridge; improvements there are
not interpreter dispatch optimizations or closure of KI-008 user hooks.

Two counted ASCII/NUL payloads (192 and 8192 bytes) are formatted into an owning
string with a runtime decimal index and lowercase boolean. Each result is fully
hashed; input construction, formatting, hash and cleanup are inside the kernel.
All lanes have two in-process warmups. C++ is a frozen stream + owning-copy
reference, not an optimized rvalue-extraction lower bound. C# uses a UTF-16
StringBuilder and a UTF-8 hash allocation; these costs are real but not identical
allocation/encoding contracts. Python independently verifies every checksum.

```sh
python tests/performance/stream_ownership/prepare.py --output build/stream-own-before --label before --count 2001
build/tools/wio-bench/Debug/wio-bench run build/stream-own-before/manifest.json --output build/stream-own-before-results
# Apply the scoped runtime change, keeping the workload sources frozen.
python tests/performance/stream_ownership/prepare.py --output build/stream-own-after --sources build/stream-own-before --label after --count 2001
build/tools/wio-bench/Debug/wio-bench run build/stream-own-after/manifest.json --output build/stream-own-after-results
```

Use the `.exe` suffix on Windows. See `--help` for explicit tools, CPU, count and
paths. The Wio controller owns comparison samples, native A/A and the separate
final native/VM phase. Preserve failed and timing-unqualified samples. Memory is
a separate opt-in replacement-new diagnostic, not production timing or all malloc
traffic. Result capacity is a separate retained-memory tradeoff: taking a stream's
buffer may keep more spare bytes than copying exactly its logical contents.
On Windows the diagnostic statically links libstdc++/libgcc so stream allocations
and replacement delete share an image. Ordinary timing binaries keep their normal
linkage. An explicit `--reuse-build --rebuild-diagnostics` can repair/rebuild just
that separate executable while appending its command to the retained build log;
it must not be used to claim changed production sources were rebuilt.

`wio_n03_stream_ownership` checks extraction against the owning-copy implementation
at SSO/growth boundaries, including counted NUL, seek/high-water output, badbit
partial output, throwing operators, stateful ToString hooks, containers, live
locale and rounding modes. It also measures allocations, requested/peak bytes and
returned capacity. `wio_n03_stream_ownership_semantics` runs Wio native O0/O2 and
VM bridge checks for NUL/Unicode, integer boundaries and independent snapshots.

This narrow compatibility pass does not migrate other format APIs to streams or
prove their complete conformance. Native generated generic stream fallback,
remaining KI-008 surfaces and N01/N14 timing qualification remain separate.
