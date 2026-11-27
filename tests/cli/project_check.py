"""Focused, artifact-free project-check qualification using a built Wio CLI."""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


PARSER = argparse.ArgumentParser()
PARSER.add_argument("--wio", required=True, type=Path)
PARSER.add_argument("--scratch", required=True, type=Path)
PARSER.add_argument("--wio-root", type=Path)
OPTIONS, TEST_ARGS = PARSER.parse_known_args()
WIO = OPTIONS.wio.resolve()


class ProjectCheck(unittest.TestCase):
    def setUp(self):
        OPTIONS.scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="check-", dir=OPTIONS.scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "Project space İstanbul"
        self.root.mkdir()
        (self.root / "modules").mkdir()
        (self.root / "native" / "include").mkdir(parents=True)
        self.put("wio/main.wio", "fn Entry() -> i32 { return 99; }\n")
        self.manifest()

    def put(self, relative, source):
        destination = self.root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(source, encoding="utf-8")
        return destination

    def manifest(self, target="exe", host=False):
        self.put("wio.makewio", f'''schemaVersion = 1
name = "CheckOnly"
[wio]
entry = "wio/main.wio"
target = "{target}"
sourceRoots = ["modules"]
includeDirs = ["native/include"]
nativeSources = ["native/must_not_build.cpp"]
linkDirs = ["not-yet-built-library-directory"]
linkLibraries = ["deliberately_missing_library"]
backendArgs = ["--invalid-backend-flag-must-not-execute"]
[host]
enabled = {str(host).lower()}
compiler = "missing-host-compiler"
sources = ["missing-host.cpp"]
[outputs]
directory = ".wio-build/interop"
baseName = "check_only"
''')
        self.put("native/must_not_build.cpp", "#error project check must not compile C++\n")

    def snapshot(self):
        return {
            file.relative_to(self.root).as_posix():
                (hashlib.sha256(file.read_bytes()).hexdigest(), file.stat().st_mtime_ns)
            for file in self.root.rglob("*") if file.is_file()
        }, sorted(path.relative_to(self.root).as_posix()
                  for path in self.root.rglob("*") if path.is_dir())

    def check(self, *arguments, cwd=None, explicit=True, success=True):
        before = self.snapshot()
        command = [str(WIO), "project", "check"]
        if explicit:
            command += ["--project", str(self.root)]
        command += list(arguments)
        environment = os.environ.copy()
        if OPTIONS.wio_root:
            environment["WIO_ROOT"] = str(OPTIONS.wio_root.resolve())
        result = subprocess.run(command, cwd=cwd or self.root, env=environment,
                                capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=45)
        output = result.stdout + result.stderr
        self.assertEqual(self.snapshot(), before, f"Check modified the project: {output}")
        if success:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertNotEqual(result.returncode, 0, output)
        self.assertNotIn("Generated C++ output:", output)
        self.assertNotIn("Generated backend output:", output)
        return output

    def test_executable_is_analyzed_not_run_or_built(self):
        self.assertIn("Dry run completed successfully", self.check())
        self.assertFalse((self.root / ".wio-build").exists())

    def test_existing_artifacts_and_signatures_are_untouched(self):
        for name in ("check_only.exe", "check_only.wio.cpp", "check_only.wiob",
                     "check_only.vm-native.dll", "check_only.exe.build-args"):
            self.put(f".wio-build/interop/{name}", "sentinel\n")
        self.check()

    def test_nested_directory_discovers_manifest(self):
        nested = self.root / "wio" / "nested"
        nested.mkdir()
        self.check(cwd=nested, explicit=False)

    def test_explicit_manifest_path(self):
        self.check(str(self.root / "wio.makewio"), explicit=False)

    def test_native_headers_and_module_roots_without_cpp_build(self):
        self.put("modules/helper.wio", "realm helper { fn Value() -> i32 { return 21; } }\n")
        self.put("native/include/check_api.h", "#error this header must not be compiled\n")
        self.put("wio/main.wio", '''use helper;
using cpp::header("check_api.h");
[Native, CppName(check_api::Multiply)]
fn Multiply(left: i32, right: i32) -> i32;
fn Entry() -> i32 { return Multiply(helper::Value(), 2); }
''')
        self.check()

    def test_missing_declared_native_header_is_reported(self):
        self.put("wio/main.wio", '''using cpp::header("missing_check_api.h");
[Native, CppName(check_api::Read)] fn Read() -> i32;
fn Entry() -> i32 { return Read(); }
''')
        self.assertIn("missing_check_api.h", self.check(success=False))

    def test_imported_semantic_error_is_not_hidden(self):
        self.put("modules/helper.wio", 'realm helper { fn Value() -> i32 { return "wrong"; } }\n')
        self.put("wio/main.wio", "use helper; fn Entry() -> i32 { return helper::Value(); }\n")
        self.assertIn("helper.wio", self.check(success=False))

    def test_rechecks_changed_sources_without_build_cache(self):
        self.check()
        self.put("wio/main.wio", "fn Entry() -> i32 { return MissingCheckSymbol; }\n")
        self.assertIn("MissingCheckSymbol", self.check(success=False))

    def test_library_targets_do_not_require_entry(self):
        self.put("wio/main.wio", "fn Add(a: i32, b: i32) -> i32 { return a + b; }\n")
        for target in ("static", "shared"):
            with self.subTest(target=target):
                self.manifest(target=target)
                self.check()

    def test_executable_still_requires_entry(self):
        self.put("wio/main.wio", "fn Helper() -> i32 { return 0; }\n")
        self.assertIn("Entry", self.check(success=False))

    def test_hybrid_check_does_not_build_host(self):
        self.manifest(target="shared", host=True)
        self.check()

    def test_generated_native_source_and_library_need_not_exist_yet(self):
        manifest = self.root / "wio.makewio"
        content = manifest.read_text(encoding="utf-8")
        manifest.write_text(content.replace("native/must_not_build.cpp", "generated/absent.cpp")
                            .replace("deliberately_missing_library", "generated/absent.a"), encoding="utf-8")
        self.check()

    def test_explicit_cpp_emission_remains_a_build_operation(self):
        (self.root / "not-yet-built-library-directory").mkdir()
        environment = os.environ.copy()
        if OPTIONS.wio_root:
            environment["WIO_ROOT"] = str(OPTIONS.wio_root.resolve())
        result = subprocess.run([str(WIO), "project", "build", "--project", str(self.root), "--emit-cpp"],
                                cwd=self.root, env=environment, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=45)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.root / ".wio-build/interop/check_only.wio.cpp").is_file())
        self.assertFalse((self.root / ".wio-build/interop/check_only.exe").exists())

    def test_vm_selection_does_not_require_vm_artifacts(self):
        self.check("--backend", "vm", "--vm-runner", "missing-vm-runner")

    def test_config_and_build_directory_override_do_not_create_outputs(self):
        self.check("--config", "Release", "--build-dir", "uncreated build dir")
        self.assertFalse((self.root / "uncreated build dir").exists())

    def test_invalid_backend_and_build_flags_are_rejected(self):
        for arguments in (("--backend", "unknown"), ("--emit-cpp",), ("--rebuild",),
                          ("--no-build",), ("--run",)):
            with self.subTest(arguments=arguments):
                self.check(*arguments, success=False)

    def test_no_builtin_analysis(self):
        self.check("--no-builtin")

    def test_help_does_not_resolve_missing_project(self):
        self.assertIn("check", self.check("--help", "--project", "missing-project", explicit=False))

    def test_missing_entry_and_malformed_manifest_fail_without_outputs(self):
        self.put("wio.makewio", 'name = "Missing"\n[wio]\nentry = "absent.wio"\n')
        self.assertIn("absent.wio", self.check(success=False))
        self.put("wio.makewio", "invalid manifest line\n")
        self.assertIn("Invalid makewio", self.check(success=False))


if __name__ == "__main__":
    unittest.main(argv=[__file__, *TEST_ARGS], verbosity=2)
