"""Independently verify a completed range-write session and optionally archive raw evidence."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import zipfile


def expected(count):
    checksum = 14695981039346656037
    for index in range(7, 65526):
        checksum = ((checksum ^ ((index * 17 + 3) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return str(checksum ^ (count * 65519))


def verify(folder):
    report = json.loads((folder / "results.json").read_text(encoding="utf-8"))
    journal = [json.loads(line) for line in (folder / "samples.jsonl").read_text(encoding="utf-8").splitlines()]
    assert report["complete"] and report["outputsCorrect"] and report["artifactsUnchanged"]
    assert report["samples"] == journal and report["artifactsBefore"] == report["artifactsAfter"]
    manifest = report["manifest"]
    cases = {case["name"]: case for case in manifest["cases"]}
    assert len(cases) == len(manifest["cases"]) == 8
    checksums = {case["operations"]: expected(case["operations"]) for case in cases.values()}
    for sample in journal:
        case = cases[sample["case"]]
        assert sample["correct"] and sample["process"]["status"] == "passed" and sample["process"]["exitCode"] == 0
        assert sample["workload"] == case["workload"] and sample["implementation"] == case["implementation"]
        assert case["expectedChecksum"] == sample["payload"]["checksum"] == checksums[case["operations"]]
        assert case["operations"] == sample["payload"]["operations"]
        assert json.loads(sample["process"]["stdout"]) == sample["payload"]
        assert case["metric"] == "kernel" and sample["nanoseconds"] == sample["payload"]["elapsedNanoseconds"] > 0

    def budget(phase, implementation):
        key = "finalRepetitions" if phase == "final" else "repetitions"
        overrides = "implementationFinalRepetitions" if phase == "final" else "implementationRepetitions"
        return manifest.get(overrides, {}).get(implementation, manifest[key])

    expected_total = 0
    for phase in ("comparison", "final"):
        blocks = [sample["implementation"] == "wio-vm" for sample in journal if sample["phase"] == phase]
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
    assert len(journal) == expected_total

    def analyze(values, actual):
        ordered = sorted(values)

        def quantile(fraction):
            position = (len(ordered) - 1) * fraction
            left = int(position)
            return ordered[left] + (ordered[min(left + 1, len(ordered) - 1)] - ordered[left]) * (position - left)

        median = statistics.median(values)
        half = len(values) // 2
        result = dict(minimum=min(values), maximum=max(values), mean=statistics.mean(values), median=median,
                      standardDeviation=statistics.stdev(values), standardError=statistics.stdev(values) / math.sqrt(len(values)),
                      medianAbsoluteDeviation=statistics.median(abs(value - median) for value in values),
                      p90=quantile(.90), p95=quantile(.95), p99=quantile(.99),
                      relativeRange=(max(values) - min(values)) / median,
                      relativeDrift=abs(statistics.median(values[:half]) - statistics.median(values[-half:])) / median)
        assert actual["count"] == len(values)
        for key, value in result.items():
            assert math.isclose(actual[key], value, rel_tol=1e-9, abs_tol=1e-5), (key, actual[key], value)
        return result

    summaries = []
    all_qualified = True
    assert len(report["summary"]) == 12
    assert {(row["case"], row["phase"]) for row in report["summary"]} == {
        (case["name"], phase) for case in cases.values() for phase in ("comparison", "final")
        if phase == "comparison" or case["implementation"] in ("wio-native", "wio-vm")
    }
    for row in report["summary"]:
        lane = [s for s in journal if s["case"] == row["case"] and s["phase"] == row["phase"]
                and not s["warmup"] and not s["control"]]
        assert row["expectedSamples"] == len(lane) == budget(row["phase"], row["implementation"])
        stats = analyze([s["nanoseconds"] for s in lane], row["statistics"])
        qualified = stats["relativeRange"] <= manifest["maximumRange"] and stats["relativeDrift"] <= manifest["maximumDrift"]
        rss = statistics.median(s["process"]["peakResidentBytes"] for s in lane)
        assert row["processPeakResidentBytes"]["median"] == rss
        control_difference = None
        if row["implementation"] == "wio-native":
            controls = [s["nanoseconds"] for s in journal if s["case"] == row["case"] and s["phase"] == row["phase"]
                        and not s["warmup"] and s["control"]]
            control_stats = analyze(controls, row["control"])
            control_difference = abs(stats["median"] - control_stats["median"]) / ((stats["median"] + control_stats["median"]) / 2)
            assert math.isclose(row["controlRelativeDifference"], control_difference, rel_tol=1e-9, abs_tol=1e-12)
            qualified = qualified and control_stats["relativeRange"] <= manifest["maximumRange"] \
                and control_stats["relativeDrift"] <= manifest["maximumDrift"] \
                and control_difference <= manifest["maximumControlDifference"]
        assert row["statisticsQualified"] == qualified
        all_qualified = all_qualified and qualified
        summaries.append(dict(phase=row["phase"], case=row["case"], count=len(lane), medianMs=stats["median"] / 1e6,
                              sdMs=stats["standardDeviation"] / 1e6, madMs=stats["medianAbsoluteDeviation"] / 1e6,
                              p95Ms=stats["p95"] / 1e6, p99Ms=stats["p99"] / 1e6,
                              relativeRange=stats["relativeRange"], relativeDrift=stats["relativeDrift"],
                              controlRelativeDifference=control_difference,
                              peakRssKiB=row["processPeakResidentBytes"]["median"] / 1024,
                              statisticsQualified=row["statisticsQualified"]))
    affinity = manifest["cpu"] >= 0 and all(sample["process"]["affinityApplied"] for sample in journal)
    assert report["affinityQualified"] == affinity
    original_power = report["environmentBefore"].get("powerProfile")
    power_unchanged = original_power is not None and original_power == report["environmentAfter"].get("powerProfile")
    assert report["powerProfileUnchanged"] == power_unchanged
    assert report["timingQualified"] == (all_qualified and affinity and report["powerProfileUnchanged"])
    assert all(row["timingQualified"] == (report["timingQualified"] and row["statisticsQualified"])
               for row in report["summary"])
    fixture = folder / "fixture"
    if fixture.is_dir():
        payload = bytes((index * 17 + 3) & 255 for index in range(65536))
        assert (fixture / "payload.bin").read_bytes() == payload
        assert (fixture / "output.bin").read_bytes() == payload[7:65526]
        count = next(iter(cases.values()))["operations"]
        memory = json.loads((fixture / "memory.json").read_text(encoding="utf-8"))
        assert [row["mode"] for row in memory] == ["slice-copy-64k", "borrow-range-64k"]
        for mode, row in enumerate(memory):
            assert row["exitCode"] == 0
            protocol, counters = [json.loads(line) for line in row["stdout"].splitlines() if line]
            assert protocol["checksum"] == checksums[count] and protocol["operations"] == count
            allocations = count * 2 if mode == 0 else 0
            assert counters["available"] and counters["allocations"] == counters["frees"] == allocations
            assert counters["requestedBytes"] == counters["freedBytes"] == allocations * 65519
            assert counters["liveBlocks"] == counters["liveBytes"] == counters["failures"] == 0
    return dict(verified=True, samples=len(journal), complete=True, outputsCorrect=True, artifactsUnchanged=True,
                affinityQualified=report["affinityQualified"], powerProfileUnchanged=report["powerProfileUnchanged"],
                timingQualified=report["timingQualified"], collector=report["toolVersion"], summaries=summaries)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--archive", type=Path, help="New ZIP; includes raw rows and fixture sources, not binaries")
    parser.add_argument("--images", type=Path, help="Frozen preparation directory, required with --archive")
    args = parser.parse_args()
    summary = verify(args.results)
    if args.archive:
        if args.images is None:
            parser.error("--archive requires --images")
        with zipfile.ZipFile(args.archive, "x", compression=zipfile.ZIP_DEFLATED) as archive:
            for name in ("results.json", "samples.jsonl", "report.md"):
                archive.write(args.results / name, name)
            for name in ("manifest.json", "build.json", "memory.json", "main.wio", "probe.h", "main.cpp",
                         "Program.cs", "ByteWriteRange.csproj", "payload.bin", "output.bin"):
                archive.write(args.images / name, "fixture/" + name)
            archive.write(Path(__file__), "verify.py")
            archive.write(Path(__file__).parents[2] / "tests/performance/byte_write_range/prepare.py", "fixture/prepare.py")
        summary["archive"] = dict(path=str(args.archive), bytes=args.archive.stat().st_size,
                                   sha256=hashlib.sha256(args.archive.read_bytes()).hexdigest())
    print(json.dumps(summary, indent=2))
