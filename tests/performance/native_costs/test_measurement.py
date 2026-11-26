"""Protocol tests in Python; real language/stdlib behavior is exercised in Wio."""
from __future__ import annotations

import argparse
import contextlib
import ctypes
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest
from unittest import mock
import uuid

import qualify
import run as builder
import measurement as m
sys.path.insert(0, str(qualify.ROOT / "tests/performance/mandelbrot_compute"))
import progress as mandelbrot

OPTIONS = None
SCRATCH = None


class Protocol(unittest.TestCase):
    def test_mandelbrot_fingerprint(self):
        self.assertEqual(mandelbrot.compute_seconds("seconds=0.025000000 iterations=11613930 weighted=3721766287256 interior=94440"), 0.025)
        with self.assertRaises(ValueError):
            mandelbrot.compute_seconds("seconds=0.025000000 iterations=1 weighted=3721766287256 interior=94440")
    def test_numeric_protocol(self):
        row = m.parse_cost("seconds=0.012345678 checksum=18446744073709551615 allocations=2 requested_bytes=16")
        self.assertEqual(row["Status"], "passed")
        self.assertEqual(row["Checksum"], str((1 << 64) - 1))
        self.assertEqual(row["Seconds"], 0.012345678)
        for text in ("seconds=nan checksum=1 allocations=0 requested_bytes=0",
                     "seconds=1,2 checksum=1 allocations=0 requested_bytes=0",
                     "seconds=1.0 checksum=18446744073709551616 allocations=0 requested_bytes=0",
                     "seconds=1.0 checksum=1 allocations=18446744073709551616 requested_bytes=0",
                     "seconds=1.0 checksum=1 allocations=0 requested_bytes=18446744073709551616",
                     "prefix seconds=1.0 checksum=1 allocations=0 requested_bytes=0"):
            with self.subTest(text=text):
                self.assertEqual(m.parse_cost(text)["Status"], "malformed-output")
        self.assertEqual(m.parse_cost("seconds=0.0 checksum=1 allocations=0 requested_bytes=0")["Status"], "invalid-time")

    def test_noise_policy(self):
        self.assertTrue(m.stability([1, 1.01, 1.02], 3)["Accepted"])
        for values in ([1, 1, 3], [1, 1.06, 1.12], [1, 1], [], [1, float("inf"), 1], [1, float("nan"), 1]):
            self.assertFalse(m.stability(values, 3)["Accepted"])
        with self.assertRaises(ValueError):
            m.stability([1, 1, 1], 3, float("nan"))
        self.assertIn("range", m.stability([1, 1, 1, 1, 1.8, 1, 1, 1, 1], 9)["Reasons"])

    def test_round_order(self):
        first = {x: 0 for x in "abc"}
        for index in range(6):
            ordered = m.order(list("abc"), index)
            self.assertEqual(set(ordered), set("abc"))
            first[ordered[0]] += 1
        self.assertEqual(set(first.values()), {2})


