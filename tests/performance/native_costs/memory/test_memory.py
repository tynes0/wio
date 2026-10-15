"""Memory-accounting contracts, plus real Wio allocation/cleanup workloads."""
from __future__ import annotations

import copy
import json
import os
import sys
import unittest
import uuid

import run as memory

OPTIONS = None


def control_rows():
    base = {key: 0 for key in memory.COUNTERS}
    base.update({key: None for key in memory.PROCESS})
    base.update(Phase="baseline", Checksum="0", MemorySource="unsupported", MemoryAvailable=False)
    live = dict(base, Phase="live", Allocations=2, RequestedBytes=196,
                LiveBytes=196, PeakBytes=196, LiveBlocks=2, PeakBlocks=2)
    released = dict(live, Phase="released", Checksum="196", Frees=2, FreedBytes=196, LiveBytes=0, LiveBlocks=0)
    return [base, live, released]


class MemoryContract(unittest.TestCase):
    def parse(self, rows):
        return memory.parse("\n".join(json.dumps(row) for row in rows), "control", 1)

    def test_independent_checksums(self):
        self.assertEqual(memory.checksum("control", 2000), "384004")
        self.assertEqual(memory.checksum("aligned", 17), "8516")
        self.assertEqual(memory.checksum("weak", 2000), "2001")
        self.assertEqual(memory.checksum("object", 1), "0")
        self.assertEqual(memory.checksum("bytes", 17), memory.checksum("object", 17))
        self.assertNotEqual(memory.checksum("string", 17), memory.checksum("object", 17))

    def test_valid_and_unavailable(self):
        rows = self.parse(control_rows())
        self.assertIsNone(rows[1]["ResidentBytes"])
        self.assertEqual(rows[1]["PeakBytes"], 196)
        for source in ("win32-working-set", "linux-proc-status-approximate", "darwin-mach-task-basic-info"):
            rows = control_rows()
            for row in rows:
                row.update(MemorySource=source, MemoryAvailable=True, ResidentBytes=4096,
                           ProcessLifetimePeakResidentBytes=8192)
            self.assertEqual(self.parse(rows)[1]["ResidentBytes"], 4096)

    def test_bad_accounting_and_leaks(self):
        for key, value in (("LiveBytes", 195), ("LiveBlocks", 1), ("PeakBytes", 195),
                           ("AlignedAllocations", 3), ("Allocations", True), ("Failures", -1),
                           ("RequestedBytes", 1 << 64)):
            rows = control_rows()
            rows[1][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.parse(rows)
        rows = control_rows()
        rows[2].update(FreedBytes=195, LiveBytes=1)
        with self.assertRaisesRegex(ValueError, "remain"):
            self.parse(rows)

    def test_protocol_rejection(self):
        for rows in ([], control_rows()[:2], list(reversed(control_rows())), [0, [], False]):
            with self.assertRaises(ValueError):
                self.parse(rows)
        rows = control_rows()
        for key, value in (("ResidentBytes", 0), ("MemoryAvailable", True), ("Checksum", "wrong")):
            invalid = copy.deepcopy(rows)
            invalid[2][key] = value
            with self.assertRaises(ValueError):
                self.parse(invalid)
        for value in ('{"Phase":"live","Phase":"released"}', '{"PeakBytes":NaN}'):
            with self.assertRaises(ValueError):
                memory.parse(value, "control", 1)

    def test_real_wio_memory(self):
        result = memory.run(OPTIONS)
        self.assertTrue(result["Correct"], result["Failure"])
        self.assertFalse(result["TimingQualified"])
        self.assertEqual(len(result["Samples"]), 18)
        source = ("win32-working-set" if os.name == "nt" else "linux-proc-status-approximate"
                  if sys.platform.startswith("linux") else "darwin-mach-task-basic-info"
                  if sys.platform == "darwin" else "unsupported")
        for sample in result["Samples"]:
            for row in sample["Checkpoints"]:
                self.assertEqual(row["MemorySource"], source)
                if source != "unsupported":
                    self.assertTrue(row["MemoryAvailable"])
                    self.assertGreater(row["ResidentBytes"], 0)
                    self.assertGreater(row["ProcessLifetimePeakResidentBytes"], 0)


if __name__ == "__main__":
    OPTIONS = memory.parser().parse_args()
    OPTIONS.output = OPTIONS.output / uuid.uuid4().hex
    unittest.main(argv=["test_memory"], verbosity=2)
