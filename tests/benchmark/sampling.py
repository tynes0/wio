"""New controller scheduling/profile contract only; synthetic timings are not performance data."""
import argparse
from collections import Counter
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[2]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    rows = []

    def run(command, accepted=(0, 2)):
        result = subprocess.run([str(x) for x in command], cwd=folder,
                                env=dict(os.environ, WIO_ROOT=str(ROOT)), capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=180)
        row = dict(command=[str(x) for x in command], exitCode=result.returncode,
                   stdout=result.stdout, stderr=result.stderr)
        rows.append(row)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode not in accepted:
            raise RuntimeError(str(row))

    def write(name, value):
        target = folder / (name + ".json")
        target.write_text(json.dumps(value), encoding="utf-8")
        return target

    # Use an ordinary image: Windows Store Python aliases are not hashable
    # images, while their protected physical images may not be CreateProcess-able.
    child = folder / ("protocol-child.exe" if os.name == "nt" else "protocol-child")
    child_source = Path(__file__).with_name("sampling_child.cpp")
    run([args.cxx, "-std=c++20", "-O2", *(["-static"] if os.name == "nt" else []),
         child_source, "-o", child], (0,))
    cases = [dict(name=kind, implementation=kind, workload="synthetic-sampling-only", operations=1,
                  expectedChecksum="42", metric="kernel", program=str(child),
                  args=["child"], cwd=str(folder))
             for kind in ("wio-native", "cpp", "csharp", "wio-vm")]
    base = dict(schemaVersion=1, name="Synthetic sampling contract; NOT language performance",
                repetitions=3, finalRepetitions=3, warmups=1, checkpointInterval=7,
                artifacts=[str(Path(__file__).resolve())], cases=cases)
    custom = copy.deepcopy(base)
    custom.update(implementationRepetitions={"wio-native": 5, "cpp": 4, "wio-vm": 3},
                  implementationFinalRepetitions={"wio-native": 4, "wio-vm": 5})
    runner = args.runner.resolve()
    for name, manifest, expected_count in (("legacy", base, 32), ("custom", custom, 41)):
        source = write(name, manifest)
        output = folder / (name + "-run")
        run([runner, "run", source, "--output", output])
        report = json.loads((output / "results.json").read_text())
        journal = [json.loads(line) for line in (output / "samples.jsonl").read_text().splitlines()]
        assert report["complete"] and report["outputsCorrect"] and report["artifactsUnchanged"]
        assert len(report["samples"]) == expected_count and journal == report["samples"]
        assert "Statistics stable | Session qualified" in (output / "report.md").read_text()
        for row in report["summary"]:
            mapping = manifest.get("implementationFinalRepetitions" if row["phase"] == "final" else "implementationRepetitions", {})
            expected = mapping.get(row["implementation"], 3)
            assert row["expectedSamples"] == row["statistics"]["count"] == expected
            assert row["statistics"]["median"] == 1000000
            if row["implementation"] == "wio-native":
                assert row["control"]["count"] == expected
        for phase in ("comparison", "final"):
            blocks = [s["implementation"] == "wio-vm" for s in report["samples"] if s["phase"] == phase]
            assert blocks == sorted(blocks), "VM must not interrupt the non-VM block"
        assert {s["implementation"] for s in journal if s["phase"] == "final"} == {"wio-native", "wio-vm"}
        old = (output / "results.json").read_bytes()
        run([runner, "run", source, "--output", output], (1,))
        assert (output / "results.json").read_bytes() == old

    for index, (key, value) in enumerate([
        ("implementationRepetitions", []), ("implementationRepetitions", {"wio-vm": 2}),
        ("implementationRepetitions", {"wio-vm": 10001}), ("implementationRepetitions", {"wio-vm": "20"}),
        ("implementationRepetitions", {"wio-vm": True}), ("implementationRepetitions", {"wio-vmm": 20}),
        ("implementationFinalRepetitions", {"cpp": 20}), ("checkpointInterval", 0),
        ("checkpointInterval", "100"), ("checkpointInterval", 10001)
    ]):
        bad = copy.deepcopy(base)
        bad[key] = value
        run([runner, "validate", write(f"bad-{index}", bad)], (1,))

    high = copy.deepcopy(base)
    high.update(repetitions=10000, finalRepetitions=10000,
                implementationRepetitions={"wio-vm": 20}, implementationFinalRepetitions={"wio-vm": 10})
    run([runner, "validate", write("high-validate-only", high)], (0,))
    bad = copy.deepcopy(base)
    bad["cases"][0]["args"][-1] = "wrong"
    output = folder / "wrong-checksum"
    run([runner, "run", write("wrong", bad), "--output", output], (1,))
    report = json.loads((output / "results.json").read_text())
    journal = [json.loads(line) for line in (output / "samples.jsonl").read_text().splitlines()]
    assert report["complete"] and not report["outputsCorrect"] and journal == report["samples"]
    assert len(journal) == 32 and any(not s["correct"] for s in journal)

    spec = importlib.util.spec_from_file_location("sampling_profile", ROOT / "tools/wio-bench/sampling_profile.py")
    profile = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(profile)
    source = write("profile-source", base)
    raw = source.read_bytes()
    for repetitions in (1000, 10000):
        target = folder / f"profile-{repetitions}.json"
        profile.derive(source, target, repetitions, 20, 100)
        derived = json.loads(target.read_text())
        assert derived["cases"] == base["cases"] and derived["artifacts"][:1] == base["artifacts"]
        assert derived["repetitions"] == derived["finalRepetitions"] == repetitions
        assert derived["implementationRepetitions"] == derived["implementationFinalRepetitions"] == {"wio-vm": 20}
        assert source.read_bytes() == raw
        run([runner, "validate", target], (0,))
        try:
            profile.derive(source, target, repetitions, 20, 100)
        except FileExistsError:
            pass
        else:
            raise AssertionError("Profile derivation overwrote evidence")
    counts = Counter((s["phase"], s["implementation"], s["control"], s["warmup"]) for s in journal)
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows,
        checks=["legacy 32/custom 41 samples", "native A/A exact counts", "separate VM blocks",
                "raw journal equals report", "10 invalid manifests", "1000/10000 profiles validate",
                "no overwrite", "all wrong-checksum samples retained"],
        negativeSampleCounts={str(k): v for k, v in counts.items()}), indent=2) + "\n")
    print(folder / "results.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    verify(parser.parse_args())