class Driver(unittest.TestCase):
    def setUp(self):
        self.root = SCRATCH / uuid.uuid4().hex
        self.artifacts = self.root / "artifacts"
        self.artifacts.mkdir(parents=True)
        for name in ("cpp.exe", "wio.exe", "probe.h"):
            (self.artifacts / name).write_bytes(b"synthetic protocol unit fixture, never executed")
        records = m.snapshot(self.artifacts.iterdir())
        for record in records:
            record["Path"] = Path(record["Path"]).name
        self.build = dict(Suite="core", Count=7, CountersEnabled=True, Artifacts=records)
        self.args = qualify.parser().parse_args(["--artifacts", str(self.artifacts), "--output", str(self.root / "evidence"), "--repetitions", "3"])
        self.counts = {}

    def exercise(self, mode="steady"):
        m.write_json(self.artifacts / "build.json", self.build)
        if mode == "tampered":
            (self.artifacts / "cpp.exe").write_bytes(b"changed after build")

        def fake(argv, cwd, timeout):
            variant, kind = Path(argv[0]).stem, argv[1]
            key = variant, kind
            index = self.counts.get(key, 0)
            self.counts[key] = index + 1
            checksum = "123"
            if self.build.get("Suite") == "extended":
                checksum = str(qualify.GOLDENS[self.build["Count"]][qualify.EXTENDED.index(kind)])
            if mode == "mismatch" and variant == "wio" or mode == "bad-golden":
                checksum = "124"
            seconds = "0.003000000" if mode == "unstable" and index % 2 else "0.001000000"
            counters = 0 if not self.build.get("CountersEnabled", True) and mode != "countermismatch" else 1
            if mode == "mutation":
                (cwd / "probe.h").write_text(str(index))
            text = f"seconds={seconds} checksum={checksum} allocations={counters} requested_bytes={8*counters}"
            return dict(Status="timeout" if mode == "timeout" else "passed", ExitCode=0, WallSeconds=0.001,
                        CpuSeconds=0.001, CpuToWall=1, SystemBusyFraction=0.1,
                        Output="malformed" if mode == "malformed" else text, Error="")

        class SyntheticAffinity:
            info = dict(Supported=mode != "unpinned", LogicalProcessor=0, Policy="unit-test mock, NOT a platform measurement")
            def __init__(self, *_):
                pass
            def __enter__(self):
                return self
            def __exit__(self, *_):
                pass

        policies = [{"PowerState": "before"}, {"PowerState": "after"}] if mode == "powerchange" else [{"PowerState": "synthetic"}] * 2
        with mock.patch.object(m, "Affinity", SyntheticAffinity), mock.patch.object(m, "environment", side_effect=policies), contextlib.redirect_stdout(io.StringIO()):
            result = qualify.qualify(self.args, runner=fake)
        saved = json.loads((self.args.output / "results.json").read_text())
        self.assertEqual(result, saved)
        return result

    def test_steady(self):
        result = self.exercise()
        self.assertTrue(result["Accepted"])
        self.assertEqual(len(result["Samples"]), 76)
        self.assertEqual(sum(r["Warmup"] for r in result["Samples"]), 19)
        self.assertTrue(result["SameBinaryControl"]["Accepted"])

    def test_counterfree(self):
        self.build["CountersEnabled"] = False
        result = self.exercise()
        self.assertTrue(result["Accepted"])
        self.assertTrue(all(r["Allocations"] is None and r["RequestedBytes"] is None for r in result["Samples"]))

    def test_counter_mode_failure(self):
        self.build["CountersEnabled"] = False
        result = self.exercise("countermismatch")
        self.assertFalse(result["OutputsCorrect"])
        self.assertEqual(result["Samples"][-1]["Status"], "counter-mode-mismatch")

    def test_noise_is_not_wrong_output(self):
        result = self.exercise("unstable")
        self.assertTrue(result["OutputsCorrect"])
        self.assertFalse(result["TimingQualified"])
        self.assertIsNone(result["Failure"])

    def test_mismatch(self):
        result = self.exercise("mismatch")
        self.assertEqual(result["Samples"][-1]["Status"], "checksum-mismatch")
        self.assertFalse(result["Accepted"])

    def test_malformed(self):
        self.assertEqual(self.exercise("malformed")["Samples"][-1]["Status"], "malformed-output")

    def test_timeout(self):
        self.assertEqual(self.exercise("timeout")["Samples"][-1]["Status"], "timeout")

    def test_mutation(self):
        result = self.exercise("mutation")
        self.assertFalse(result["ArtifactsUnchanged"])
        self.assertFalse(result["OutputsCorrect"])
        self.assertEqual(len(result["Samples"]), 76)

    def test_bad_provenance(self):
        self.build["Artifacts"][0]["Path"] = "../outside.exe"
        with self.assertRaises(ValueError):
            self.exercise()
        self.assertFalse(self.args.output.exists())

    def test_duplicate_provenance(self):
        self.build["Artifacts"].append(self.build["Artifacts"][0])
        with self.assertRaises(ValueError):
            self.exercise()
        self.assertFalse(self.args.output.exists())

    def test_invalid_metadata(self):
        original = dict(self.build)
        for key, value in (("Count", True), ("Count", 100001), ("Count", 17.0), ("CountersEnabled", "false"), ("Suite", "unknown"), ("Programs", [])):
            with self.subTest(key=key, value=value):
                self.build = dict(original, **{key: value})
                with self.assertRaises(ValueError):
                    self.exercise()
                self.assertFalse(self.args.output.exists())

    def test_unpinned_not_qualified(self):
        result = self.exercise("unpinned")
        self.assertTrue(result["OutputsCorrect"])
        self.assertFalse(result["TimingQualified"])

    def test_power_change_not_qualified(self):
        result = self.exercise("powerchange")
        self.assertTrue(result["OutputsCorrect"])
        self.assertFalse(result["PowerStateUnchanged"])
        self.assertFalse(result["TimingQualified"])

    def test_tampered(self):
        with self.assertRaises(ValueError):
            self.exercise("tampered")
        self.assertFalse(self.args.output.exists())

    def test_legacy_is_explicit(self):
        del self.build["Suite"], self.build["CountersEnabled"]
        self.build.update(Scope="instrumented ordinary new/new[] calls", Flags=[])
        self.args.legacy_instrumented_core = True
        self.assertTrue(self.exercise()["Accepted"])

    def test_independent_golden(self):
        self.build.update(Suite="extended", Count=17)
        self.assertTrue(self.exercise()["Accepted"])

    def test_wrong_independent_golden(self):
        self.build.update(Suite="extended", Count=17)
        self.assertEqual(self.exercise("bad-golden")["Samples"][-1]["Status"], "independent-checksum-mismatch")


