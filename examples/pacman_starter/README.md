# Pacman rendering starter

A Pacman-style scene drawn in Wio with raylib. The maze, pellets, power pellets,
cherries, animated player and ghosts, score/lives/level HUD, and ready/pause/win/
game-over overlays are ready. **Gameplay is intentionally not implemented.**
Movement, collisions, pellet consumption, score rules, timers and ghost AI belong
to your code in `wio/logic.wio`.

![Pacman rendering starter](assets/preview.png)

## Run

From this directory:

```powershell
wio project run
```

Or from the repository root:

```powershell
wio project run --project examples/pacman_starter
```

This manifest uses the existing Windows x64 MinGW raylib headers/library in
`../atlas_desk/native`. No download or duplicated library is needed. Keep the
two example directories in their current relative locations. Raylib's license
notice is in `../atlas_desk/LICENSES/raylib-zlib.txt`. Other platforms need their
own raylib build and platform libraries in the manifest.

The drawing uses raylib's built-in pixel font and procedural shapes, without
image, audio or font downloads. The window is resizable;
the maze and side panel adapt down to 800 by 780 pixels.

## Connect your logic

`logic.Start(state)` runs once. `logic.Update(state, input, delta)` runs every
frame, before drawing. `delta` is seconds, capped at 0.05. `GameState` is an
object, so assignments made through that parameter update the rendered scene.

```wio
fn Update(state: GameState, input: InputFrame, delta: f32) {
    // A minimal example only; add your own wall checks and movement rules.
    if (input.actionPressed) state.phase = Phase::playing;
    if (input.right) {
        state.player.direction = Direction::right;
        state.player.position.x += 3.0f32 * delta;
    }

}
```

The renderer reads state without moving actors or mutating gameplay. It owns
only visual effects: mouth animation, ghost bob/skirt animation and power-pellet
pulses. Fractional positions work, so your logic can interpolate smoothly.

| State | Meaning |
| --- | --- |
| `player.position.x/y` | Tile coordinates; `(0, 0)` is the center of the top-left tile, not pixels |
| `player.direction` | `right`, `down`, `left`, `up`, `none` |
| `player.visible` | Whether to draw the player |
| `ghosts[index].actor` | Position, direction and visibility of a ghost |
| `ghosts[index].mode` | `normal`, `frightened` or `eyes` |
| `frightenedFlash` | Flash frightened ghosts between blue and white |
| `score`, `bestScore`, `lives`, `level` | HUD values; the renderer does not calculate them |
| `phase` | `ready`, `playing`, `paused`, `won`, `gameOver` |
| `fruitVisible`, `fruitPosition` | Cherry visibility and tile position |
| `TileAt(x, y)` | Read a tile; out-of-bounds returns `Tile::outside` |
| `SetTile(x, y, value)` | Change a tile; false if out of bounds |
| `Reset()` | Restore the initial scene; call it from your reset logic if wanted |

The map is 28 columns by 31 rows, stored row-major in `tiles`. Layout symbols
in `model.wio` are `#` wall, `.` pellet, `o` power pellet, `-` pen door, `_` empty
floor and space outside the maze. Doors and tunnel wrapping have no built-in
collision behavior; decide those rules in your logic.

## Inputs and visual previews

| Key | Behavior |
| --- | --- |
| Arrow keys or WASD | Held flags in `input.up/down/left/right`; also illuminate the input panel |
| Space or Enter | One-frame `input.actionPressed` |
| P | One-frame `input.pausePressed` |
| R | One-frame `input.resetPressed`; the shell does not reset for you |
| F1 | Toggle tile grid |
| F2 | Preview normal/frightened/eyes graphics without changing actual ghost modes |
| F3 | Preview overlays; cycle back to actual gameplay state |
| F12 | Save `.wio-build/pacman-preview.png` |
| Escape | Close the window |

Preview controls belong to the shell's `Presentation`, not `GameState`. The
window therefore stays on the initial scene until you implement logic. Arrow
keys do not secretly move the player, and Space/P/R have no preset game rules.

## Files

- `wio/logic.wio`: your gameplay hook; start here.
- `wio/model.wio`: editable tile map and public scene data.
- `wio/renderer.wio`: all maze, character, HUD and overlay drawing.
- `wio/raylib.wio`: typed native Color/Vector2/Rectangle and raylib declarations.
- `wio/main.wio`: window/input/frame loop and visual-preview controls.
- `native/include/pacman_raylib.h`: small C-string and pixel-position adapters;
  no gameplay or scene renderer lives in C++.

## Check the initial scene

```powershell
wio project run -- --check
wio project run --no-build -- --smoke-modes
wio project run --no-build -- --smoke-small
```

`--check` validates the initial 28 by 31 scene, 240 pellets, four power pellets,
four ghosts and bounded tile writes without opening a window. `--smoke-modes`
renders seven screenshots to `.wio-build`, previews every ghost mode and overlay,
checks the initial state is unchanged, then exits. `--smoke-small` captures the
minimum window size. Adjust these initial-state checks once your own gameplay
deliberately changes the scene.
