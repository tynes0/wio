"""Bytecode-first VM project builds, standard adapter reuse and native fallback."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


PARSER = argparse.ArgumentParser()
PARSER.add_argument("--wio", type=Path, required=True)
PARSER.add_argument("--scratch", type=Path, required=True)
PARSER.add_argument("--wio-root", type=Path)
OPTIONS, TEST_ARGS = PARSER.parse_known_args()
WIO = OPTIONS.wio.resolve()
SUFFIX = ".exe" if os.name == "nt" else ""


class BytecodeFirst(unittest.TestCase):
    def setUp(self):
        OPTIONS.scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="bytecode-", dir=OPTIONS.scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "Project İstanbul"
        self.root.mkdir()
        self.environment = os.environ.copy()
        if OPTIONS.wio_root:
            self.environment["WIO_ROOT"] = str(OPTIONS.wio_root.resolve())
        self.write("main.wio", "fn Entry() -> i32 { return 0; }\n")
        self.write("wio.makewio", '''name = "BytecodeFirst"
[wio]
entry = "main.wio"
target = "exe"
includeDirs = ["native"]
[host]
enabled = false
[outputs]
baseName = "bytecode_first"
''')
        (self.root / "native").mkdir()

    def write(self, path, text):
        destination = self.root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(text, encoding="utf-8")
        return destination

    def execute(self, arguments, success=True, environment=None):
        result = subprocess.run([str(value) for value in arguments], cwd=self.root,
                                env=environment or self.environment, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=180)
        output = result.stdout + result.stderr
        if success:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertNotEqual(result.returncode, 0, output)
        return output

    def project(self, command, *arguments, **options):
        return self.execute([WIO, "project", command, "--project", self.root,
                             "--backend", "vm", *arguments], **options)

    def describe(self):
        return json.loads(self.execute([WIO, "project", "describe", "--project", self.root]))

    def assert_no_project_cpp(self, output):
        self.assertNotIn("Generated C++ output:", output)
        self.assertNotIn("Generated backend output:", output)
        self.assertIn("no project C++ build", output)
        self.assertEqual(list(self.root.rglob("*.wio.cpp")), [])
        self.assertEqual([path for path in self.root.rglob("*.vm-native.*")
                          if path.suffix in (".dll", ".so", ".dylib")], [])
        for staging in self.root.rglob(".wbc"):
            self.assertEqual(list(staging.iterdir()), [], "Private bytecode working files were retained")

    def test_plain_root_entry_and_incremental_reuse(self):
        self.assert_no_project_cpp(self.project("run"))
        description = self.describe()
        bytecode = Path(description["vm"]["bytecode"])
        before = bytecode.stat().st_mtime_ns
        self.assert_no_project_cpp(self.project("run"))
        self.assertEqual(bytecode.stat().st_mtime_ns, before,
                         "Root module directory included its own generated outputs")
        self.project("run", "--no-build")

    def test_console_convert_filesystem_and_unicode(self):
        self.write("main.wio", '''use std::console as console;
use std::convert as convert;
use std::fs as fs;
fn Entry() -> i32 {
    let number = convert::ToI32("42");
    fs::WriteText!("result.txt", "standard-adapter");
    console::PrintLine!($"bytecode-first:${number}");
    console::PrintLine!(u"Unicode 🌍");
    console::PrintLine!(42usize);
    console::Print!(-42isize);
    console::PrintLine!(18446744073709551615usize);
    return number == 42 ? 0 : 9;
}
''')
        output = self.project("run")
        self.assert_no_project_cpp(output)
        self.assertIn("bytecode-first:42", output)
        self.assertIn("Unicode 🌍", output)
        self.assertIn("-4218446744073709551615", output)
        self.assertEqual((self.root / "result.txt").read_text(encoding="utf-8"), "standard-adapter")

    def test_async_wio_body_uses_bytecode(self):
        self.write("main.wio", '''async fn Value() -> i32 { return 42; }
async fn Entry() -> i32 {
    let value = await Value();
    return value == 42 ? 0 : 7;
}
''')
        self.assert_no_project_cpp(self.project("run"))

    def test_standard_hash_math_and_regex_array_results(self):
        self.write("main.wio", '''use std::hash as hash;
use std::math as math;
use std::regex as regex;
fn Entry() -> i32 {
    let digest = hash::Sha256("abc");
    if (digest != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") return 5;
    if (hash::Sha256Digest("abc").Count() != 32usize) return 6;
    if (math::Sqrt(9f64) != 3f64) return 7;
    let matches = regex::FindAll!("abc12def34", "[0-9]+");
    return matches.Count() == 2usize and matches[0usize] == "12" and matches[1usize] == "34" ? 0 : 8;
}
''')
        self.assert_no_project_cpp(self.project("run"))

    def test_standard_random_object_and_mutable_array_state(self):
        self.write("main.wio", '''use std::random as random;
fn Entry() -> i32 {
    let first = random::Mt19937(42u64);
    let second = random::Mt19937(42u64);
    let value = first.NextU32();
    if (value != 1608637542u32 or value != second.NextU32()) return 5;
    return first.NextU32() == second.NextU32() ? 0 : 6;
}
''')
        self.assert_no_project_cpp(self.project("run"))

    def test_imported_native_generic_templates_are_not_missing_specializations(self):
        self.write("main.wio", "use std::console;\nfn Entry() -> i32 { return 0; }\n")
        self.assert_no_project_cpp(self.project("build"))

    def test_explicit_canonical_backend_and_no_builtin_do_not_force_cpp(self):
        self.assert_no_project_cpp(self.project("build", "--cpp-backend", "wir", "--no-builtin"))
        self.project("run", "--no-build")

    def test_cpp_compiler_poison_proves_both_routes(self):
        marker = self.root / "compiler-was-invoked"
        harness = self.root.parent / "harness"
        harness.mkdir()
        shim_source = harness / "fail_compiler.wio"
        shim_source.write_text(f'''use std::fs as fs;
fn Entry() -> i32 {{
    fs::WriteText!({json.dumps(str(marker), ensure_ascii=False)}, "invoked");
    return 91;
}}
''', encoding="utf-8")
        compiler_directory = harness / "compiler"
        compiler_directory.mkdir()
        shim = compiler_directory / ("g++" + SUFFIX)
        self.execute([WIO, shim_source, "--output", shim])
        poisoned = self.environment.copy()
        poisoned["PATH"] = str(compiler_directory) + os.pathsep + poisoned.get("PATH", "")
        output = self.project("run", environment=poisoned)
        self.assert_no_project_cpp(output)
        self.assertFalse(marker.exists(), "Pure VM project invoked a C++ compiler")
        code = Path(self.describe()["vm"]["bytecode"])
        old_code = hashlib.sha256(code.read_bytes()).hexdigest()
        self.native_source()
        self.project("build", environment=poisoned, success=False)
        self.assertTrue(marker.exists(), "Poison compiler fixture did not intercept native fallback")
        self.assertEqual(hashlib.sha256(code.read_bytes()).hexdigest(), old_code,
                         "Failed native fallback published bytecode for an unbuilt adapter")
        self.project("run", "--no-build")

    def native_source(self):
        self.write("native/fixture.h", "#pragma once\ninline int FixtureAnswer() { return 42; }\n")
        self.write("main.wio", '''using cpp::header("fixture.h");
[Native, CppName(FixtureAnswer)]
fn NativeAnswer() -> i32;
fn Entry() -> i32 { return NativeAnswer() == 42 ? 0 : 9; }
''')

    def test_user_native_binding_falls_back_then_returns_to_bytecode_only(self):
        self.native_source()
        output = self.project("run")
        self.assertIn("project-specific adapter", output)
        self.assertIn("Generated C++ output:", output)
        description = self.describe()
        bridge = Path(description["vm"]["bridge"])
        before = bridge.stat().st_mtime_ns
        self.write("main.wio", "fn Entry() -> i32 { return 0; }\n")
        pure_output = self.project("run")
        self.assertNotIn("Generated C++ output:", pure_output)
        self.assertIn("no project C++ build", pure_output)
        self.assertEqual(bridge.stat().st_mtime_ns, before)
        for old in Path(description["vm"]["executable"]).parent.glob("*.vm-native.wio-*"):
            old.rename(old.with_suffix(old.suffix + ".offline"))
        self.project("run", "--no-build")

    def test_concrete_user_native_generic_requires_project_adapter(self):
        self.write("native/generic.h", "#pragma once\ntemplate<class T> T FixtureIdentity(T value) { return value; }\n")
        self.write("main.wio", '''using cpp::header("generic.h");
[Native, CppName(FixtureIdentity), Instantiate(i32)]
fn NativeIdentity<T>(value: T) -> T;
fn Entry() -> i32 { return NativeIdentity<i32>(42) == 42 ? 0 : 9; }
''')
        output = self.project("run")
        self.assertIn("project-specific adapter", output)
        self.assertIn("Generated C++ output:", output)

    def test_native_pod_defaults_without_native_functions_require_adapter(self):
        self.write("native/settings.h", "#pragma once\nstruct FixtureSettings { int count = 17; };\n")
        self.write("main.wio", '''using cpp::header("settings.h");
[Native, CppName(FixtureSettings)]
component Settings { public count: i32; }
fn Entry() -> i32 {
    let value = Settings();
    return value.count == 17 ? 0 : 9;
}
''')
        output = self.project("run")
        self.assertIn("project-specific adapter", output)
        self.assertIn("Generated C++ output:", output)

    def test_module_source_edit_invalidates_bytecode(self):
        self.write("helper.wio", "fn Answer() -> i32 { return 42; }\n")
        self.write("main.wio", "use helper;\nfn Entry() -> i32 { return Answer() == 42 ? 0 : 8; }\n")
        self.assert_no_project_cpp(self.project("run"))
        original = (self.root / "helper.wio").stat()
        self.write("helper.wio", "fn Answer() -> i32 { return 43; }\n")
        os.utime(self.root / "helper.wio", ns=(original.st_atime_ns, original.st_mtime_ns))
        output = self.project("run", success=False)
        self.assertIn("Generated bytecode output:", output)
        self.assertNotIn("Generated C++ output:", output)

    def test_modified_cached_candidate_is_rebuilt(self):
        self.assert_no_project_cpp(self.project("build"))
        code = Path(self.describe()["vm"]["bytecode"])
        candidate = code.parent / ".vm-bytecode-stage" / code.name
        candidate.write_bytes(b"corrupted cache")
        output = self.project("run")
        self.assert_no_project_cpp(output)
        self.assertIn("Generated bytecode output:", output)

    def test_user_cpp_header_startup_is_preserved_but_not_run_during_build(self):
        self.write("native/startup.h", '''#pragma once
#include <cstdio>
inline const int fixtureStartup = [] {
    if (auto* file = std::fopen("header-started", "wb")) {
        std::fputs("started", file);
        std::fclose(file);
    }
    return 0;
}();
''')
        self.write("main.wio", '''using cpp::header("startup.h");
fn Entry() -> i32 { return 0; }
''')
        output = self.project("build")
        self.assertIn("project-specific adapter", output)
        self.assertFalse((self.root / "header-started").exists(), "Build executed native header startup")
        self.project("run", "--no-build")
        self.assertEqual((self.root / "header-started").read_text(), "started")

    def test_packaged_project_has_no_dependency_on_build_intermediates(self):
        self.assert_no_project_cpp(self.project("build"))
        description = self.describe()
        code = Path(description["vm"]["bytecode"])
        code.rename(code.with_suffix(".offline"))
        self.project("package", "--no-build", "--output-dir", self.root / "deliverables")
        packaged = list((self.root / "deliverables").glob("*/bin/*" + SUFFIX))
        application = next(path for path in packaged if path.is_file() and path.name == "bytecode_first" + SUFFIX)
        clean_environment = self.environment.copy()
        clean_environment.pop("WIO_ROOT", None)
        self.execute([application], environment=clean_environment)

    def test_raw_preflight_does_not_execute_entry(self):
        self.write("main.wio", "fn Entry() -> i32 { while (true) {} return 0; }\n")
        self.assert_no_project_cpp(self.project("build"))
        info = self.describe()["vm"]
        self.execute([info["runner"], "--check-native", info["bytecode"],
                      "--standard-library", info["standardNativeLibrary"]])

    def test_same_cpp_symbol_from_another_header_is_not_standard_support(self):
        self.write("native/custom_math.h", "#pragma once\n")
        source = self.write("main.wio", '''using cpp::header("custom_math.h");
[Native, CppName(wio::runtime::std_math::SqrtF64)]
fn CustomSqrt(value: f64) -> f64;
fn Entry() -> i32 { return CustomSqrt(4f64) == 2f64 ? 0 : 9; }
''')
        module = self.root / "header-probe.wiob"
        self.execute([WIO, source, "--include-dir", self.root / "native",
                      "--emit-bytecode", "--bytecode-output", module])
        info = self.describe()["vm"]
        output = self.execute([info["runner"], "--check-native", module,
                               "--standard-library", info["standardNativeLibrary"]], success=False)
        self.assertIn("requires a project adapter", output)

    def test_failed_build_preserves_deployed_image(self):
        self.assert_no_project_cpp(self.project("build"))
        image = Path(self.describe()["vm"]["executable"])
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        self.write("main.wio", "fn Entry() -> i32 { return missing_name; }\n")
        self.project("build", success=False)
        self.assertEqual(hashlib.sha256(image.read_bytes()).hexdigest(), digest)
        self.project("run", "--no-build")


if __name__ == "__main__":
    unittest.main(argv=[__file__, *TEST_ARGS])
