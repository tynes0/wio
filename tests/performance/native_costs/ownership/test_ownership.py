"""Validate attribution parsing and actual Wio ownership workloads with GCC."""
from __future__ import annotations

import copy
from pathlib import Path
import unittest
import uuid

import run as ownership

OPTIONS = None


class Attribution(unittest.TestCase):
    def test_checksum(self):
        self.assertEqual(ownership.expected_checksum("owned", 1), "0")
        self.assertEqual(ownership.expected_checksum("weak", 17), "3335769950788531865")
        self.assertEqual(ownership.expected_checksum("control", 2000), "11505999919603546472")

    def test_strict_event_lookup(self):
        methods = ("IncStrong", "DecStrong", "IncWeak", "DecWeak", "TryIncStrong", "DestroyInternal")
        rows = [dict(demangled_name=f"wio::runtime::RefCountedObject::{name}() const", execution_count=index)
                for index, name in enumerate(methods)]
        self.assertEqual(ownership.summarize(rows), dict(zip(methods, range(6))))
        for invalid in (rows[:-1], rows + [rows[0]]):
            with self.assertRaises(ValueError):
                ownership.summarize(invalid)

    def test_template_transfers(self):
        rows = [dict(demangled_name=f"wio::runtime::Ref<{kind}>::{operation}", execution_count=number)
                for kind, operation, number in (
                    ("A", "Ref(wio::runtime::Ref<A> const&)", 2),
                    ("B", "Ref(wio::runtime::Ref<B> const&)", 3),
                    ("A", "Ref(wio::runtime::Ref<A>&&)", 4),
                    ("A", "operator=(wio::runtime::Ref<A> const&)", 5),
                    ("A", "operator=(wio::runtime::Ref<A>&&)", 6),
                    ("A", "Ref(A*)", 100))]
        rows.append(dict(demangled_name="wio::runtime::WeakRef<A>::WeakRef(wio::runtime::WeakRef<A> const&)", execution_count=100))
        self.assertEqual(ownership.transfers(rows), dict(CopyConstruct=5, MoveConstruct=4, CopyAssign=5, MoveAssign=6))

    def test_profile_validation(self):
        header = Path(__file__).resolve()
        document = dict(files=[dict(file=str(header), functions=[dict(name="f", execution_count=1)])])
        self.assertEqual(len(ownership.functions(document, header)), 1)
        invalid = copy.deepcopy(document)
        invalid["files"][0]["functions"] *= 2
        with self.assertRaises(ValueError):
            ownership.functions(invalid, header)
        for number in (-1, True, 1.5):
            invalid = copy.deepcopy(document)
            invalid["files"][0]["functions"][0]["execution_count"] = number
            with self.assertRaises(ValueError):
                ownership.functions(invalid, header)
        with self.assertRaises(ValueError):
            ownership.functions(dict(files=[]), header)

    def test_real_wio(self):
        result = ownership.run(OPTIONS)
        self.assertTrue(result["Correct"], result["Failure"])
        self.assertFalse(result["TimingQualified"])
        self.assertEqual(len(result["Samples"]), 15)
        self.assertEqual(sum(s["Events"]["DestroyInternal"] for s in result["Samples"]), 10090)


if __name__ == "__main__":
    p = ownership.parser()
    p.set_defaults(counts=[1, 17, 2000])
    OPTIONS = p.parse_args()
    OPTIONS.output = OPTIONS.output / uuid.uuid4().hex
    unittest.main(argv=["test_ownership"], verbosity=2)
