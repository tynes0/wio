"""Native requirements identity and publication; adapter reuse stays disabled."""

import argparse
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


class NativeRequirements(unittest.TestCase):
    def setUp(self):
        OPTIONS.scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="requirements-", dir=OPTIONS.scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "Project İstanbul"
        self.root.mkdir()
        self.environment = os.environ.copy()
        if OPTIONS.wio_root:
            self.environment["WIO_ROOT"] = str(OPTIONS.wio_root.resolve())
        self.write("wio.makewio", '''name = "NativeRequirements"
[wio]
entry = "main.wio"
target = "exe"
includeDirs = ["native"]
[host]
enabled = false
''')
        self.write("native/fixture.h", "#pragma once\ninline int Answer(int value) { return value + 1; }\n")
        self.write("main.wio", '''using cpp::header("fixture.h");
[Native, CppName(Answer)] fn Answer(value: i32) -> i32;
fn Entry() -> i32 { return Answer(41) == 42 ? 0 : 9; }
''')

    def write(self, path, value):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(value, encoding="utf-8")

    def execute(self, *arguments, success=True):
        result = subprocess.run([str(WIO), *map(str, arguments)], cwd=self.root,
                                env=self.environment, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=180)
        output = result.stdout + result.stderr
        if success:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertNotEqual(result.returncode, 0, output)
        return output

    def project(self, command, *arguments, **options):
        return self.execute("project", command, "--project", self.root,
                            "--backend", "vm", *arguments, **options)

    def report(self):
        output = self.execute("main.wio", "--include-dir", "native", "--emit-bytecode",
                              "--bytecode-output", "module.wiob",
                              "--native-requirements-output", "requirements.json")
        self.assertNotIn("Generated C++ output:", output)
        value = json.loads((self.root / "requirements.json").read_text(encoding="utf-8"))
        self.assertEqual(value["schemaVersion"], 1)
        self.assertEqual(value["algorithm"], "sha256")
        self.assertRegex(value["sha256"], "^[0-9a-f]{64}$")
        self.assertFalse(value["cacheReuseEnabled"])
        self.assertFalse(value["dependencyClosureIncluded"])
        return value

    def test_vm_body_change_does_not_change_identity(self):
        first = self.report()
        self.write("main.wio", '''using cpp::header("fixture.h");
[Native, CppName(Answer)] fn Answer(value: i32) -> i32;
fn Entry() -> i32 { let message = "VM-only text 🌍"; return Answer(9) + 2; }
''')
        self.assertEqual(first["sha256"], self.report()["sha256"])

    def test_source_line_and_comment_changes_are_ignored(self):
        before = self.report()["sha256"]
        self.write("main.wio", "\n\n// source-only\n" + (self.root / "main.wio").read_text(encoding="utf-8"))
        self.assertEqual(before, self.report()["sha256"])

    def test_native_signature_change_invalidates_identity(self):
        before = self.report()["sha256"]
        self.write("main.wio", '''using cpp::header("fixture.h");
[Native, CppName(Answer)] fn Answer(value: i64) -> i64;
fn Entry() -> i32 { return 0; }
''')
        self.assertNotEqual(before, self.report()["sha256"])

    def test_native_header_contents_are_explicitly_outside_identity(self):
        before = self.report()["sha256"]
        self.write("native/fixture.h", "#pragma once\ninline int Answer(int value) { return value + 2; }\n")
        self.assertEqual(before, self.report()["sha256"])

    def test_header_only_import_changes_requirements(self):
        before = self.report()["sha256"]
        self.write("native/other.h", "#pragma once\n")
        self.write("main.wio", 'using cpp::header("other.h");\n' +
                   (self.root / "main.wio").read_text(encoding="utf-8"))
        value = self.report()
        self.assertNotEqual(before, value["sha256"])
        self.assertTrue(any("other.h" in entry for entry in value["nativeImports"]))

    def test_callback_signature_changes_identity(self):
        def source(type_name):
            return f'''using cpp::header("fixture.h");
[Native, CppName(Call)] fn Call(callback: fn({type_name}) -> {type_name}) -> i32;
fn Entry() -> i32 {{ return 0; }}
'''
        self.write("main.wio", source("i32"))
        before = self.report()["sha256"]
        self.write("main.wio", source("i64"))
        self.assertNotEqual(before, self.report()["sha256"])

    def test_project_bridge_publishes_report_without_reuse(self):
        self.project("run")
        info = json.loads(self.execute("project", "describe", "--project", self.root))
        report = Path(info["vm"]["nativeRequirements"])
        first = json.loads(report.read_text(encoding="utf-8"))
        self.write("main.wio", (self.root / "main.wio").read_text(encoding="utf-8").replace("Answer(41)", "Answer(40) + 1"))
        output = self.project("run", "--rebuild")
        self.assertIn("Generated C++ output:", output, "0.20.3 must not silently enable adapter reuse")
        self.assertEqual(first["sha256"], json.loads(report.read_text(encoding="utf-8"))["sha256"])
        report.unlink()
        self.project("run", "--no-build")
        self.assertIn("Generated native requirements:", self.project("build"))

    def test_failed_cpp_build_preserves_previous_report(self):
        self.project("build")
        info = json.loads(self.execute("project", "describe", "--project", self.root))
        report = Path(info["vm"]["nativeRequirements"])
        before = report.read_bytes()
        self.write("native/fixture.h", "#error deliberate native build failure\n")
        self.project("build", "--rebuild", success=False)
        self.assertEqual(report.read_bytes(), before)

    def test_analysis_failure_preserves_previous_report(self):
        self.report()
        before = (self.root / "requirements.json").read_bytes()
        self.write("main.wio", "fn Entry() -> i32 { return missing; }\n")
        self.execute("main.wio", "--emit-bytecode", "--native-requirements-output", "requirements.json", success=False)
        self.assertEqual(before, (self.root / "requirements.json").read_bytes())

    def test_source_and_bytecode_collisions_are_rejected(self):
        before = (self.root / "main.wio").read_bytes()
        for path in ("main.wio", "module.wiob"):
            with self.subTest(path=path):
                output = self.execute("main.wio", "--include-dir", "native", "--emit-bytecode",
                                      "--bytecode-output", "module.wiob",
                                      "--native-requirements-output", path, success=False)
                self.assertIn("must be distinct", output)
        self.assertEqual(before, (self.root / "main.wio").read_bytes())

    def test_unrelated_modes_reject_report_option(self):
        for mode in ("--dry-run", "--emit-typed-wir", "--emit-cpp"):
            with self.subTest(mode=mode):
                output = self.execute("main.wio", mode, "--native-requirements-output", "requirements.json", success=False)
                self.assertIn("requires --emit-bytecode", output)
        self.assertFalse((self.root / "requirements.json").exists())

    def test_std_covered_project_keeps_bytecode_first_route(self):
        self.write("main.wio", 'use std::console as console;\nfn Entry() -> i32 { console::PrintLine!("ok"); return 0; }\n')
        output = self.project("run")
        self.assertIn("no project C++ build", output)
        info = json.loads(self.execute("project", "describe", "--project", self.root))
        self.assertFalse(Path(info["vm"]["nativeRequirements"]).exists())


if __name__ == "__main__":
    unittest.main(argv=[__file__, *TEST_ARGS])
