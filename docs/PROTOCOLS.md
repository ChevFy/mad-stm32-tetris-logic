# protocols/ — usage

Send-side helpers for the STM32 Logic → Hub UART link. Transmits on **USART3**
(`huart3`, PD8, 921600 8N1). Wire format spec: see the UART protocol doc.

```c
#include "protocols.h"
```

All send functions return `int`:

| Code             | Meaning                     |
|------------------|-----------------------------|
| `PROTO_OK` (0)   | sent                        |
| `PROTO_ERR_ARG`  | bad argument (len>112 / NULL) |
| `PROTO_ERR_TX`   | `HAL_UART_Transmit` failed  |

## Typed senders

### JOIN — enter the lobby
```c
protocols_send_join(1, 0, NULL, 0);   // player 1, no card, no name, 0 points
```
Call every 0.5 s until a START arrives from the Hub.
`name` may be `NULL` (sends 16 zero bytes).

### STATE — stream the board while playing
```c
protocols_state_t st = {0};
st.player = 1;
st.next   = 3;          // next piece 1..7
st.hold   = 0;          // 0 = none
st.score  = 1250;
st.lines  = 4;
st.level  = 2;
st.garbage_wait = 0;

uint8_t cells[200];                      // one colour code per cell (0..8)
/* ... fill cells from your board ... */
protocols_pack_board(cells, st.board);   // 200 cells -> 100 packed bytes
protocols_send_state(&st);               // seq auto-increments each call
```
`seq` is managed internally — do not set it. Send continuously (every frame).

### ATTACK — send garbage rows
```c
protocols_send_attack(2);   // 2 garbage rows
```

### GAMEOVER — pieces overflowed
```c
protocols_send_gameover();  // empty payload, send once
```

### SKIP_WAIT — start solo, skip waiting for player 2
```c
protocols_send_skip_wait(); // send once, >=0.5 s after first JOIN
```

## Helpers

### protocols_pack_board
```c
void protocols_pack_board(const uint8_t cells[200], uint8_t board[100]);
```
Packs 200 colour cells (index `i = y*10 + x`, row 0 top) into 100 bytes,
2 cells/byte. Even index → high nibble, odd → low nibble.
Colours: `0` empty, `1`=I `2`=O `3`=T `4`=S `5`=Z `6`=J `7`=L, `8`=garbage.

### protocols_crc8
```c
uint8_t protocols_crc8(const uint8_t *data, uint32_t len);
```
CRC-8/SMBUS (poly 0x07, init 0x00). Sanity check: `protocols_crc8("123456789",9) == 0xF4`.
You normally don't call this — `protocols_hub_send` does it for you.

### protocols_hub_send — low-level framer
```c
int protocols_hub_send(uint8_t type, const uint8_t *payload, uint8_t len);
```
Builds `0xAA | TYPE | LEN | PAYLOAD | CRC8` and transmits. The typed senders
above wrap this; use it directly only for a raw/custom packet.

## Typical game flow
```c
// lobby
do { protocols_send_join(1,0,NULL,0); HAL_Delay(500); } while (!start_received);

// playing
while (!game_over) {
    protocols_pack_board(cells, st.board);
    protocols_send_state(&st);
    if (lines_cleared) protocols_send_attack(lines_cleared);
}

// end
protocols_send_gameover();
```

> Only sending is implemented. Receiving (START / ATTACK / RESULT from the Hub)
> is not in `protocols/` yet.
>
> For a runnable example see `test_protocols()` in `protocols/protocols_test.c`
> (test only — not part of the game).
