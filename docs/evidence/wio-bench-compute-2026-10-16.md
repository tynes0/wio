# Wio benchmark example: integer recurrence

All raw samples, warmups and failures are retained in results.json. No trimming or retries.

| Phase | Case | Metric | Median ns | CPU ms | Peak RSS KiB | Qualified |
| --- | --- | --- | ---: | ---: | ---: | --- |
| comparison | wio-native | kernel | 60100 | 15.625 | 9244 | no |
| comparison | wio-vm | kernel | 1.27815e+07 | 78.125 | 21076 | no |
| comparison | cpp | kernel | 60900 | 0 | 5192 | no |
| comparison | csharp | kernel | 59800 | 46.875 | 23824 | no |
| comparison | python | kernel | 1.5992e+06 | 78.125 | 25868 | no |
| final | wio-native | kernel | 60100 | 15.625 | 9248 | no |
| final | wio-vm | kernel | 1.29558e+07 | 62.5 | 21076 | no |

Kernel time excludes process startup; wall time includes startup and polling.
Child CPU/peak RSS cover the root process; allocation diagnostics are separate and optional.
Approximate mean 95% intervals are descriptive normal intervals, not a universal accuracy guarantee.

Outputs correct: yes; overall timing qualified: no.
