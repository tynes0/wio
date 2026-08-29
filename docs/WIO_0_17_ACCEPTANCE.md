# Wio 0.17 Acceptance Matrix

This matrix is the release gate for the ordinary application/system member
model, attribute-driven lifecycle and scheduling, and host-visible schedule
metadata. Every row is expected to run on Windows and Ubuntu in the release
workflow; ABI rows additionally exercise an independently compiled C++ host.

| Scenario | Contract proved | Evidence |
|---|---|---|
| Canonical application | ordinary default-initialized fields, conventional and attributed lifecycle functions, fixed and ordered stages | `wio_test_application_attribute_model_run` |
| Mixed-source migration | v0.16 `on` handlers and v0.17 functions share one lifecycle and deterministic close order | `wio_test_application_mixed_model_run` |
| Project scheduling policy | a user attribute composes `Update`, `Fixed`, and `Main`, including argument substitution | `wio_test_application_composed_attribute_run` |
| Host inspection | ABI v11 stage descriptors agree with normalized Wio schedule order and flags | `wio_test_sdk_017_application_schedule_interop` |
| Lifecycle diagnostics | conflicting roles, invalid parameters, non-unit results, and invalid system delta types fail at compile time | `wio_invalid_application_attribute_multiple_lifecycle`, `wio_invalid_application_attribute_start_parameter`, `wio_invalid_application_attribute_update_return`, `wio_invalid_system_attribute_update_type` |
| Schedule diagnostics | worker affinity, invalid fixed frequency, unknown dependency, and system-level schedule misuse fail at compile time | `wio_invalid_application_attribute_worker_rejected`, `wio_invalid_application_attribute_fixed_frequency`, `wio_invalid_application_attribute_unknown_dependency`, `wio_invalid_system_application_schedule_attribute` |
| Suspension boundary | application and system stack receivers cannot cross async suspension | `wio_invalid_application_async_helper`, `wio_invalid_system_async_helper` |

Release blockers:

- a composed lifecycle/schedule attribute producing different behavior from
  the equivalent direct attributes;
- an application lifecycle function running more than once per intended stage;
- a host stage table disagreeing with the executable scheduler;
- `[Worker]` dispatching before ref/view conflict analysis is implemented;
- legacy and canonical lifecycle declarations producing different startup,
  update, rollback, or close ordering;
- Windows and Ubuntu disagreement in the matrix above.

Known 0.17 boundaries, not hidden release claims:

- automatic owned-system recognition requires the system declaration to appear
  earlier in the same source module;
- compiler-consumed attribute compositions require their declaration to appear
  earlier in the same source module;
- `[Main]` is metadata/no-op dispatch in the sequential main-thread runner;
- `[Worker]` remains intentionally reserved.
