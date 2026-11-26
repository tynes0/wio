# Test registration

`app/CMakeLists.txt` builds the executable, self-hosted CLI and playground.
It includes these registration modules only when `BUILD_TESTING=ON`:

| Module | Responsibility |
| --- | --- |
| `LanguageTests.cmake` | Positive language, semantic and code-generation cases |
| `InvalidTests.cmake` | Compile-time rejection and backend failure cases |
| `RuntimeTests.cmake` | Standard library, runtime failures, async and fuzzing |
| `InteropTests.cmake` | Native libraries, host SDK and module boundaries |
| `CliTests.cmake` | Project/file commands, packaging, editor and workflow tooling |

`WioTestHelpers.cmake` contains the shared registration helpers;
`WioTestTargets.cmake` contains the CTest convenience targets. Modules are
included in the **app directory scope**, not added as subdirectories, so test
properties, output locations and generator expressions keep their old meaning.

Keep suite membership beside the registration:

```cmake
add_wio_output_test(
    wio_test_example_run
    "${CMAKE_SOURCE_DIR}/tests/example.wio"
    "example-ok"
    --run
)
wio_test_labels(wio_test_example_run language coherence traceability)
```

`wio_test_labels` records additive labels. Finalization happens once after all
modules have been included, preserving existing `LABELS` properties and removing
duplicates. Renaming a test does not implicitly add or remove release coverage.
Separately registered C++ tests use `set_property(TEST ... APPEND PROPERTY LABELS
...)` in their owning directory.

```powershell
ctest --test-dir build -C Debug -L '^traceability$' --output-on-failure
ctest --test-dir build -C Debug -L '^coherence$' --output-on-failure
ctest --test-dir build -C Debug -L '^(traceability|freeze|coherence)$' --output-on-failure
```

`wio_tests_freeze` runs that last union **once**, instead of executing overlap
cases repeatedly. `wio_tests_traceability` and `wio_tests_language_coherence`
retain their names. With `BUILD_TESTING=OFF`, registration modules and these
test-only targets are absent.
