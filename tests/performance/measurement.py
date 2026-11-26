"""Standard-library measurement support; never imported by production Wio.

Windows workers enter an owning job BEFORE starting a benchmark. Killing the
worker closes its last job handle and terminates its children. POSIX workers
own a new process group. Python-worker startup is outside child wall timing.
"""
from __future__ import annotations

import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import signal
import statistics
import subprocess
import sys
import threading
import time


def snapshot(paths):
    return [{"Path": str(p), "Bytes": p.stat().st_size,
             "Hash": hashlib.sha256(p.read_bytes()).hexdigest().upper()}
            for p in sorted({Path(p).resolve() for p in paths})]


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def stability(values, expected, max_range=0.15, max_drift=0.10):
    if expected < 3 or any(not math.isfinite(v) or v <= 0 for v in (max_range, max_drift)):
        raise ValueError("Require >=3 expected samples and finite positive limits")
    reasons = []
    if len(values) != expected:
        reasons.append("incomplete")
    if len(values) < 3:
        reasons.append("insufficient-samples")
    if any(not math.isfinite(v) or v <= 0 for v in values):
        reasons.append("invalid-time")
    result = dict(Accepted=False, Reasons=reasons, Samples=len(values), Expected=expected,
                  Median=None, Min=None, Max=None, RelativeRange=None, RelativeDrift=None,
                  MaxRelativeRange=max_range, MaxRelativeDrift=max_drift)
    if len(values) >= 3 and "invalid-time" not in reasons:
        median = statistics.median(values)
        half = len(values) // 2
        spread = (max(values) - min(values)) / median
        drift = abs(statistics.median(values[-half:]) - statistics.median(values[:half])) / median
        if spread > max_range:
            reasons.append("range")
        if drift > max_drift:
            reasons.append("drift")
        result.update(Median=median, Min=min(values), Max=max(values), RelativeRange=spread, RelativeDrift=drift)
    result["Accepted"] = not reasons
    return result


def order(cases, round_index):
    if not cases or round_index < 0:
        raise ValueError("Require cases and a nonnegative round")
    offset = (round_index // 2) % len(cases)
    result = cases[offset:] + cases[:offset]
    return result[::-1] if round_index % 2 else result


def parse_cost(output):
    match = re.fullmatch(r"seconds=([0-9]+\.[0-9]+) checksum=([0-9]+) allocations=([0-9]+) requested_bytes=([0-9]+)", output)
    result = dict(Status="malformed-output", Seconds=None, Checksum=None, Allocations=None, RequestedBytes=None)
    if not match:
        return result
    if any(len(match[i]) > 20 for i in range(2, 5)):
        return result
    seconds = float(match[1])
    numbers = [int(match[i]) for i in range(2, 5)]
    if any(v > (1 << 64) - 1 for v in numbers):
        return result
    result.update(Status="passed" if math.isfinite(seconds) and seconds > 0 else "invalid-time",
                  Seconds=seconds if math.isfinite(seconds) else None, Checksum=str(numbers[0]),
                  Allocations=numbers[1], RequestedBytes=numbers[2])
    return result


def windows():
    from ctypes import wintypes as w
    dll = ctypes.WinDLL("kernel32", use_last_error=True)
    declarations = {
        "GetCurrentProcess": ([], w.HANDLE),
        "GetActiveProcessorGroupCount": ([], w.WORD),
        "GetProcessAffinityMask": ([w.HANDLE, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)], w.BOOL),
        "SetProcessAffinityMask": ([w.HANDLE, ctypes.c_size_t], w.BOOL),
        "GetSystemTimes": ([ctypes.POINTER(w.FILETIME)] * 3, w.BOOL),
        "GetProcessTimes": ([w.HANDLE] + [ctypes.POINTER(w.FILETIME)] * 4, w.BOOL),
        "CreateJobObjectW": ([ctypes.c_void_p, w.LPCWSTR], w.HANDLE),
        "SetInformationJobObject": ([w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD], w.BOOL),
        "AssignProcessToJobObject": ([w.HANDLE, w.HANDLE], w.BOOL),
        "CloseHandle": ([w.HANDLE], w.BOOL),
    }
    for name, (args, result) in declarations.items():
        getattr(dll, name).argtypes = args
        getattr(dll, name).restype = result
    return dll


