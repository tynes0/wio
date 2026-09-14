# Wio 0.17.0 Release Notes

Wio 0.17 makes the application model read like the rest of the language.
Applications and systems now use ordinary fields and functions; lifecycle and
scheduler intent are typed attributes rather than a second collection of
member-only keywords.

## Ordinary application and system bodies

Fields are mutable and default-initialized unless stated otherwise. `Start`,
`Update`, and `Close` are conventional lifecycle names. Descriptive names bind
the same roles with `[Start]`, `[Update]`, and `[Close]`.

```wio
system Simulation {
    ticks: u64;

    fn Update(delta: f64) {
        if (delta >= 0.0) { self.ticks += 1u64; }
    }
}

application DesktopApp {
    simulation: Simulation;

    [Start]
    fn Load() { }

    fn Update(delta: f64) {
        if ShouldClose() { self.Exit(0); }
    }

    [Close]
    fn Save() { }
}
```

Application and system helpers remain synchronous because their implicit
receiver is a mutable borrow of stack-resident state. They may start async work
explicitly and observe its task from later frames without blocking the host
update loop.

## Attribute-driven scheduling

- `[Fixed(hz)]` adds a positive-frequency fixed stage.
- `[After(Stage)]` adds deterministic dependency ordering.
- `[Main]` records main-thread affinity.
- `[Worker]` is reserved with a focused diagnostic until ref/view conflict
  analysis can prove safe transfer.

The policies are composable, including argument forwarding:

```wio
attribute FrameStep(rate: f64) for handler
    compose [Update, Fixed(rate), Main];

application Tool {
    [FrameStep(60.0)]
    fn Advance(step: f64) { }
}
```

## SDK and ABI v11

The module descriptor advances to version 11 and adds the capability-gated
`WIO_MODULE_CAP_APPLICATION_SCHEDULE_V1` table. C++ hosts inspect normalized
stage names, dependencies, frequency, execution order, and stage flags through
`ApplicationHost::stages()` without taking scheduler ownership.

```cpp
for (const WioApplicationStageDescriptor& stage : application.stages()) {
    std::cout << stage.name << " @ " << stage.order << '\n';
}
```

## Compatibility and migration

The v0.16 `on start/update/close`, `resource`, member `system`, and explicit
`schedule` forms remain accepted. New code should use ordinary members and
attributes. Legacy explicit schedules remain necessary when a system update
receives explicitly injected `ref`/`view` resources.

`wio migrate applications PATH --check|--write` performs the safe lifecycle
and field rewrites while preserving comments, strings, and explicit schedules.

## Lowered WIR C++ backend cutover

File and project builds now use the independent Lowered-WIR C++ backend by
default. The compiler freezes overloads, generic specializations, ownership,
native ABI adapters, SDK exports, application scheduling, reflection, and
behavioral attribute hooks before C++ emission; the backend does not inspect the
AST or repeat semantic decisions.

Behavioral pre/post/finally/around attributes execute through canonical
processor type and hook function identities, including typed receiver guards,
result hooks, async results, exactly-once finalization, and escape/call-count
checks for `proceed`. Windows and Ubuntu release gates compare exit status,
stdout, and stderr with the former generator across focused source and clean
project matrices.

`--cpp-backend legacy` remains available for this release line as an explicit
rollback and differential oracle. WIR never falls back to it automatically.

Automatic owned-system and compiler-consumed composed-attribute discovery are
currently source-order local. Import-aware and forward discovery require
application lowering to move into semantic analysis and remain tracked before
v1.

The normative delta is
[`spec/WIO_LANGUAGE_SPEC_0_17.md`](./spec/WIO_LANGUAGE_SPEC_0_17.md), the
migration guide is
[`WIO_0_17_APPLICATION_MODEL.md`](./WIO_0_17_APPLICATION_MODEL.md), and the
cross-platform gate is
[`WIO_0_17_ACCEPTANCE.md`](./WIO_0_17_ACCEPTANCE.md).
