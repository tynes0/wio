# Fuzzing Wio

Wio has two complementary fuzz layers.

## Deterministic pipeline fuzzing

`tools/fuzz/wio_fuzz.py` runs a checked-in corpus plus reproducible mutations through:

1. lexer/token dumping;
2. parser and semantic dry-run;
3. dry-run versus C++ emission differential checking;
4. native compiler `-fsyntax-only` validation for every semantically valid input.

Each subprocess has a timeout. Abnormal exits, mode-dependent outcomes, more than the configured diagnostic budget, excessive output, missing generated files, and invalid generated C++ fail the run. The corpus includes nested interpolation with nested calls and strings, malformed generics, deep types, import cycles, semantic poison, arbitrary bytes, NULs, and invalid UTF-8. Mutations insert/delete/duplicate token fragments, flip arbitrary bytes, and grow nested expressions. A failure is copied to `failure-repro.wio`.

Build-tree smoke runs are available through CTest as `wio_fuzz_smoke` when Python 3 is installed. For a longer local run:

```text
python tools/fuzz/wio_fuzz.py --wio build/app/wio --backend g++ --root . --corpus tests/fuzz/corpus --work build/fuzz-work --iterations 1000 --seed 5712207
```

## Sanitizer-guided frontend fuzzing

Configure with Clang and `-DWIO_BUILD_FUZZERS=ON` to build `wio_frontend_fuzzer`. It exposes `LLVMFuzzerTestOneInput`, suppresses expected diagnostics, and drives lexer, parser, semantic analysis, and C++ generation in-process under libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer.

```text
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DWIO_BUILD_FUZZERS=ON
cmake --build build-fuzz --target wio_frontend_fuzzer
build-fuzz/compiler/wio_frontend_fuzzer tests/fuzz/corpus -max_len=262144 -timeout=5
```

Crashes, hangs, sanitizer findings, diagnostic explosions, dry-run/emission disagreement, and backend-invalid C++ are release blockers. New minimized reproducers belong in `tests/fuzz/corpus` and should also receive a focused regression test when the expected diagnostic or behavior is stable.
