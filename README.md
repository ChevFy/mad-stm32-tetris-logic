# mad-stm32-tetris-logic

Tetris logic firmware for STM32F767ZI (Nucleo-144).

## Single-player game

- Seven tetrominoes, counterclockwise rotation, and collision checks without kicks.
- Independent random pieces, one next preview, and a once-per-piece hold slot.
- Automatic gravity, piece locking, line clears, scoring, and spawn game over.
- Start at level 1; increase every 10 cleared lines. Score 40/100/300/1200 for
  1/2/3/4 lines, multiplied by the level before the clear.
- Level-based gravity uses a classic-style frame table (about 60.1 frames/sec).
- Board snapshots include the falling piece and stream stats to the PC Hub at 10 Hz.

### Controls (polled)

| Pin | Action |
|-----|--------|
| PC13, blue USER button | Join/start from idle |
| PF13 | Move left |
| PE9 | Move right |
| PE11 | Rotate counterclockwise |
| PF14 | Hold while playing; skip waiting in the lobby |

The four external buttons connect their inputs to GND and use internal pull-ups.
The onboard USER button is active-high. Movement, rotation, and hold act on press
edges; there is no soft/hard drop or held-button movement repeat.

The existing lobby starts locally after 10 seconds or PF14. UART receive handling
for Hub START/ATTACK/RESULT is not implemented, so local start and the Hub countdown
are not synchronized. Wait at least 0.5 seconds after joining before skipping.

## Code layout

- `Core/gameplay/gameplay.c`: lobby, polled inputs, timers, and UART snapshots.
- `Core/gameplay/tetris_engine.c`: board, piece actions, hold, scoring, and gravity.
- `Core/gameplay/tetris_pieces.c`: base shapes and box-based rotations.
- `Core/gameplay/tetris_rng.c`: seeded xorshift32 piece selection.
- `Core/utils/buttons.c`: GPIO input and debounce.
- `Core/protocols/`: UART packet encoding and packed-board transport.

Paths above are relative to `stm-logic/`.

## Build

Open `stm-logic/` in STM32CubeIDE, build, flash via ST-LINK.

Refresh and Clean/Build after adding source files so CubeIDE regenerates the
source lists. The Hub link uses USART3 on PD8/PD9 at **921600 8N1**, available
through ST-LINK's Virtual COM Port. See [pinout](docs/PINOUT.md) and
[protocol helpers](docs/PROTOCOLS.md).

### Testing from a fresh checkout

1. Check out `feat/game-engine` and import `stm-logic/` using CubeIDE's
   **Existing Projects into Workspace** import. Use the committed `.project` and
   `.cproject`; generated `Debug/` files are not needed.
2. Refresh, Clean, and Build the project, then flash with ST-LINK. Create a local
   debug launch configuration when prompted. CubeMX regeneration is optional;
   the generated firmware, HAL/CMSIS files, and linker scripts are included.
3. In the companion `mad-tetris-python-uart-simulation/tools` directory, install
   `requirements.txt` into a Python 3.12 environment and run
   `python hub_sim.py --port COM3` (replace `COM3` with the board's serial port).
4. Press the blue USER button to join. Wait for solo start, or press PF14 after
   at least 0.5 seconds to skip waiting. Allow the PC countdown to finish.
5. Check left/right, rotation, one-use hold, automatic locking, line clears,
   score/level progression, and game over. The simulator's `D` key shows packet
   statistics; confirm that STATE packets arrive and CRC errors remain zero.

## Desktop engine checks

The engine has no HAL dependencies. From the repository root with GCC installed:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Istm-logic/Core/gameplay tests/tetris_engine_test.c stm-logic/Core/gameplay/tetris_engine.c stm-logic/Core/gameplay/tetris_pieces.c stm-logic/Core/gameplay/tetris_rng.c -o /tmp/tetris_engine_test
/tmp/tetris_engine_test
```

Checks cover shape geometry, boundaries, blocked rotations, snapshot isolation,
line clears/scoring, level persistence across spawn, hold, spawn game over,
deterministic random sequences, and gravity speeds.
