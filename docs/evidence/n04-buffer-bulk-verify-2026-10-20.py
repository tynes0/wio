"""Independently verify a completed ByteBuffer legacy/block session and its raw evidence."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import zipfile


def expected(count):
    checksum = 14695981039346656037
    for index in range(1536):
        checksum = ((checksum ^ ((index * 17 + 3) & 255)) * 1099511628211) & ((1 << 64) - 1)
    return str(checksum ^ (count * 2560))


def verify(folder):
    report = json.loads((folder / "results.json").read_text(encoding="utf-8"))
    journal = [json.loads(line) for line in (folder / "samples.jsonl").read_text(encoding="utf-8").splitlines()]
    assert report["complete"] and report["outputsCorrect"] and report["artifactsUnchanged"]
    assert report["samples"] == journal and report["artifactsBefore"] == report["artifactsAfter"]
    manifest = report["manifest"]
    cases = {case["name"]: case for case in manifest["cases"]}
    assert len(cases) == len(manifest["cases"]) == 6
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
    assert len(report["summary"]) == 10
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
    memory = None
    preparations = folder / "preparations"
    if preparations.is_dir():
        old_source = (preparations / "before/main.wio").read_text(encoding="utf-8")
        new_source = (preparations / "after/main.wio").read_text(encoding="utf-8")
        assert old_source.count("buffer.Write(source);") == 3
        assert old_source.replace("buffer.Write(source);", "buffer.WriteBlock(ref source);") == new_source
        assert (preparations / "before/probe.h").read_bytes() == (preparations / "after/probe.h").read_bytes()
        counters_by_label = {}
        count = next(iter(cases.values()))["operations"]
        for index, label in enumerate(("before", "after")):
            preparation = preparations / label
            provenance = manifest["metadata"]["preparations"][index]
            assert hashlib.sha256((preparation / "manifest.json").read_bytes()).hexdigest() == provenance["sha256"]
            header = next(item for item in manifest["metadata"]["frozenStdInputs"]
                          if item["path"].replace("\\", "/").endswith("/std_utility." + label + ".h"))
            assert hashlib.sha256((preparation / "std_utility.h").read_bytes()).hexdigest() == header["sha256"]
            rows = json.loads((preparations / label / "memory.json").read_text(encoding="utf-8"))
            assert len(rows) == 1 and rows[0]["exitCode"] == 0
            protocol, counters = [json.loads(line) for line in rows[0]["stdout"].splitlines() if line]
            assert protocol["checksum"] == checksums[count] and protocol["operations"] == count
            assert counters["available"] and counters["allocations"] == counters["frees"]
            assert counters["requestedBytes"] == counters["freedBytes"]
            assert counters["liveBlocks"] == counters["liveBytes"] == counters["failures"] == 0
            counters_by_label[label] = counters
        assert counters_by_label["after"]["allocations"] <= counters_by_label["before"]["allocations"]
        assert counters_by_label["after"]["requestedBytes"] <= counters_by_label["before"]["requestedBytes"]
        memory = counters_by_label
    return dict(verified=True, samples=len(journal), complete=True, outputsCorrect=True, artifactsUnchanged=True,
                affinityQualified=report["affinityQualified"], powerProfileUnchanged=report["powerProfileUnchanged"],
                timingQualified=report["timingQualified"], collector=report["toolVersion"],
                memory=memory, summaries=summaries)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--archive", type=Path, help="New ZIP with raw rows and fixture sources, not binaries")
    parser.add_argument("--before", type=Path)
    parser.add_argument("--after", type=Path)
    args = parser.parse_args()
    summary = verify(args.results)
    if args.archive:
        if args.before is None or args.after is None:
            parser.error("--archive requires --before and --after preparation folders")
        root = Path(__file__).resolve().parents[2]
        with zipfile.ZipFile(args.archive, "x", compression=zipfile.ZIP_DEFLATED) as archive:
            for name in ("results.json", "samples.jsonl", "report.md"):
                archive.write(args.results / name, name)
            for label, images in (("before", args.before), ("after", args.after)):
                for name in ("manifest.json", "build.json", "memory.json", "main.wio", "probe.h",
                             "main.cpp", "Program.cs", "ByteBufferBulk.csproj"):
                    archive.write(images / name, "preparations/" + label + "/" + name)
                archive.write(images / ("std_utility." + label + ".h"), "preparations/" + label + "/std_utility.h")
            archive.write(Path(__file__), "verify.py")
            for name in ("combine.py", "prepare.py"):
                archive.write(root / "tests/performance/byte_buffer_bulk" / name, "fixture/" + name)
            archive.write(root / "tools/wio-bench/main.wio", "collector/main.wio")
        summary["archive"] = dict(path=str(args.archive), bytes=args.archive.stat().st_size,
                                   sha256=hashlib.sha256(args.archive.read_bytes()).hexdigest())
    print(json.dumps(summary, indent=2))
