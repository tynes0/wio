"""Combine frozen legacy/block images; one shared output contract, no sampling or rebuilding."""
import argparse
import hashlib
import json
from pathlib import Path

from prepare import expected

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def combine(before, after, output):
    before, after = before.resolve(), after.resolve()
    old = json.loads((before / "manifest.json").read_text(encoding="utf-8"))
    new = json.loads((after / "manifest.json").read_text(encoding="utf-8"))
    assert old["metadata"]["nativeFlags"] == new["metadata"]["nativeFlags"]
    assert old["metadata"]["bridgeFlags"] == new["metadata"]["bridgeFlags"]
    assert old["metadata"]["runtimeBuild"]["profile"] == new["metadata"]["runtimeBuild"]["profile"]
    old_source = (before / "main.wio").read_text(encoding="utf-8")
    assert old_source.count("buffer.Write(source);") == 3
    assert old_source.replace("buffer.Write(source);", "buffer.WriteBlock(ref source);") == (after / "main.wio").read_text(encoding="utf-8")
    assert (before / "probe.h").read_bytes() == (after / "probe.h").read_bytes()
    cases = []
    for label, manifest in (("legacy", old), ("block", new)):
        for case in manifest["cases"]:
            if label == "legacy" and case["implementation"] not in ("wio-native", "wio-vm"):
                continue
            case = dict(case, name=label + "-" + case["implementation"], workload="n04-byte-buffer-mixed-1k-v1")
            assert case["expectedChecksum"] == expected(0, case["operations"])
            assert case["operations"] == old["cases"][0]["operations"]
            cases.append(case)
    assert len(cases) == 6
    # Snapshot the std inputs used at build time, rather than calling today's
    # changed header the source of the already-frozen legacy executable.
    snapshots = [before / "buffer.before.wio", before / "std_utility.before.h",
                 after / "buffer.after.wio", after / "std_utility.after.h"]
    assert all(path.is_file() for path in snapshots)
    mutable_sources = {str((ROOT / name).resolve()) for name in ("std/buffer.wio", "runtime/include/std_utility.h")}
    artifacts = sorted({str(Path(path).resolve()) for manifest in (old, new) for path in manifest["artifacts"]
                        if str(Path(path).resolve()) not in mutable_sources})
    artifacts += [str(path) for path in snapshots]
    artifacts += [str(Path(__file__).resolve()), str(ROOT / "tools/wio-bench/main.wio")]
    result = dict(new, name="N04 legacy Write versus borrowed WriteBlock / sampling-1000-vm-20",
                  repetitions=1000, finalRepetitions=1000,
                  implementationRepetitions={"wio-vm": 20}, implementationFinalRepetitions={"wio-vm": 20},
                  checkpointInterval=1000, artifacts=artifacts, cases=cases)
    result["metadata"] = dict(new["metadata"],
        comparison="API strategy, not automatic legacy Write or unchanged-source compiler speedup; C++/C# lanes use borrowed block writes",
        baselinePreparationNote="Original baseline manifest predates the safe API decision; this combined metadata defines the actual comparison, not its old unchanged-input boilerplate",
        sourceDelta="Exactly three Write(source) calls become WriteBlock(ref source); Work still takes an owned snapshot in all languages",
        runtimeArchives=[manifest["metadata"]["runtimeBuild"] for manifest in (old, new)],
        runtimeNote="Native images retain their recorded preparation archives under the same O2 profile; the header update triggered an archive rebuild. Both bytecode versions are measured with the same current capacity-corrected VM host. This measures the update/API strategy, not an isolated compiler transformation",
        frozenStdInputs=[dict(path=str(path), sha256=digest(path)) for path in snapshots],
        preparations=[dict(path=str(path / "manifest.json"), sha256=digest(path / "manifest.json"),
                           revision=manifest["metadata"]["revision"])
                      for path, manifest in ((before, old), (after, new))])
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    print(output.resolve())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    combine(args.before, args.after, args.output)
