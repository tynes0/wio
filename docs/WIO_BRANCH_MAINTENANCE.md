# Branch Maintenance Record

Date: **2026-10-12**. Repository: `tynes0/wio`.

## Current policy and result

- Active development: `feature/backend-closure`, replacing
  `performance/compiler-pipeline`. The rename did not switch working trees,
  reset files, stash user edits or discard commits.
- Pushed all 66 previously unpushed development commits and the issue/code
  reference commit `2dba95d` to the new branch before removing any remote.
- Retain `main` and exactly the last five **published releases**, not merely
  the last five previously existing release branches:
  `releases/v0.18.1`, `releases/v0.18.0`, `releases/v0.17.0`,
  `releases/v0.16.0`, `releases/v0.15.0`.
- The missing v0.15/v0.16 branch refs were created at the existing verified
  release tag commits. No release/tag/artifact was deleted or retagged.
- Retired **14 remote** and **17 local** stale branch refs. The former local
  performance branch was renamed, not deleted before preservation.
- Final inventory: seven local branches and seven remote branches, excluding
  the symbolic `origin/HEAD`.
- Other retired tips are reachable through the retained development/main
  history. The local-only v0.13 example work is separately archived below.
- No merge, PR, release publication or full compiler qualification was performed.
  A feature-branch push does not automatically qualify release CI.
- Local `main` remains two commits behind `origin/main`; it was deliberately
  not reset/rewritten during this cleanup.
- User-owned playground/std files and the pre-existing staged Mandelbrot entry
  are preserved. Their hashes/index entries are checked separately.

## Unique local work preservation

Local `releases/v0.13.0` had four commits absent from the active/main history.
They were preserved using an annotated tag, pushed and checked on GitHub
**before** the old local ref was removed:

- Tag: `archive/branches/2026-10-12/releases-v0.13.0`
- Commit: `723850426c18499e71076d9435026b1419116b48`
- Preserved commits: `c89bf37` telemetry interop example, `871cbd2` C++ mini
  engine/Wio scripting, `60abfe3` installed SDK compatibility, `7238504`
  Flappy example.

Recover a local branch without changing the working tree:

```powershell
git fetch origin refs/tags/archive/branches/2026-10-12/releases-v0.13.0:refs/tags/archive/branches/2026-10-12/releases-v0.13.0
git branch recovered/v0.13-examples archive/branches/2026-10-12/releases-v0.13.0
```

Remote deletion used an atomic push with an explicit expected-SHA lease for
every retired ref. If another actor had changed a target, the push would have
failed instead of silently deleting newer work. Ordinary merged local refs used
`git branch -d`; the one unmerged local ref used explicit deletion only after
its remote archive tag and exact commit were checked.

## Retired ref inventory

SHA values identify the pre-cleanup tips. Local and remote tips differed for
some old release branches; the distinction is intentional.

| Branch | Local tip before deletion/rename | Remote tip before deletion |
| --- | --- | --- |
| `examples/wio-observatory` | `95dbeb751a8ed33f0f88b12f1886ba1f87ce95bb` | — |
| `feature/cpp-backend-cutover` | `1e8b05d6714d98775428df5881d676bada848122` | `1e8b05d6714d98775428df5881d676bada848122` |
| `feature/p0-correctness-sprint` | `d978da15cf51e8f36924e710eaaccf2db3ad9f63` | — |
| `feature/p1-b-lifecycle-safety` | `2a680bc5594046d3b6db0eec607e6f34dd506600` | — |
| `feature/p1-c-generics-type-system` | `34969f4026ea12ecd596f3b3f428a05317d90d39` | — |
| `feature/v1-language-coherence` | `409f7eb5633b144b8ca32718fe20fc5cf572a2d5` | `409f7eb5633b144b8ca32718fe20fc5cf572a2d5` |
| `feature/wir-pipeline` | `a49be596d911b5a63efce86679b495d859e3988a` | `a49be596d911b5a63efce86679b495d859e3988a` |
| `features/p1-language-std` | `ba38b307b7ebf5becdade186e052f564e657e933` | — |
| `performance/compiler-pipeline` | Renamed; original tip `147422fd694ea164bf350ce050d860ceee75bae4` | `e0695eee38dab69ee57810a48ccf4eee9b196cf3` |
| `planning/post-v0.12` | `fcc2704067489cabd2680f0a2cab1464a3515d59` | `fcc2704067489cabd2680f0a2cab1464a3515d59` |
| `releases/v0.10.0` | `7df266002cff2d7debce147c14885169bb5fbd50` | `7df266002cff2d7debce147c14885169bb5fbd50` |
| `releases/v0.11.0` | — | `ba38b307b7ebf5becdade186e052f564e657e933` |
| `releases/v0.11.1` | `1e2e245713f2d534a1730241a7ca65a0426a6221` | `1e2e245713f2d534a1730241a7ca65a0426a6221` |
| `releases/v0.12.0` | `2555035cc06c8e33b202f4248cd41bcbec242292` | `2555035cc06c8e33b202f4248cd41bcbec242292` |
| `releases/v0.13.0` | `723850426c18499e71076d9435026b1419116b48` | `6373c24fe3cc3d8c09a79e6d35fe26a64c8172e2` |
| `releases/v0.14.0` | `098b95dd6259671ce3642741c58f6763bb3e0ea2` | `098b95dd6259671ce3642741c58f6763bb3e0ea2` |
| `releases/v0.7.0` | `72fd65191d551dcb4c5b5ecceef13aaac9510538` | `72fd65191d551dcb4c5b5ecceef13aaac9510538` |
| `releases/v0.8.0` | `2a680bc5594046d3b6db0eec607e6f34dd506600` | `2a680bc5594046d3b6db0eec607e6f34dd506600` |
| `releases/v0.9.0` | `31d0ddbbf18905b3207673028be8490d0ed9aa49` | `34969f4026ea12ecd596f3b3f428a05317d90d39` |

To recover another retired branch whose tip is retained, use
`git branch recovered/NAME SHA`. Published version tags remain available for
release reproduction; no old release packages were removed.
