# Wio 0.16 Acceptance Matrix

This matrix is the release gate for ownership, callbacks, async host tasks, and
application hosting. Every row runs on Windows and Ubuntu in the release
workflow.

| Scenario | Contract proved | Evidence |
|---|---|---|
| Console application | deterministic start/update/reverse close, real delta, orderly exit | `wio_test_application_lifecycle_run`, `wio_test_application_delta_context_run` |
| Resource-driven tool | explicit `ref`/`view` injection and deterministic stage dependencies | `wio_test_application_resource_schedule_run`, schedule diagnostics |
| Desktop/event loop | host-owned state, non-blocking frame poll, main-thread affinity and explicit main pump | `wio_test_sdk_016_application_host_interop` |
| Fixed game loop | monotonic host delta and deterministic fixed-step catch-up | `wio_test_sdk_016_fixed_application_host_interop` |
| Startup failure | partial-start rollback in executable and hosted applications | `wio_runtime_application_start_rollback`, `wio_test_sdk_016_application_rollback_interop` |
| Native async host | typed scalar task, polling/wait, deadline, cancellation, callbacks, stale binding | `wio_test_sdk_016_async_task_host_interop` |
| Native callback | retained userdata, typed invocation, foreign-thread entry and exception containment | `wio_sdk_016_callback_lifetime` |
| Native resource | move-only ownership, borrowed view, transfer and exactly-once release | `wio_sdk_016_native_resource` |
| Service/tool I/O | cancellation-aware filesystem, process, DNS, TCP and UDP surfaces | `wio_test_async_cancellable_io_surface_check` and existing async I/O runtime tests |
| Structured shutdown | parent-to-awaited-child cancellation and explicit runtime drain | `wio_test_async_cancellation_outcome_run`, async scope/runtime stress tests |

Release blockers:

- any implicit wait in `ApplicationHost::update` or task `poll`;
- a callback exception crossing the ABI;
- a copied owned native resource;
- a started system omitted from rollback or closed twice;
- an I/O cancellation surfacing as an unhandled coroutine failure;
- Windows/Ubuntu disagreement in the matrix above.
