# mad-stm32-tetris-logic

Tetris logic firmware for STM32F767ZI (Nucleo-144). Runs the game engine on the
board and streams state to the PC Hub over UART.

## Features

- **Two-player local Tetris** on one board (players 1 and 2, separate inputs).
- **Full engine**: 7 tetrominoes, counterclockwise rotation, collision checks,
  one next preview, once-per-piece hold slot.
- **Gameplay loop**: automatic gravity, piece lock, line clears, scoring, and
  spawn game over. Level starts at 1, +1 every 10 lines. Score 40/100/300/1200
  for 1/2/3/4 lines × level.
- **Soft drop**: hold to speed up gravity.
- **Hub link over UART**: packed board snapshots + stats streamed at 10 Hz.
- **Payment/RFID relay**: credit status (OK / no-card / denied, balance, spent)
  forwarded to the Hub.
- **Leaderboard & new-record** packets relayed to the Hub.

## Controls (polled GPIO, active-low + pull-up)

| Action | Player 1 | Player 2 |
|--------|----------|----------|
| Move left | PF13 | PE10 |
| Move right | PE9 | PE12 |
| Rotate CCW | PE11 | PE13 |
| Hold / skip wait | PF14 | PE14 |
| Soft drop | PF12 | PE15 |

Start/join: onboard USER button **PC13** (active-high). Movement, rotation, and
hold act on press edges; soft drop acts while held.

## Code layout (`stm-logic/Core/`)

- `gameplay/gameplay.c` — lobby, states (IDLE/WAIT/PLAY/RESULT), polled inputs,
  timers, snapshot streaming.
- `gameplay/tetris_engine.c` — board, piece actions, hold, scoring, gravity.
- `gameplay/tetris_pieces.c` — base shapes + box rotations.
- `gameplay/tetris_rng.c` — seeded xorshift32 piece selection.
- `utils/buttons.c` — GPIO input + debounce.
- `utils/player_id.c` — player 1/2 identity.
- `protocols/protocols.c` — UART packet encode (JOIN/STATE/ATTACK/GAMEOVER/
  SKIP_WAIT/CREDIT/LEADERBOARD/NEWRECORD) + packed-board transport.

## Build

Open `stm-logic/` in STM32CubeIDE, build, flash via ST-LINK. Refresh and
Clean/Build after adding source files. Hub link: **USART3 on PD8/PD9, 921600 8N1**
via ST-LINK Virtual COM Port. See [pinout](docs/PINOUT.md) and
[protocols](docs/PROTOCOLS.md).

## Desktop engine tests (no HAL)

```sh
gcc -std=c11 -Wall -Wextra -Werror -Istm-logic/Core/gameplay \
  tests/tetris_engine_test.c \
  stm-logic/Core/gameplay/tetris_engine.c \
  stm-logic/Core/gameplay/tetris_pieces.c \
  stm-logic/Core/gameplay/tetris_rng.c -o /tmp/tetris_engine_test
/tmp/tetris_engine_test
```

Covers shape geometry, boundaries, blocked rotations, snapshot isolation, line
clears/scoring, level persistence, hold, spawn game over, deterministic RNG, and
gravity speeds.
