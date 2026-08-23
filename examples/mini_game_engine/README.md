# MiniWio Game Engine: Sky Hopper

Sky Hopper is a small, playable Flappy-style game built on the MiniWio
embedding example:

```text
C++ engine library + Raylib + C++ main loop
                         |
                         | Wio public SDK
                         v
                  Wio game script DLL
```

The engine is not written in Wio. Window creation, input, bird and pipe
storage, collision detection, deterministic spawning, rendering, and the main
loop all live in C++. Wio owns the user-authored rules: gravity, flap impulse,
pipe speed, gap size, spawn cadence, score, best score, difficulty level, and
the color theme.

## Play

With Wio 0.14 installed, open this directory and run:

```powershell
wio project run
```

The project now opens the game window by default. Use any of:

- `Space`
- `Up Arrow`
- left mouse button

The same input starts a round, flaps, and restarts after a crash.

## Layout

- `engine/include/miniwio_engine.h`: engine-facing `GameScript` contract and
  public engine types.
- `engine/src/miniwio_engine.cpp`: C++ bird/pipe world, physics integration,
  collision, drawing, headless autopilot, and main loop.
- `host/main.cpp`: Wio SDK adapter and native executable entrypoint.
- `wio/game_script.wio`: the complete user-authored Sky Hopper behavior.
- `wio.makewio`: builds the Wio script DLL and C++/Raylib host.

The example reuses the Raylib headers and Windows x64 MinGW library shipped
with `examples/atlas_desk`, avoiding a duplicate binary in the repository.

## Script-owned rules

The Wio script exposes an exported `GameScript` object. Its methods decide:

- how gravity changes vertical velocity
- the velocity applied by a flap
- pipe scroll speed
- pipe gap and spawn interval
- scoring and best-score retention
- level-based difficulty scaling
- sky, bird, and pipe colors

For example, changing only these Wio fields noticeably changes the game:

```wio
self.gravity = 850.0f;
self.flapImpulse = -330.0f;
self.pipeGap = 245.0f;
self.pipeSpeed = 160.0f;
```

No Raylib object, native world pointer, or entity container crosses into Wio.
The SDK adapter translates the exported object into the engine's pure C++
`miniwio::GameScript` interface.

## Deterministic acceptance mode

The interactive game and automated test use the same engine. Headless mode
runs 720 deterministic frames with a small autopilot and a forced recovery
cycle:

```powershell
wio project run -- --headless
```

It exits non-zero unless pipes spawn and pass, collision/restart works, and the
Wio-owned best score is retained. The current reference result begins with:

```text
MiniWio Flappy: mode=headless frames=720
```

The adapter uses the SDK surface shared by Wio 0.12 through 0.14, while the
recommended installed toolchain for this example is Wio 0.14.0.
