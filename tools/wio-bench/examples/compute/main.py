"""Optional Python lane; same bounded integer recurrence and runtime argument."""
import json
import sys
import time


def compute(count):
    state, checksum = 12345, 0
    for _ in range(count):
        state = state * 48271 % 2147483647
        checksum += state
    return checksum


count = int(sys.argv[1])
if not 1 <= count <= 1000000:
    raise SystemExit(1)
compute(count)
compute(count)
start = time.perf_counter_ns()
checksum = compute(count)
elapsed = time.perf_counter_ns() - start
print(json.dumps(dict(schemaVersion=1, elapsedNanoseconds=elapsed, operations=count, checksum=str(checksum))))
