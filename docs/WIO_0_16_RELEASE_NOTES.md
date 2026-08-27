# Wio 0.16.0 Release Notes

Wio 0.16 freezes the boundary needed by real native applications: who owns
state, how a host drives an application, how async results arrive later, how
cancellation propagates, and how callbacks/native resources cross C++ safely.

## Application runtime

- `application` update handlers receive a monotonic `f64` frame delta.
- `schedule` supports deterministic dependency ordering and fixed stages with
  `at N hz` accumulation.
- `resource` values can be passed explicitly into scheduled system updates as
  `ref self.name`; handler `ref`/`view` types state access intent.
- Start failure rolls back only successfully started systems in reverse order,
  calls application close, and leaves hosted state terminally closed.
- Generated executables and C++ hosts share explicit main-executor drain and
  async-runtime shutdown boundaries.

```wio
application Editor {
    resource documents: DocumentStore;
    system autosave: Autosave;

    schedule {
        fixed stage maintenance at 2hz on main {
            run autosave.update(ref self.documents);
        }
        stage ui after maintenance { run self.update; }
    }

    on update(delta: f64) {
        if (ShouldExit()) { self.Exit(0); }
    }
}
```

## Native host SDK and ABI v10

- `Module::application()` returns a move-only host state with start, frame
  update, exit request, close, status, error, and main-pump operations.
- Stable scalar `[Export] async fn` entries load as typed `AsyncTask<T>` calls.
  Polling never blocks; wait/deadline APIs are explicit; completion callbacks
  choose current or main delivery.
- Task and application bindings reject stale module generations, and their
  library leases prevent unloaded-code calls while owned work remains.
- `WioHostCallback` adds typed signatures, retained userdata, thread-safety
  declaration, and ABI exception containment.
- `WioOwnedNativeResource` plus `UniqueNativeResource` separates move-only
  ownership from copyable borrowed native views and guarantees exactly-once
  release.

## Async and portable I/O

- Cancellation propagates from a coroutine into the task it is currently
  awaiting, including cancellation-aware sleep/timer boundaries.
- `TryWithCancellation` returns `Option::None`/`false` for recoverable
  cancellation.
- Filesystem, process, DNS, TCP, listener, and UDP async APIs accept
  `CancellationToken` overloads and return cancellation through `Result`.
- Portable blocking syscalls remain cooperative; leases keep resources alive
  and abandoned results are drained safely.

## Compatibility

The module descriptor advances to version 10. Existing descriptor fields and
capabilities retain their prior meaning. The new application and async host
tables are capability-gated. Legacy attribute source may still be migrated by
the 0.15 tooling; maintained examples use bracket attributes.

Windows MinGW builds automatically use PE/COFF big-object mode for generated
Wio translation units, so large applications and static libraries do not hit
the classic section-count limit.

The normative language delta is
[`spec/WIO_LANGUAGE_SPEC_0_16.md`](./spec/WIO_LANGUAGE_SPEC_0_16.md). The exact
cross-platform evidence is [`WIO_0_16_ACCEPTANCE.md`](./WIO_0_16_ACCEPTANCE.md).
