# MiniWio Game Engine

MiniWio demonstrates an engine-first embedding model:

```text
C++ engine library + C++ main loop
                  |
                  | Wio public SDK
                  v
          user-authored Wio script DLL
```

The engine is not written in Wio. Raylib windowing/rendering, input polling,
entity storage, collision detection, deterministic spawning, and the main loop
all live in C++. Wio owns the game-specific behavior: movement speed, enemy
steering, spawn cadence, waves, score, title, and visual theme.

## Layout

- `engine/include/miniwio_engine.h`: engine-facing script interface and public
  engine types.
- `engine/src/miniwio_engine.cpp`: world/entity system, collision, rendering,
  input, deterministic headless simulation, and main loop.
- `host/main.cpp`: Wio SDK adapter and native executable entrypoint.
- `wio/game_script.wio`: the complete user game script.
- `wio.makewio`: builds the Wio script as a shared module and the C++ engine as
  the host executable.

The example reuses the Raylib headers and Windows x64 MinGW library already
shipped with `examples/atlas_desk`, avoiding a duplicate 3 MB binary in the
repository.

## Automated headless run

From the Wio repository root:

```powershell
$env:WIO_ROOT = (Get-Location).Path
build\app\Debug\wio.exe project run --project .\examples\mini_game_engine
```

The manifest selects `--headless`, runs exactly 240 deterministic frames, and
fails if spawning, collision, Wio-driven scoring, or waves stop working.

## Windowed run

Build once, then launch the host directly so `--windowed` replaces the
manifest's acceptance-test argument:

```powershell
$env:WIO_ROOT = (Get-Location).Path
build\app\Debug\wio.exe project build --project .\examples\mini_game_engine
examples\mini_game_engine\.wio-build\interop\miniwio_engine.exe `
  examples\mini_game_engine\.wio-build\interop\miniwio_game.dll --windowed
```

Use WASD or the arrow keys. Closing the window stops the C++ main loop and
unloads the Wio script module through the SDK.

## Boundary rule

The engine library knows only the C++ `miniwio::GameScript` interface. The Wio
SDK is isolated in the host adapter, and the Wio script never receives a raw
Raylib object, native world pointer, or entity container. That gives the demo a
clear ownership boundary and makes a future reloadable script adapter possible
without rewriting the engine.

The adapter deliberately uses the SDK surface shared by Wio 0.12 and 0.13.
Newer optional inspection/capability helpers are not required to run the game;
the exported object field and method tables still validate the script contract.