class Processes(unittest.TestCase):
    def test_large_output(self):
        row = m.run_process([sys.executable, "-c", "import sys;print('o'*70000);print('e'*70000,file=sys.stderr)"], SCRATCH, 15)
        self.assertEqual(row["Status"], "passed", row)
        self.assertEqual(len(row["Output"]), 70000)
        self.assertEqual(len(row["Error"]), 70000)

    def test_launch_error(self):
        row = m.run_process([SCRATCH / "missing-program"], SCRATCH, 5)
        self.assertEqual(row["Status"], "launch-failed")
        self.assertTrue(row["Error"])

    def test_timeout_retains_output_and_kills_tree(self):
        code = "import subprocess,sys,time;child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)']);print(child.pid,flush=True);print('partial error',file=sys.stderr,flush=True);time.sleep(30)"
        row = m.run_process([sys.executable, "-c", code], SCRATCH, 2)
        self.assertEqual(row["Status"], "timeout", row)
        self.assertEqual(row["Error"], "partial error")
        pid = int(row["Output"])
        if os.name == "nt":
            from ctypes import wintypes as w
            dll = ctypes.WinDLL("kernel32", use_last_error=True)
            dll.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
            dll.OpenProcess.restype = w.HANDLE
            dll.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
            dll.WaitForSingleObject.restype = w.DWORD
            handle = dll.OpenProcess(0x100000, False, pid)
            if handle:
                try:
                    self.assertEqual(dll.WaitForSingleObject(handle, 5000), 0, "descendant survived worker timeout")
                finally:
                    m.windows().CloseHandle(handle)
        else:
            # A killed process may remain a zombie until its new parent reaps it.
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                try:
                    os.kill(pid, 0)
                    stat = Path(f"/proc/{pid}/stat")
                    if stat.exists() and stat.read_text().split()[2] == "Z":
                        break
                except ProcessLookupError:
                    break
                time.sleep(0.05)
            else:
                self.fail("descendant survived process-group timeout")

    def test_affinity_restored(self):
        if os.name == "nt":
            def mask():
                a, b = ctypes.c_size_t(), ctypes.c_size_t()
                self.assertTrue(m.windows().GetProcessAffinityMask(m.windows().GetCurrentProcess(), ctypes.byref(a), ctypes.byref(b)))
                return a.value
            before = mask()
        elif hasattr(os, "sched_getaffinity"):
            before = os.sched_getaffinity(0)
            mask = lambda: os.sched_getaffinity(0)
        else:
            self.skipTest("Hard affinity unsupported; controller explicitly refuses timing qualification")
        with self.assertRaisesRegex(RuntimeError, "deliberate"):
            with m.Affinity() as affinity:
                expected = 1 << affinity.info["LogicalProcessor"] if os.name == "nt" else {affinity.info["LogicalProcessor"]}
                self.assertEqual(mask(), expected)
                script = f"import sys;sys.path.insert(0,{str(Path(m.__file__).parent)!r});import measurement as m,ctypes,os;a=ctypes.c_size_t();b=ctypes.c_size_t();print(os.sched_getaffinity(0) if os.name!='nt' else (m.windows().GetProcessAffinityMask(m.windows().GetCurrentProcess(),ctypes.byref(a),ctypes.byref(b)) and a.value))"
                child = m.run_process([sys.executable, "-c", script], SCRATCH, 15)
                self.assertEqual(child["Output"], str(expected))
                raise RuntimeError("deliberate")
        self.assertEqual(mask(), before)


class WioWorkloads(unittest.TestCase):
    def test_real_wio_workloads(self):
        if OPTIONS is None or OPTIONS.unit_only:
            self.skipTest("unit-only requested")
        for suite in ("core", "extended"):
            with self.subTest(suite=suite):
                argv = ["--output", str(SCRATCH / f"real-wio-{suite}"), "--suite", suite, "--count", "17", "--repetitions", "3", "--allow-unpinned", "--cxx", OPTIONS.cxx]
                if OPTIONS.wio:
                    argv += ["--wio", str(OPTIONS.wio)]
                if OPTIONS.runtime:
                    argv += ["--runtime", str(OPTIONS.runtime)]
                if suite == "extended":
                    argv.append("--disable-allocation-counters")
                result = builder.build(builder.parser().parse_args(argv))
                self.assertTrue(result["OutputsCorrect"], result["Failure"])
                self.assertEqual(len(result["Samples"]), 76 if suite == "core" else 52)
                self.assertTrue(all(r["Status"] == "passed" for r in result["Samples"]))
                if suite == "extended":
                    self.assertTrue(all(r["Allocations"] is None for r in result["Samples"]))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--cxx", default="g++" if os.name == "nt" else "c++")
    p.add_argument("--wio", type=Path)
    p.add_argument("--runtime", type=Path)
    p.add_argument("--scratch", type=Path)
    p.add_argument("--unit-only", action="store_true")
    OPTIONS = p.parse_args()
    SCRATCH = OPTIONS.scratch or qualify.ROOT / "build" / f"native-cost-python-contract-{uuid.uuid4().hex}"
    SCRATCH.mkdir(parents=True, exist_ok=False)
    suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(case) for case in (Protocol, Driver, Processes, WioWorkloads))
    outcome = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(0 if outcome.wasSuccessful() else 1)
