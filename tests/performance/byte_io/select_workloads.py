"""Select complete workload groups from a frozen manifest without rebuilding/relabeling artifacts."""
import argparse
import hashlib
import json
from pathlib import Path


def select(source, output, workloads, label):
    raw = source.read_bytes()
    manifest = json.loads(raw)
    requested = set(workloads)
    if len(requested) != len(workloads) or not requested:
        raise ValueError("Workloads must be nonempty and unique")
    selected = [row for row in manifest["cases"] if row["workload"] in requested]
    for workload in requested:
        rows = [row for row in selected if row["workload"] == workload]
        if len(rows) != 4 or {row["implementation"] for row in rows} != {"wio-native", "wio-vm", "cpp", "csharp"}:
            raise ValueError("Each selected group must contain all four implementations exactly once")
        if len({(row["operations"], row["expectedChecksum"], row["metric"]) for row in rows}) != 1:
            raise ValueError("Selected group has incompatible contracts")
    manifest["cases"] = selected
    manifest["name"] += " / " + label
    manifest.setdefault("metadata", {})["selection"] = dict(
        label=label, sourceManifest=str(source.resolve()),
        sourceManifestSha256=hashlib.sha256(raw).hexdigest(), workloads=workloads,
        contracts="Only workload groups selected; no build inputs, flags, counts or qualification limits changed")
    manifest["artifacts"].append(str(Path(__file__).resolve()))
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(manifest, indent=2) + "\n")
    print(output.resolve())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workload", nargs="+", required=True)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    select(args.manifest, args.output, args.workload, args.label)
