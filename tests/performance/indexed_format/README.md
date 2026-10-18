# Indexed format benchmark and scratch contract

Wio owns the workload and timing controller. Python prepares frozen artifacts
and independently checks full-byte FNV outputs. Two modes use 10001 iterations:
indexed D8/X8 and indexed F3 plus string alignment. C++ specializes these known
formats with to_chars, without an indexed parser; C# uses invariant string.Format
and an allocating UTF-8 hash. These are output-equivalent, not identical runtime
or allocation contracts.

```sh
python tests/performance/indexed_format/prepare.py --output build/indexed-before --label before --count 10001
build/tools/wio-bench/Debug/wio-bench run build/indexed-before/manifest.json --output build/indexed-before-results
# Apply the candidate and rebuild the compiler, backend runtime and VM runner.
python tests/performance/indexed_format/prepare.py --output build/indexed-after --sources build/indexed-before --label after --count 10001
build/tools/wio-bench/Debug/wio-bench run build/indexed-after/manifest.json --output build/indexed-after-results
```

Use platform-specific paths/extensions or the shared preparation flags. Builds,
correctness gates and measurements are serial. Keep every sample and failed
qualification; do not rerun/trim to select a timing result. Allocation windows
are a separate native replacement-new/delete image, not RSS or VM heap counts.

The new `wio_n03_indexed_format` gate builds contract.cpp at O0/O2. It checks
ordinary versus deliberately poisoned numeric scratch, integer extrema,
scientific/fixed/custom/grouping/currency/percent formats, precision boundaries,
NUL, escaped braces, repeated/reordered arguments, alignment and errors. The
ordinary StackText constructor retains zero initialization. Only the audited
numeric conversion path opts into uninitialized scratch, and it must write all
bytes before committing a readable View.