class Affinity:
    def __init__(self, cpu=-1, allow_unpinned=False):
        self.cpu, self.allow_unpinned = cpu, allow_unpinned
        self.original = None
        self.info = dict(Supported=False, LogicalProcessor=None, Policy="unsupported; timing not qualified")

    def __enter__(self):
        if os.name == "nt":
            self.dll = windows()
            if ctypes.sizeof(ctypes.c_void_p) != 8 or self.dll.GetActiveProcessorGroupCount() != 1:
                raise RuntimeError("Windows timing needs x64 and one processor group; refusing ambiguous affinity")
            mask, system_mask = ctypes.c_size_t(), ctypes.c_size_t()
            if not self.dll.GetProcessAffinityMask(self.dll.GetCurrentProcess(), ctypes.byref(mask), ctypes.byref(system_mask)):
                raise ctypes.WinError(ctypes.get_last_error())
            allowed = {i for i in range(64) if mask.value & (1 << i)}
            self.original = mask.value
        elif hasattr(os, "sched_getaffinity") and hasattr(os, "sched_setaffinity"):
            allowed = os.sched_getaffinity(0)
            self.original = set(allowed)
        else:
            if not self.allow_unpinned:
                raise RuntimeError("Hard CPU affinity unavailable; use --allow-unpinned for explicitly unqualified observations")
            return self
        cpu = min(allowed) if self.cpu == -1 else self.cpu
        if cpu not in allowed:
            self.original = None
            raise ValueError("Requested logical CPU is outside the allowed affinity")
        try:
            self._set((1 << cpu) if os.name == "nt" else {cpu})
            if os.name == "nt":
                current = ctypes.c_size_t()
                if not self.dll.GetProcessAffinityMask(self.dll.GetCurrentProcess(), ctypes.byref(current), ctypes.byref(system_mask)) or current.value != 1 << cpu:
                    raise RuntimeError("Affinity verification failed")
            elif os.sched_getaffinity(0) != {cpu}:
                raise RuntimeError("Affinity verification failed")
        except BaseException:
            self._set(self.original)
            raise
        self.info = dict(Supported=True, LogicalProcessor=cpu, Policy="controller pinned before child launch; inherited; restored on exit")
        return self

    def _set(self, value):
        if os.name == "nt":
            if not self.dll.SetProcessAffinityMask(self.dll.GetCurrentProcess(), value):
                raise ctypes.WinError(ctypes.get_last_error())
        else:
            os.sched_setaffinity(0, value)

    def __exit__(self, *_):
        if self.original is not None:
            self._set(self.original)


def environment():
    state = None
    error = None
    try:
        if os.name == "nt":
            state = subprocess.check_output(["powercfg", "/getactivescheme"], timeout=10).decode(errors="replace").strip()
        elif sys.platform.startswith("linux"):
            governors = sorted(Path("/sys/devices/system/cpu").glob("cpu[0-9]*/cpufreq/scaling_governor"))
            state = {str(p): p.read_text().strip() for p in governors} or None
    except (OSError, subprocess.SubprocessError) as exc:
        error = str(exc)
    return dict(OS=platform.platform(), Processor=platform.processor(), LogicalProcessors=os.cpu_count(),
                PowerState=state, PowerQueryError=error,
                Caveat="Endpoint policy snapshots only; no effective-frequency, temperature, SMT-isolation or per-core-load claim")


def system_times():
    if os.name == "nt":
        from ctypes.wintypes import FILETIME
        values = [FILETIME() for _ in range(3)]
        if not windows().GetSystemTimes(*(ctypes.byref(v) for v in values)):
            return None
        idle, kernel, user = [(v.dwHighDateTime << 32) | v.dwLowDateTime for v in values]
        return idle, kernel + user
    if sys.platform.startswith("linux"):
        values = [int(v) for v in Path("/proc/stat").read_text().splitlines()[0].split()[1:9]]
        return values[3] + values[4], sum(values)
    return None


def _worker_job():
    """Job belongs to this dedicated worker, not the user's host Python process."""
    from ctypes import wintypes as w

    class Basic(ctypes.Structure):
        _fields_ = [("process_time", ctypes.c_longlong), ("job_time", ctypes.c_longlong),
                    ("flags", w.DWORD), ("min_ws", ctypes.c_size_t), ("max_ws", ctypes.c_size_t),
                    ("active", w.DWORD), ("affinity", ctypes.c_size_t), ("priority", w.DWORD), ("schedule", w.DWORD)]

    class Extended(ctypes.Structure):
        _fields_ = [("basic", Basic), ("io", ctypes.c_ulonglong * 6),
                    ("process_mem", ctypes.c_size_t), ("job_mem", ctypes.c_size_t),
                    ("peak_process", ctypes.c_size_t), ("peak_job", ctypes.c_size_t)]

    dll = windows()
    limits = Extended()
    limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    job = dll.CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())
    if not dll.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)) or not dll.AssignProcessToJobObject(job, dll.GetCurrentProcess()):
        error = ctypes.get_last_error()
        dll.CloseHandle(job)
        raise ctypes.WinError(error)
    # Intentionally owned until OS process exit: closing it here kills this worker.
    return job


