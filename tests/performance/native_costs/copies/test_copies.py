"""Copy-traffic protocol checks and real Wio/ordinary-O2 output equivalence."""
from __future__ import annotations

import json
import unittest
import uuid

import run as copies

OPTIONS = None


class CopyContract(unittest.TestCase):
    def control(self):
        return dict(Checksum="13162035316282210040", CountersEnabled=True,
                    MemcpyCalls=2, MemcpyBytes=7, MemmoveCalls=2, MemmoveBytes=16)

    def test_independent_goldens(self):
        expected = dict(control="13162035316282210040", string_owned="18024119753710511888",
                        text_owned="11209124219353850944", component_owned="18024119753710511895",
                        scalar_component="1")
        for kind, value in expected.items():
            self.assertEqual(copies.checksum(kind, 1), value)
        self.assertEqual(copies.checksum("control", 17), "3735637692064323380")
        self.assertEqual(copies.checksum("scalar_component", 17), "13498714292532788257")
        self.assertEqual(len(copies.STRING), 45)
        self.assertIn(0, copies.STRING)
        self.assertEqual(len(copies.TEXT), 42)

    def test_valid_and_counter_free(self):
        self.assertEqual(copies.parse(json.dumps(self.control()), "control", 1, True), self.control())
        row = self.control()
        row.update(CountersEnabled=False, **{key: None for key in copies.METRICS})
        self.assertIsNone(copies.parse(json.dumps(row), "control", 1, False)["MemcpyBytes"])
        row["MemcpyBytes"] = 0
        with self.assertRaisesRegex(ValueError, "null"):
            copies.parse(json.dumps(row), "control", 1, False)

    def test_calibration_and_mode_rejection(self):
        for key, value in (("MemcpyCalls", 0), ("MemmoveBytes", 15), ("Checksum", "wrong"),
                           ("CountersEnabled", False)):
            row = self.control()
            row[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                copies.parse(json.dumps(row), "control", 1, True)

    def test_strict_schema(self):
        for value in ("[]", "null", "{}", '{"MemcpyBytes":NaN}',
                      '{"MemcpyCalls":1,"MemcpyCalls":2}'):
            with self.assertRaises(ValueError):
                copies.parse(value, "control", 1, True)
        for invalid in (True, -1, 1 << 64, 1.5, None):
            row = self.control()
            row["MemcpyBytes"] = invalid
            with self.assertRaises(ValueError):
                copies.parse(json.dumps(row), "control", 1, True)

    def test_real_wio(self):
        result = copies.run(OPTIONS)
        self.assertTrue(result["Correct"], result["Failure"])
        self.assertFalse(result["TimingQualified"])
        self.assertEqual(len(result["Samples"]), 78)
        for row in result["Samples"]:
            self.assertEqual(row["Process"]["Status"], "passed")
            if row["Variant"] == "plain":
                self.assertTrue(all(row["Metrics"][key] is None for key in copies.METRICS))


if __name__ == "__main__":
    OPTIONS = copies.parser().parse_args()
    OPTIONS.output = OPTIONS.output / uuid.uuid4().hex
    unittest.main(argv=["test_copies"], verbosity=2)
