# N02 concat closure

Wio owns the workload and sample collection. Python only prepares serial builds,
checks the independent byte-level FNV oracle and collects separate diagnostic
allocation output. It does not collect timing samples.

```sh
python tests/performance/concat_reuse/prepare.py --output build/n02-before --label before
build/tools/wio-bench/Debug/wio-bench run build/n02-before/manifest.json --output build/n02-before-results
# Implement the scoped change, rebuild Wio, then use the EXACT frozen sources:
python tests/performance/concat_reuse/prepare.py --output build/n02-after --label after --sources build/n02-before
build/tools/wio-bench/Debug/wio-bench run build/n02-after/manifest.json --output build/n02-after-results
build/tools/wio-bench/Debug/wio-bench compare build/n02-before-results/results.json build/n02-after-results/results.json
```

Use `.exe` on Windows. Tool/compiler/runtime/VM paths, C++ compiler, CPU,
repetition count and runtime workload count have explicit options (`--help`).
Prefer a Release VM host. A correct but noisy run exits 2; keep it. Do not retry
until timing gates pass. Output directories must be new. `--reuse-build` is only
an explicit recovery of a prepared build; never label changed-source reuse a new
build. Build commands, source and executable hashes are retained. The independent
oracle includes all UTF-8 bytes, embedded NUL, combining marks, emoji and Japanese.

The four workload groups are growing `value = value + source` for byte string
and Unicode text, and six-part concat trees for both types. Default count 2001
is odd so XOR fingerprints of repeated chain results do not collapse to zero.
All lanes perform the same full-result FNV hash. The native hash contains a
compiler memory fence; Wio uses the same hash through the native view boundary.
C# uses a no-inline hash and method, Release/net9.0 with tiering disabled. Its
UTF-16 strings require a UTF-8 byte-array allocation for the hash; this real cost
is included and is NOT described as identical storage/allocator semantics.
Wio/C++ use UTF-8 bytes. No implementation reserves output capacity manually.

All programs take runtime inputs and perform two in-process warmups. The Wio
controller then performs rotating/reversing comparison rounds, native exact-
command A/A controls, and a distinct final native/VM phase. CPU/peak RSS are
whole-root-process OS metrics; kernel timings include concat, hashing and local
cleanup, not parsing/printing/startup. Memory diagnostics are separate opt-in
global-new/delete windows, not ordinary timing or VM heap counters. All tracked
blocks must be released before the window ends.

Only the new `wio_n02_concat_{plan,runtime,semantics}` CTests are the local gate.
Wio semantics cover O0/O2 and VM; C++ proof tests cover cleanup, type/ownership,
CFG and effect rejection. The runtime test includes self-alias, SSO/growth,
embedded NUL, text rvalue overloads and allocation-failure rollback.

`barrier.wio` is explicitly native-only compatibility coverage for an existing
borrowed-LHS/mutating-RHS discrepancy; it is not VM parity evidence. The shared
test uses an explicit snapshot where mutation is involved. See KI-007 in
`docs/WIO_KNOWN_ISSUES.md`; N02 must not silently change that language behavior.
