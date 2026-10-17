# N03: preserve the host floating display contract

2026-10-17. N03 remains in progress; this is a correctness follow-up, not a new
speedup claim, N03 closure, or a transition to N04.

## Observed failure and fix

[CI run 38974779551](https://github.com/tynes0/wio/actions/runs/38974779551)
passed Windows Debug and Ubuntu Release. macOS Release compiled successfully,
but its host floating oracle and Wio formatting gate failed:

```text
value=-0x1.a349dp+20
host stream=-1.71740e+06
optimized output=-1.7174e+06
```

The numeric value is equivalent, but Wio must not change its existing display
text silently as an optimization. `AppendFloating` now conservatively retains
the stream fallback when compiled with libc++. The finite/classic/nearest
`to_chars(general, 6)` path remains enabled on the locally qualified MSVC and
libstdc++ hosts. This is a standard-library gate, not an OS-name check: Clang
with libstdc++ and Clang with libc++ are different qualification targets.

The counterexample is explicit in the host oracle as both f32 and f64, before
the 30,000-bit-pattern test for each type. Expected text comes from the actual
host stream, not a spelling hard-coded from Windows. The same shared helper
serves generated native formatting and VM builtin formatting. No numeric value,
precision, rounding mode, locale, bytecode opcode or public API changes.

The reserved builder allocation assertion now covers integer/bool values on
every host. Floating values still have correctness checks, but their stream
fallback is not advertised as allocation-free. This narrows an unsupported
cross-platform performance promise; it does not waive the float output oracle.

## Validation

The six new N03 tests passed locally in 131.76 seconds: the primitive fusion
proof, numeric allocation/locale unit, 60,000-value float oracle, VM formatter
unit, and both Wio native O0/O2 + VM gates. No old functional corpus was run.
The macOS failure evidence is retained separately from the passing local run:
[CI probe failure](evidence/n03-libcpp-failure-2026-10-17.json),
[CI test log](evidence/n03-libcpp-failure-2026-10-17.log),
[local six-test result](evidence/n03-portability-tests-2026-10-17.xml).

## Remaining work

libc++ floating performance needs its own measured, output-compatible fast path.
The fallback is deliberately not reported as equally fast to the qualified
`to_chars` path. Failed CI stays visible; local Windows results do not prove
macOS success. Shared user ToString lowering/VM invocation (KI-008), richer
formatting, and the legacy Format inventory remain open under N03. N01/N14
measurement qualification and the fixed N01-N14 numbering are unchanged.
