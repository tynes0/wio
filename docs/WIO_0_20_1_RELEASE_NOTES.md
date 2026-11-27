# Wio 0.20.1 Release Notes

Wio 0.20.1 separates normal VM application execution from deliberately bounded
embedding and test execution. Long-running applications no longer stop merely
because they exhaust the previous default instruction budget.

## Application execution

`wio-vm MODULE.wiob` and VM executable projects now default to unlimited
instructions. Select an explicit finite budget when needed:

```sh
wio-vm game.wiob --instruction-limit 50000000
wio project run --backend vm --vm-instruction-limit 50000000
```

Project build, run and package accept `--vm-instruction-limit`. The option
overrides the manifest setting below; zero means unlimited, not zero work:

```toml
[vm]
instructionLimit = 0
```

Values must contain only decimal digits and fit an unsigned 64-bit integer.
Negative, signed, fractional, overflowing and repeated command-line values are
rejected. The project override requires `--backend vm`. Finite manifest budgets
apply to executables, not libraries whose host owns execution policy.

## Packaged application policy

The selected budget is embedded in the VM executable and survives project
packaging and `wio-vm --deploy`. A policy-only CLI override repackages an existing
build without recompiling an otherwise up-to-date native bridge. `--no-build`
rejects an image whose embedded policy differs from the requested policy.

```sh
wio-vm --package game.wiob --output game --instruction-limit 50000000
wio-vm --verify game --expect-instruction-limit 50000000
```

A packaged application's arguments remain application data. Passing
`--instruction-limit` to that application does not override its embedded policy.
For a raw bytecode run, put application arguments after `--`.

Finite budgets use executable envelope version 3, which includes the budget
and the dependency layout version. The new reader still accepts existing
version 1 and 2 envelopes. Bytecode format and module ABI descriptor 11 are
unchanged. Already-built older executables retain their original runner and
behavior until rebuilt; older runners reject version 3 envelopes.

## Embedding and budget scope

Direct C++ embedding retains the finite `MachineOptions::instructionLimit`
default of 10,000,000. A trusted host can explicitly set it to zero:

```cpp
wio::vm::MachineOptions options;
options.instructionLimit = 0;
wio::vm::Machine machine{module, options};
```

The budget counts bytecode instructions in one execution state, including
synchronous nested calls, loop iterations and coroutine suspension/resumption.
Resuming on another executor does not replenish it. Independent invocations,
separately executed child tasks and foreign callback entries have their own
execution states; this is not a machine-wide, process-wide or per-frame quota.
At most the selected number of instructions execute before WVM1008 is raised.
The diagnostic includes the configured limit and exhausted function location.

Budget exhaustion unwinds owned values. Existing cancellation, debugger abort,
call-depth and register limits remain active when the instruction budget is
disabled. A bytecode budget is not a wall-clock timeout or an untrusted-code
sandbox: it cannot interrupt a native call that does not return.

## Scope and remaining work

This patch does not change the native bridge build architecture or optimize
VM execution. Bytecode-first project builds, compiler-stage separation and the
remaining 0.20.x usability work stay separate. SDK/editor/documentation product
versions remain aligned; this does not close the independent v1 SDK feature gate.

See [CLI reference](WIO_CLI_REFERENCE.md), [release plan](WIO_V1_RELEASE_PLAN.md)
and [0.20.0 notes](WIO_0_20_RELEASE_NOTES.md).
