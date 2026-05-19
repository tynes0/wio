# Wio Source Tools

This folder is the first step toward moving workflow helpers out of PowerShell
and into Wio itself.

Run these tools through the repo-local CLI:

```powershell
build\app\Debug\wio.exe file run .\scripts\wio\print_file.wio
build\app\Debug\wio.exe file run .\scripts\wio\line_count.wio
build\app\Debug\wio.exe file run .\scripts\wio\run_host_interop.wio -- --help
build\app\Debug\wio.exe file run .\scripts\wio\run_hybrid_arena_demo.wio -- --help
```

Current intent:

- keep core orchestration commands in the native CLI (`build`, `test`, `project`, `bind`, `package`)
- move smaller interactive or workflow-oriented helpers into source-based Wio tools
- use this folder as the staging area for future demo, smoke, and developer helper scripts
- keep PowerShell wrappers as thin compatibility launchers while the real workflow lives here

For `v1`, the intended split is:

- core lifecycle, packaging, project, and binding orchestration stays in the
  native CLI,
- source-based Wio tools live here when the helper is best expressed as Wio
  code,
- and `scripts/*.ps1` remain compatibility wrappers rather than the primary
  public interface.