def _child_worker(argv, timeout):
    job = _worker_job() if os.name == "nt" else None
    before = system_times()
    if os.name != "nt":
        import resource
        usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.perf_counter()
    try:
        child = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        buffers = [bytearray(), bytearray()]

        def drain(stream, buffer):
            while chunk := stream.read1(65536):
                buffer.extend(chunk)

        readers = [threading.Thread(target=drain, args=(stream, buffer), daemon=True)
                   for stream, buffer in zip((child.stdout, child.stderr), buffers)]
        waiter = threading.Thread(target=child.wait, daemon=True)
        waiter.start()
        for reader in readers:
            reader.start()
        try:
            waiter.join(max(0, timeout - (time.perf_counter() - start)))
            if waiter.is_alive():
                raise subprocess.TimeoutExpired(argv, timeout)
            for reader in readers:
                reader.join(max(0, timeout - (time.perf_counter() - start)))
            if any(reader.is_alive() for reader in readers):
                raise subprocess.TimeoutExpired(argv, timeout)
        except subprocess.TimeoutExpired:
            row = dict(Status="timeout", ExitCode=None, WallSeconds=time.perf_counter() - start,
                       CpuSeconds=None, CpuToWall=None, SystemBusyFraction=None,
                       Output=bytes(buffers[0]).decode("utf-8", errors="replace").strip(),
                       Error=bytes(buffers[1]).decode("utf-8", errors="replace").strip(), Pid=child.pid)
            print(json.dumps(row), flush=True)
            if os.name == "nt":
                os._exit(0)  # OS closes the owning job; child tree cannot outlive us.
            os.killpg(os.getpgrp(), signal.SIGKILL)
            raise AssertionError("unreachable")
        else:
            out, err = map(bytes, buffers)
            child.stdout.close()
            child.stderr.close()
            wall = time.perf_counter() - start
            cpu = None
            if os.name == "nt":
                from ctypes.wintypes import FILETIME
                stamps = [FILETIME() for _ in range(4)]
                if windows().GetProcessTimes(int(child._handle), *(ctypes.byref(v) for v in stamps)):
                    cpu = sum((v.dwHighDateTime << 32) | v.dwLowDateTime for v in stamps[2:]) / 10_000_000
            else:
                usage = resource.getrusage(resource.RUSAGE_CHILDREN)
                cpu = usage.ru_utime + usage.ru_stime - usage_before.ru_utime - usage_before.ru_stime
            after = system_times()
            busy = None
            if before and after and after[1] > before[1]:
                busy = 1 - (after[0] - before[0]) / (after[1] - before[1])
            row = dict(Status="passed" if child.returncode == 0 else "failed", ExitCode=child.returncode,
                       WallSeconds=wall, CpuSeconds=cpu, CpuToWall=cpu / wall if cpu is not None else None,
                       SystemBusyFraction=busy, Output=out.decode("utf-8", errors="replace").strip(),
                       Error=err.decode("utf-8", errors="replace").strip(), Pid=child.pid)
    except OSError as exc:
        row = dict(Status="launch-failed", ExitCode=None, WallSeconds=time.perf_counter() - start,
                   CpuSeconds=None, CpuToWall=None, SystemBusyFraction=None, Output="", Error=str(exc))
    print(json.dumps(row), flush=True)
    # Keep the owning Windows job alive through result publication.
    del job


def run_process(argv, cwd, timeout):
    if timeout <= 0:
        raise ValueError("Require positive timeout")
    command = [sys.executable, str(Path(__file__).resolve()), "--child-worker", str(timeout), *map(str, argv)]
    start = time.perf_counter()
    with subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          start_new_session=os.name != "nt") as worker:
        try:
            out, err = worker.communicate(timeout=timeout + 10)
        except subprocess.TimeoutExpired:
            if os.name == "nt":
                worker.kill()  # OS closes worker's owning job, killing its children.
            else:
                os.killpg(worker.pid, signal.SIGKILL)
            worker.communicate()
            return dict(Status="timeout", ExitCode=worker.returncode, WallSeconds=time.perf_counter() - start,
                        CpuSeconds=None, CpuToWall=None, SystemBusyFraction=None, Output="", Error="Process timeout",
                        WallScope="timeout includes worker startup/termination; not workload throughput")
        if worker.returncode != 0 and not out:
            return dict(Status="worker-error", ExitCode=worker.returncode, WallSeconds=time.perf_counter() - start,
                        CpuSeconds=None, CpuToWall=None, SystemBusyFraction=None, Output=out.decode(errors="replace"),
                        Error=err.decode(errors="replace"))
        result = json.loads(out)
        result["WallScope"] = "child launch through output drain; excludes Python worker startup"
        return result


if __name__ == "__main__":
    if sys.argv[1:2] != ["--child-worker"]:
        raise SystemExit("Internal worker; import measurement helpers from a benchmark runner")
    _child_worker(sys.argv[3:], float(sys.argv[2]))
