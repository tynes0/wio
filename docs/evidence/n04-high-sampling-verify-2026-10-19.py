"""Independent verifier for the archived N04 1000/20 session; no sample collection."""
import json
import math
from pathlib import Path
import statistics
import sys


def verify(folder):
    report = json.loads((folder / "results.json").read_text(encoding="utf-8"))
    journal = [json.loads(line) for line in (folder / "samples.jsonl").read_text(encoding="utf-8").splitlines()]
    assert report["complete"] and report["outputsCorrect"] and report["artifactsUnchanged"]
    assert report["samples"] == journal and report["artifactsBefore"] == report["artifactsAfter"]
    manifest = report["manifest"]
    cases = {case["name"]: case for case in manifest["cases"]}
    expected_checksum = 14695981039346656037
    for index in range(65536):
        expected_checksum = ((expected_checksum ^ ((index * 17 + 3) & 255)) * 1099511628211) & ((1 << 64) - 1)
    expected_checksum = str((expected_checksum * 257) & ((1 << 64) - 1))
    for sample in journal:
        case = cases[sample["case"]]
        assert sample["correct"] and sample["process"]["status"] == "passed" and sample["process"]["exitCode"] == 0
        assert sample["workload"] == case["workload"] and sample["implementation"] == case["implementation"]
        assert case["expectedChecksum"] == sample["payload"]["checksum"] == expected_checksum
        assert case["operations"] == sample["payload"]["operations"] == 257
        assert json.loads(sample["process"]["stdout"]) == sample["payload"]
        assert case["metric"] == "kernel" and sample["nanoseconds"] == sample["payload"]["elapsedNanoseconds"] > 0

    def budget(phase, implementation):
        key = "finalRepetitions" if phase == "final" else "repetitions"
        overrides = "implementationFinalRepetitions" if phase == "final" else "implementationRepetitions"
        return manifest.get(overrides, {}).get(implementation, manifest[key])

    expected_total = 0
    for phase in ("comparison", "final"):
        blocks = [s["implementation"] == "wio-vm" for s in journal if s["phase"] == phase]
        assert blocks == sorted(blocks)
        for case in cases.values():
            implementation = case["implementation"]
            if phase == "final" and implementation not in ("wio-native", "wio-vm"):
                continue
            count = budget(phase, implementation)
            expected_total += (count + manifest["warmups"]) * (2 if implementation == "wio-native" else 1)
            for control in ((False, True) if implementation == "wio-native" else (False,)):
                lane = [s for s in journal if s["case"] == case["name"] and s["phase"] == phase and s["control"] == control]
                assert sum(s["warmup"] for s in lane) == manifest["warmups"]
                assert sum(not s["warmup"] for s in lane) == count
                assert [s["round"] for s in lane] == list(range(count + manifest["warmups"]))
    assert len(journal) == expected_total == 12096

    def analyze(values, actual):
        ordered = sorted(values)
        def quantile(fraction):
            position = (len(ordered) - 1) * fraction
            left = int(position)
            return ordered[left] + (ordered[min(left + 1, len(ordered) - 1)] - ordered[left]) * (position - left)
        median = statistics.median(values)
        half = len(values) // 2
        expected = dict(minimum=min(values), maximum=max(values), mean=statistics.mean(values), median=median,
                        standardDeviation=statistics.stdev(values), standardError=statistics.stdev(values) / math.sqrt(len(values)),
                        medianAbsoluteDeviation=statistics.median(abs(value - median) for value in values),
                        p90=quantile(.90), p95=quantile(.95), p99=quantile(.99),
                        relativeRange=(max(values) - min(values)) / median,
                        relativeDrift=abs(statistics.median(values[:half]) - statistics.median(values[-half:])) / median)
        assert actual["count"] == len(values)
        for key, value in expected.items():
            assert math.isclose(actual[key], value, rel_tol=1e-9, abs_tol=1e-5), (key, actual[key], value)
        return expected

    summaries = []
    assert len(report["summary"]) == 12
    for row in report["summary"]:
        values = [s["nanoseconds"] for s in journal if s["case"] == row["case"] and s["phase"] == row["phase"]
                  and not s["warmup"] and not s["control"]]
        assert row["expectedSamples"] == len(values) == budget(row["phase"], row["implementation"])
        expected = analyze(values, row["statistics"])
        control_difference = None
        if row["implementation"] == "wio-native":
            controls = [s["nanoseconds"] for s in journal if s["case"] == row["case"] and s["phase"] == row["phase"]
                        and not s["warmup"] and s["control"]]
            control_stats = analyze(controls, row["control"])
            control_difference = abs(expected["median"] - control_stats["median"]) / ((expected["median"] + control_stats["median"]) / 2)
            assert math.isclose(row["controlRelativeDifference"], control_difference, rel_tol=1e-9, abs_tol=1e-12)
        summaries.append(dict(phase=row["phase"], case=row["case"], count=len(values),
            medianMs=expected["median"] / 1e6, sdMs=expected["standardDeviation"] / 1e6,
            madMs=expected["medianAbsoluteDeviation"] / 1e6, p95Ms=expected["p95"] / 1e6, p99Ms=expected["p99"] / 1e6,
            relativeRange=expected["relativeRange"], relativeDrift=expected["relativeDrift"],
            controlRelativeDifference=control_difference,
            peakRssKiB=row["processPeakResidentBytes"]["median"] / 1024,
            statisticsQualified=row["statisticsQualified"]))
    return dict(verified=True, samples=len(journal), checksum=expected_checksum,
                complete=report["complete"], outputsCorrect=report["outputsCorrect"],
                artifactsUnchanged=report["artifactsUnchanged"], affinityQualified=report["affinityQualified"],
                powerProfileUnchanged=report["powerProfileUnchanged"], timingQualified=report["timingQualified"],
                collector=report["toolVersion"], summaries=summaries)


if __name__ == "__main__":
    print(json.dumps(verify(Path(sys.argv[1])), indent=2))
