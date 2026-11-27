"""Native dependency observations. No adapter cache reuse is enabled here."""

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


class NativeDependencies(unittest.TestCase):
    def setUp(self):
        OPTIONS.scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="dependencies-", dir=OPTIONS.scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "Project İstanbul"
        self.root.mkdir()
        self.environment = os.environ.copy()
        if OPTIONS.wio_root:
            self.environment["WIO_ROOT"] = str(OPTIONS.wio_root.resolve())
        self.write("wio.makewio", '''name = "NativeDependencies"
[wio]
entry = "main.wio"
target = "exe"
includeDirs = ["native"]
nativeSources = ["native/fixture.cpp"]
[host]
enabled = false
''')
        self.write("native/inner.h", "#pragma once\n#define ANSWER_OFFSET 1\n")
        self.write("native/fixture.h", '#pragma once\n#include "inner.h"\nint Answer(int value);\n')
        self.write("native/fixture.cpp", '#include "fixture.h"\nint Answer(int value) { return value + ANSWER_OFFSET; }\n')
        self.write("main.wio", '''using cpp::header("fixture.h");
[Native, CppName(Answer)] fn Answer(value: i32) -> i32;
fn Entry() -> i32 { return Answer(41) == 42 ? 0 : 9; }
''')

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value, encoding="utf-8")

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

    def info(self):
        return json.loads(self.execute("project", "describe", "--project", self.root))

    def build(self):
        self.execute("project", "build", "--project", self.root, "--backend", "vm", "--rebuild")
        info = self.info()
        self.report_path = Path(info["vm"]["nativeDependencies"])
        value = json.loads(self.report_path.read_text(encoding="utf-8"))
        self.assertEqual(value["kind"], "wio-native-dependencies")
        self.assertEqual(value["schemaVersion"], 1)
        self.assertFalse(value["cacheReuseEnabled"])
        self.assertFalse(value["hermeticBuild"])
        self.assertTrue(value["requiresFreshPreprocessing"])
        self.assertTrue(value["requiresFreshLinkResolution"])
        self.assertTrue(value["knownInputsStable"])
        bridge = Path(info["vm"]["bridge"])
        self.assertEqual(value["artifactSha256"], hashlib.sha256(bridge.read_bytes()).hexdigest())
        requirements = json.loads(Path(info["vm"]["nativeRequirements"]).read_text(encoding="utf-8"))
        self.assertEqual(value["requirementsSha256"], requirements["sha256"])
        return value

    def input(self, report, suffix):
        entries = [entry for entry in report["inputs"] if entry["path"].endswith(suffix)]
        self.assertTrue(entries, f"missing dependency {suffix}: {report['incompleteReasons']}")
        return entries[0]

    def test_transitive_headers_sources_and_runtime_are_recorded(self):
        report = self.build()
        for suffix in ("/native/inner.h", "/native/fixture.h", "/native/fixture.cpp"):
            entry = self.input(report, suffix)
            self.assertEqual(entry["sha256"], hashlib.sha256(Path(entry["path"]).read_bytes()).hexdigest())
        self.assertTrue(any(entry["role"] == "toolchain-tool" for entry in report["inputs"]))
        self.assertTrue(any(entry["role"] == "linker-input" for entry in report["inputs"]), report["incompleteReasons"])
        self.assertEqual(len(report["preprocessedSha256"]), 2)
        self.assertIn("-dumpmachine", report["toolchainProbeSha256"])
        self.assertTrue(any(entry["role"] == "compiler" for entry in report["inputs"]))
        self.assertIn("-std=c++20", report["compileArguments"])

    def test_header_change_is_separate_from_requirements_identity(self):
        before = self.build()
        self.write("native/inner.h", "#pragma once\n#define ANSWER_OFFSET 2\n")
        after = self.build()
        self.assertEqual(before["requirementsSha256"], after["requirementsSha256"])
        self.assertNotEqual(self.input(before, "/native/inner.h")["sha256"],
                            self.input(after, "/native/inner.h")["sha256"])
        self.assertNotEqual(before["preprocessedSha256"], after["preprocessedSha256"])

    def test_negative_include_lookup_requires_fresh_preprocessing(self):
        self.write("native/inner.h", '''#pragma once
#if __has_include("optional.h")
#include "optional.h"
#else
#define ANSWER_OFFSET 1
#endif
''')
        before = self.build()
        self.write("native/optional.h", "#define ANSWER_OFFSET 2\n")
        after = self.build()
        self.input(after, "/native/optional.h")
        self.assertNotEqual(before["preprocessedSha256"], after["preprocessedSha256"])

    def test_failed_build_preserves_previous_dependency_report(self):
        self.build()
        before = self.report_path.read_bytes()
        self.write("native/inner.h", "#error deliberate-failure\n")
        self.execute("project", "build", "--project", self.root, "--backend", "vm", "--rebuild", success=False)
        self.assertEqual(before, self.report_path.read_bytes())

    def test_environment_is_hashed_not_exposed(self):
        secret = str(self.root / "private-environment-marker")
        Path(secret).mkdir()
        self.environment["CPATH"] = secret
        self.environment["LIBRARY_PATH"] = secret
        report = self.build()
        self.assertEqual(report["environmentSha256"]["CPATH"], hashlib.sha256(("set:" + secret).encode()).hexdigest())
        self.assertEqual(report["environmentSha256"]["LIBRARY_PATH"], hashlib.sha256(("set:" + secret).encode()).hexdigest())
        self.assertNotIn("private-environment-marker", json.dumps(report))

    def test_missing_report_is_regenerated(self):
        self.build()
        self.report_path.unlink()
        output = self.execute("project", "build", "--project", self.root, "--backend", "vm")
        self.assertIn("Generated native dependencies:", output)
        self.assertTrue(self.report_path.is_file())

    def test_native_source_change_is_recorded(self):
        before = self.build()
        self.write("native/fixture.cpp", '#include "fixture.h"\nint Answer(int value) { return value + 2; }\n')
        after = self.build()
        self.assertEqual(before["requirementsSha256"], after["requirementsSha256"])
        self.assertNotEqual(self.input(before, "/native/fixture.cpp")["sha256"],
                            self.input(after, "/native/fixture.cpp")["sha256"])

    def test_unclassified_options_keep_build_but_mark_record_incomplete(self):
        manifest = self.root / "wio.makewio"
        self.write("wio.makewio", manifest.read_text(encoding="utf-8").replace(
            '[host]', 'backendArgs = ["-fno-omit-frame-pointer"]\n[host]'))
        report = self.build()
        self.assertFalse(report["observationComplete"])
        self.assertIn("unclassified-compile-option: -fno-omit-frame-pointer", report["incompleteReasons"])
        self.assertEqual(report["preprocessedSha256"], [])

    def test_bytecode_only_rejects_native_dependency_capture(self):
        output = self.execute("main.wio", "--include-dir", "native", "--emit-bytecode",
                              "--native-dependencies-output", "bad.json", success=False)
        self.assertIn("requires a canonical native bridge compilation", output)
        self.assertFalse((self.root / "bad.json").exists())

    def test_dependency_report_cannot_overwrite_native_header(self):
        path = self.root / "native/inner.h"
        before = path.read_bytes()
        output = self.execute("main.wio", "--include-dir", "native", "--backend-arg", "native/fixture.cpp",
                              "--target", "shared", "--output", "bridge.dll", "--native-bridge-only",
                              "--bytecode-sidecar", "program.wiob", "--native-dependencies-output", path, success=False)
        self.assertIn("must not overwrite", output)
        self.assertEqual(before, path.read_bytes())


if __name__ == "__main__":
    unittest.main(argv=[__file__, *TEST_ARGS])
