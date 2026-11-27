"""VM application policy qualification; run against a built or extracted toolchain."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


PARSER = argparse.ArgumentParser()
PARSER.add_argument("--wio", type=Path, required=True)
PARSER.add_argument("--runner", type=Path, required=True)
PARSER.add_argument("--scratch", type=Path, required=True)
PARSER.add_argument("--wio-root", type=Path)
OPTIONS, TEST_ARGS = PARSER.parse_known_args()
WIO = OPTIONS.wio.resolve()
RUNNER = OPTIONS.runner.resolve()
SUFFIX = ".exe" if os.name == "nt" else ""


class VmExecutionBudget(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        OPTIONS.scratch.mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(prefix="budget-", dir=OPTIONS.scratch)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.root = Path(cls.temporary.name).resolve()
        cls.environment = os.environ.copy()
        if OPTIONS.wio_root:
            cls.environment["WIO_ROOT"] = str(OPTIONS.wio_root.resolve())
        cls.module = cls.root / "loop.wiob"
        source = cls.root / "loop.wio"
        source.write_text('''fn Entry(args: string[]) -> i32 {
    if (args.Count() > 1usize) {
        return args[1usize] == "--instruction-limit" ? 0 : 7;
    }
    mut count = 0u64;
    while (count < 2000000u64) {
        count += 1u64;
    }
    return count == 2000000u64 ? 0 : 8;
}
''', encoding="utf-8")
        cls.execute([WIO, source, "--emit-bytecode", "--bytecode-output", cls.module])

    @classmethod
    def execute(cls, arguments, success=True, cwd=None):
        result = subprocess.run([str(value) for value in arguments], cwd=cwd or cls.root,
                                env=cls.environment, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=180)
        output = result.stdout + result.stderr
        if (result.returncode == 0) != success:
            raise AssertionError(f"Unexpected exit {result.returncode}: {arguments}\n{output}")
        return output

    def image(self, name, *options):
        path = self.root / name / ("app" + SUFFIX)
        self.execute([RUNNER, "--package", self.module, "--output", path, *options])
        return path

    def test_raw_default_exceeds_old_ten_million_budget(self):
        self.execute([RUNNER, self.module])
        self.assertIn("WVM1008", self.execute(
            [RUNNER, self.module, "--instruction-limit", "10000000"], success=False))

    def test_explicit_zero_and_full_uint64_range(self):
        self.execute([RUNNER, self.module, "--instruction-limit", "0", "--", "--instruction-limit"])
        self.execute([RUNNER, self.module, "--instruction-limit", str(2**64 - 1),
                      "--", "--instruction-limit"])

    def test_finite_raw_budget_has_actionable_diagnostic(self):
        output = self.execute([RUNNER, self.module, "--instruction-limit", "10"], success=False)
        self.assertIn("WVM1008", output)
        self.assertIn("limit 10", output)

    def test_raw_budget_values_are_strict(self):
        for value in ("", "-1", "+1", " 1", "1 ", "1.5", "1e3", "0x10", str(2**64)):
            with self.subTest(value=value):
                self.assertIn("--instruction-limit", self.execute(
                    [RUNNER, self.module, "--instruction-limit", value], success=False))
        self.execute([RUNNER, self.module, "--instruction-limit"], success=False)
        self.execute([RUNNER, self.module, "--instruction-limit", "0",
                      "--instruction-limit", "1"], success=False)

    def test_packaged_default_and_application_argument_boundary(self):
        image = self.image("default")
        self.execute([image])
        self.execute([image, "--instruction-limit", "not-a-runtime-option"])
        self.execute([RUNNER, "--verify", image, "--expect-instruction-limit", "0"])

    def test_finite_policy_survives_packaging_and_deployment(self):
        image = self.image("bounded", "--instruction-limit", "10")
        self.assertIn("WVM1008", self.execute([image], success=False))
        self.execute([RUNNER, "--verify", image, "--expect-instruction-limit", "10"])
        self.assertIn("differs", self.execute(
            [RUNNER, "--verify", image, "--expect-instruction-limit", "0"], success=False))
        deployed = self.root / "deployed" / ("app" + SUFFIX)
        self.execute([RUNNER, "--deploy", image, "--output", deployed])
        self.execute([RUNNER, "--verify", deployed, "--expect-instruction-limit", "10"])
        self.assertIn("WVM1008", self.execute([deployed], success=False))

    def test_packager_and_verifier_reject_invalid_policy(self):
        path = self.root / "invalid" / ("app" + SUFFIX)
        for value in ("-1", "+1", str(2**64)):
            self.execute([RUNNER, "--package", self.module, "--output", path,
                          "--instruction-limit", value], success=False)
        self.execute([RUNNER, "--package", self.module, "--output", path,
                      "--instruction-limit", "0", "--instruction-limit", "1"], success=False)
        self.assertFalse(path.exists())

    def project(self, name, budget="0", target="exe"):
        project = self.root / name
        project.mkdir(exist_ok=True)
        (project / "wio").mkdir(exist_ok=True)
        (project / "wio" / "main.wio").write_text("fn Entry() -> i32 { return 0; }\n", encoding="utf-8")
        manifest = project / "wio.makewio"
        manifest.write_text(f'''name = "Budget"
[wio]
entry = "wio/main.wio"
target = "{target}"
[host]
enabled = false
[vm]
instructionLimit = "{budget}"
''', encoding="utf-8")
        return project

    def test_project_manifest_override_incremental_and_no_build(self):
        project = self.project("Project İstanbul", "1")
        command = [WIO, "project"]
        selection = ["--project", project, "--backend", "vm"]
        self.execute([*command, "build", *selection])
        self.assertIn("WVM1008", self.execute(
            [*command, "run", *selection, "--no-build"], success=False))
        self.assertIn("differs", self.execute(
            [*command, "run", *selection, "--no-build", "--vm-instruction-limit", "0"], success=False))
        build_outputs = project / ".wio-build" / "interop"
        compiler_outputs = {path: path.stat().st_mtime_ns for path in build_outputs.iterdir()
                            if path.suffix in (".wiob", ".dll", ".so", ".dylib")}
        self.execute([*command, "run", *selection, "--vm-instruction-limit", "0"])
        self.assertEqual({path: path.stat().st_mtime_ns for path in compiler_outputs}, compiler_outputs,
                         "Policy-only override rebuilt bytecode or a project bridge")
        self.execute([*command, "run", *selection, "--no-build", "--vm-instruction-limit", "0"])
        description = json.loads(self.execute([*command, "describe", "--project", project]))
        self.assertEqual(description["vm"]["instructionLimit"], 1)
        self.assertIn("differs", self.execute(
            [*command, "package", *selection, "--no-build"], success=False))

    def test_project_budget_validation_precedes_build(self):
        project = self.project("validation")
        for value in ("-1", "+1", "1.2", str(2**64)):
            with self.subTest(value=value):
                self.execute([WIO, "project", "build", "--project", project, "--backend", "vm",
                              "--vm-instruction-limit", value], success=False)
        self.assertIn("requires --backend vm", self.execute(
            [WIO, "project", "build", "--project", project, "--vm-instruction-limit", "0"], success=False))
        library = self.project("library", "1", "shared")
        self.assertIn("executables only", self.execute(
            [WIO, "project", "build", "--project", library, "--backend", "vm"], success=False))
        self.assertFalse((project / ".wio-build").exists())
        self.assertFalse((library / ".wio-build").exists())


if __name__ == "__main__":
    unittest.main(argv=[__file__, *TEST_ARGS])
