"""Derive a high-sample manifest; never rebuild workloads or collect measurements."""
import argparse
import hashlib
import json
from pathlib import Path


def derive(source, output, repetitions=1000, vm_repetitions=20, checkpoint_interval=100):
    if not 3 <= repetitions <= 10000 or not 3 <= vm_repetitions <= 10000:
        raise ValueError("Repetition counts must be 3..10000")
    if not 1 <= checkpoint_interval <= 10000:
        raise ValueError("Checkpoint interval must be 1..10000")
    raw = source.read_bytes()
    manifest = json.loads(raw)
    # Preserve relative-path semantics when deriving into another directory.
    def resolved(path):
        return str((source.resolve().parent / path).resolve())
    manifest["artifacts"] = [resolved(path) for path in manifest["artifacts"]]
    for section in ("cases", "versions", "builds"):
        for command in manifest.get(section, []):
            command["cwd"] = resolved(command.get("cwd", "."))
    manifest.update(repetitions=repetitions, finalRepetitions=repetitions,
                    implementationRepetitions={"wio-vm": vm_repetitions},
                    implementationFinalRepetitions={"wio-vm": vm_repetitions},
                    checkpointInterval=checkpoint_interval)
    manifest["name"] += f" / sampling-{repetitions}-vm-{vm_repetitions}"
    manifest.setdefault("metadata", {})["samplingProfile"] = dict(
        sourceManifest=str(source.resolve()), sourceManifestSha256=hashlib.sha256(raw).hexdigest(),
        repetitions=repetitions, vmRepetitions=vm_repetitions, checkpointInterval=checkpoint_interval,
        policy="Independent process samples, no trimming/retries; VM in separate blocks; workload counts/checksums/flags/limits unchanged")
    manifest["artifacts"] += [str(Path(__file__).resolve()), str(Path(__file__).with_name("main.wio").resolve())]
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(manifest, indent=2) + "\n")
    print(output.resolve())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=1000)
    parser.add_argument("--vm-repetitions", type=int, default=20)
    parser.add_argument("--checkpoint-interval", type=int, default=100)
    args = parser.parse_args()
    derive(args.manifest, args.output, args.repetitions, args.vm_repetitions, args.checkpoint_interval)
