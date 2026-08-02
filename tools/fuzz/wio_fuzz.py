#!/usr/bin/env python3
"""Deterministic Wio compiler pipeline fuzzer.

Every candidate is checked through token dumping, normal semantic checking,
C++ emission, and backend syntax validation when it is a valid Wio program.
The script uses only Python's standard library so it can run in CI and release
qualification without installing a fuzzing framework.
"""

from __future__ import annotations

import argparse
import random
import re
import shutil
import subprocess
import sys
from pathlib import Path


EXPECTED_EXIT_CODES = {0, 1}
TOKEN_FRAGMENTS = [
    b"${", b"}", b'"', b"::", b"<", b">", b",", b";", b"fn", b"object",
    b"component", b"ref", b"view", b"deref", b"match", b"=>", b"/*", b"*/",
    b"[", b"]", b"(", b")", b"{", b"}", b"\x00", b"\xff", b"\xc0\xaf",
]


class FuzzFailure(RuntimeError):
    pass


def run_process(command: list[str], timeout: float, cwd: Path) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise FuzzFailure(f"timeout after {timeout:.1f}s: {' '.join(command)}") from exc

    output = completed.stdout.decode("utf-8", errors="replace")
    if completed.returncode not in EXPECTED_EXIT_CODES:
        raise FuzzFailure(
            f"abnormal exit {completed.returncode}: {' '.join(command)}\n{output[-4000:]}"
        )
    return completed.returncode, output


def diagnostic_count(output: str) -> int:
    return len(re.findall(r"\b(?:Error|Fatal|Critical)\s*\[", output, re.IGNORECASE))


def mutate(data: bytes, rng: random.Random, iteration: int) -> bytes:
    if not data:
        data = b"fn Entry() -> i32 { return 0; }\n"
    result = bytearray(data)
    operation = iteration % 6
    position = rng.randrange(len(result) + 1)

    if operation == 0:
        result[position:position] = rng.choice(TOKEN_FRAGMENTS)
    elif operation == 1 and result:
        result[rng.randrange(len(result))] = rng.randrange(256)
    elif operation == 2 and result:
        start = rng.randrange(len(result))
        del result[start : min(len(result), start + rng.randrange(1, 17))]
    elif operation == 3 and result:
        start = rng.randrange(len(result))
        chunk = bytes(result[start : min(len(result), start + rng.randrange(1, 33))])
        result[position:position] = chunk
    elif operation == 4:
        depth = 1 + (iteration % 48)
        result[position:position] = (b"(" * depth) + b"0" + (b")" * depth)
    else:
        result[position:position] = b'$"outer ${Echo($"inner ${\"quoted\"}")} tail"'

    return bytes(result[: 256 * 1024])


def validate_candidate(
    candidate: Path,
    wio: Path,
    backend: Path,
    root: Path,
    timeout: float,
    max_diagnostics: int,
) -> None:
    generated = Path(str(candidate) + ".cpp")
    generated.unlink(missing_ok=True)
    base = [str(wio), str(candidate), "--no-builtin"]
    token_code, token_output = run_process(base + ["--show-tokens", "--dry-run"], timeout, root)
    check_code, check_output = run_process(base + ["--dry-run"], timeout, root)

    if token_code != check_code:
        raise FuzzFailure(
            f"--show-tokens changed compilation outcome for {candidate.name}: "
            f"tokens={token_code}, check={check_code}"
        )

    combined = token_output + check_output
    count = diagnostic_count(combined)
    if count > max_diagnostics:
        raise FuzzFailure(
            f"diagnostic budget exceeded for {candidate.name}: {count} > {max_diagnostics}\n"
            f"{combined[-4000:]}"
        )
    if len(combined.encode("utf-8")) > 512 * 1024:
        raise FuzzFailure(f"output budget exceeded for {candidate.name}")

    if check_code != 0:
        return

    emit_code, emit_output = run_process(base + ["--emit-cpp"], timeout, root)
    if emit_code != 0:
        raise FuzzFailure(
            f"dry-run/emit differential for {candidate.name}\n{emit_output[-4000:]}"
        )

    if not generated.is_file() or generated.stat().st_size == 0:
        raise FuzzFailure(f"successful emission produced no C++ for {candidate.name}")

    backend_command = [
        str(backend), "-std=c++20", "-fsyntax-only", str(generated),
        f"-I{root / 'runtime' / 'include'}", f"-I{root / 'sdk' / 'include'}",
    ]
    backend_code, backend_output = run_process(backend_command, timeout, root)
    if backend_code != 0:
        raise FuzzFailure(
            f"semantically valid Wio generated invalid C++ for {candidate.name}\n"
            f"{backend_output[-4000:]}"
        )
    generated.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wio", required=True, type=Path)
    parser.add_argument("--backend", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--corpus", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--seed", type=int, default=0x57494F)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--max-diagnostics", type=int, default=64)
    args = parser.parse_args()

    args.work.mkdir(parents=True, exist_ok=True)
    seeds = sorted(args.corpus.glob("*.wio"))
    if not seeds:
        raise FuzzFailure(f"no corpus files found in {args.corpus}")

    # Preserve corpus basenames in the work directory so multi-file fixtures
    # such as import cycles resolve exactly as they do in a real project.
    for seed in seeds:
        shutil.copyfile(seed, args.work / seed.name)

    rng = random.Random(args.seed)
    candidates: list[tuple[str, bytes]] = [(seed.stem, seed.read_bytes()) for seed in seeds]
    candidates.extend(
        [
            ("invalid_utf8", b"fn Entry() -> i32 { let x = \"\xff\xfe\"; return 0; }\n"),
            ("arbitrary_tokens", bytes(range(256))),
        ]
    )
    for index in range(args.iterations):
        name, seed = candidates[index % len(candidates)]
        candidates.append((f"mut_{index:04d}_{name}", mutate(seed, rng, index)))

    for index, (name, data) in enumerate(candidates):
        candidate = args.work / f"{index:04d}_{name}.wio"
        candidate.write_bytes(data)
        try:
            validate_candidate(
                candidate, args.wio, args.backend, args.root,
                args.timeout, args.max_diagnostics,
            )
        except FuzzFailure:
            repro = args.work / "failure-repro.wio"
            shutil.copyfile(candidate, repro)
            print(f"fuzz failure; reproducer: {repro}", file=sys.stderr)
            raise

    print(
        f"wio-fuzz-ok candidates={len(candidates)} seed={args.seed} "
        f"iterations={args.iterations}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except FuzzFailure as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
