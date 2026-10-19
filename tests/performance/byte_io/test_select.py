"""Selection is not a build, provenance rewrite or qualification escape hatch."""
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import unittest
import uuid

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("select_fixture", HERE / "select_workloads.py")
selection = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selection)


class SelectionTests(unittest.TestCase):
    def setUp(self):
        scratch = HERE.parents[2] / "build" / "n04-manifest-selection-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        # Keep UUID fixtures as evidence, like the other performance gates.
        # Python 3.13's mode-0700 tempfile directory ACL is not usable under
        # every Windows sandbox token; inherit the workspace's ordinary ACL.
        self.folder = scratch / uuid.uuid4().hex
        self.folder.mkdir()
        self.source = self.folder / "source.json"
        self.output = self.folder / "selected.json"
        self.manifest = dict(name="frozen", repetitions=5, maximumRange=.15, maximumDrift=.1,
                             maximumControlDifference=.1, cpu=2, metadata=dict(revision="old", bridgeFlags=["-O2"]),
                             artifacts=["/frozen/source.cpp"], cases=[
                                 dict(name=group + "-" + impl, workload=group, implementation=impl,
                                      operations=257, expectedChecksum="123", metric="kernel", program="/frozen/" + impl)
                                 for group in ("owned", "reused")
                                 for impl in ("wio-native", "wio-vm", "cpp", "csharp")])

    def run_selection(self, workloads=("owned",)):
        self.source.write_text(json.dumps(self.manifest), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            selection.select(self.source, self.output, list(workloads), "selected")

    def test_complete_group_and_controls_preserved(self):
        original = copy.deepcopy(self.manifest)
        self.run_selection()
        result = json.loads(self.output.read_text())
        self.assertEqual(json.loads(self.source.read_text()), original)
        self.assertEqual(result["cases"], original["cases"][:4])
        for key in ("repetitions", "maximumRange", "maximumDrift", "maximumControlDifference", "cpu"):
            self.assertEqual(result[key], original[key])
        self.assertEqual(result["metadata"]["revision"], "old")
        self.assertEqual(result["metadata"]["bridgeFlags"], ["-O2"])
        self.assertEqual(result["artifacts"][0], original["artifacts"][0])

    def test_unknown_group(self):
        with self.assertRaises(ValueError):
            self.run_selection(("missing",))

    def test_duplicate_group(self):
        with self.assertRaises(ValueError):
            self.run_selection(("owned", "owned"))

    def test_missing_implementation(self):
        self.manifest["cases"].pop(0)
        with self.assertRaises(ValueError):
            self.run_selection()

    def test_mismatched_contract(self):
        self.manifest["cases"][0]["operations"] = 258
        with self.assertRaises(ValueError):
            self.run_selection()

    def test_no_overwrite(self):
        self.output.write_text("keep", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            self.run_selection()
        self.assertEqual(self.output.read_text(), "keep")


if __name__ == "__main__":
    unittest.main()
