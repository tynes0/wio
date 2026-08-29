# Wio 0.17 Application Model

Wio 0.17 removes the small language-inside-the-language feeling from
applications. State is written as fields, behavior is written as functions,
and lifecycle/scheduling intent is attached through typed attributes.

```wio
system Simulation {
    ticks: u64;

    fn Update(delta: f64) {
        if delta >= 0.0 { self.ticks += 1u64; }
    }
}

application DesktopApp {
    simulation: Simulation;
    fixedTicks: u64;

    [Start]
    fn Load() { }

    [Fixed(60)]
    fn Physics(step: f64) {
        self.fixedTicks += 1u64;
    }

    [After(Physics)]
    [Main]
    fn Draw(delta: f64) { }

    [Update]
    fn Frame(delta: f64) {
        if ShouldClose() { self.Exit(0); }
    }

    [Close]
    fn Save() { }
}
```

The compiler normalizes this to stack-resident components and generated
extensions. There is still one host-visible application descriptor and one
deterministic start/update/close state machine; the change is source-level
coherence, not a second runtime.

Lifecycle and scheduling attributes are composable. A project can name a
reusable policy once and keep application bodies focused on domain behavior:

```wio
attribute FrameStep(rate: f64) for handler
    compose [Update, Fixed(rate), Main];

application Simulation {
    [FrameStep(60.0)]
    fn Advance(step: f64) {
        // Update lifecycle, 60 Hz fixed stage, main-thread-affine.
    }
}
```

Compiler-consumed compositions currently need to be declared before the
application or system that uses them in the same source module. General
attribute composition remains semantic and module-aware; moving application
lowering fully into that phase is the path to imported scheduling policies.

## Migration map

| v0.16 compatibility spelling | v0.17 canonical spelling |
| --- | --- |
| `mut value: T;` | `value: T;` (`mut` remains valid) |
| `resource world: World;` | `world: World;` |
| `system physics: Physics;` | `physics: Physics;` |
| `on start { ... }` | `fn Start() { ... }` or `[Start] fn Name()` |
| `on update(delta: f64) { ... }` | `fn Update(delta: f64)` or `[Update] fn Name(delta: f64)` |
| `on close { ... }` | `fn Close()` or `[Close] fn Name()` |
| `fixed stage physics at 60 hz` | `[Fixed(60)] fn Physics(step: f64)` |
| `stage render after physics on main` | `[After(Physics)] [Main] fn Render(delta: f64)` |

The old forms remain accepted in 0.17. Explicit resource arguments in a legacy
`run system.update(ref self.resource)` should remain legacy until the typed
resource-access attribute model lands; mechanically deleting that schedule can
change behavior.

The conservative migration command rewrites lifecycle handlers and owned
field declarations while deliberately preserving explicit schedules:

```powershell
wio migrate applications . --check
wio migrate applications . --write
```

## Deliberate limits

- `[Worker]` produces a focused diagnostic instead of pretending application
  borrows are thread-safe.
- The first implementation recognizes source-module system types directly.
  Imported system identity will move into semantic/module metadata.
- User-composed lifecycle/schedule policies are recognized from declarations
  earlier in the same source module. Imported policies require semantic
  application lowering.
- `[Main]` is meaningful metadata but performs no dispatch in the sequential,
  already-main-thread-affine runner.
- Fixed stages retain the 0.16 accumulator behavior. Catch-up limits and
  scheduler introspection are separate hardening work.
