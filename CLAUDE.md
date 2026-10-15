# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

`mad-stm32-tetris-logic` — the logic/controller firmware for a Tetris project running on
an **STM32F767ZI** (Nucleo-144, Cortex-M7 @ 216 MHz). Bare-metal, STM32 HAL, no RTOS.
Generated and configured with **STM32CubeMX / STM32CubeIDE** (`stm-logic/stm-logic.ioc`).

The actual Tetris application logic is **not yet written** — the tree today is the
CubeMX skeleton (peripheral init only, empty `while(1)`). New code goes in the
`/* USER CODE BEGIN ... */ ... /* USER CODE END ... */` guarded blocks so CubeMX
regeneration does not wipe it.

## Layout

```
stm-logic/
  stm-logic.ioc          CubeMX project — edit peripherals/pins HERE, then regenerate
  STM32F767ZITX_FLASH.ld linker script (flash target) / ..._RAM.ld (RAM target)
  Core/
    Inc/                 main.h, gpio.h, dma.h, usart.h, HAL conf, IRQ decls
    Src/                 main.c, gpio.c, dma.c, usart.c, *_it.c, syscalls/sysmem, HAL MSP
    Startup/             startup_stm32f767zitx.s
  Drivers/               STM32F7xx HAL + CMSIS (vendor code — do not edit)
```

## Build / flash

- **Build:** STM32CubeIDE (GUI, Eclipse-based). Import the `stm-logic/` project, build.
  There is no checked-in Makefile — CubeIDE generates one under `Debug/` at build time.
- **Flash/debug:** ST-LINK via CubeIDE, or `STM32_Programmer_CLI` on the produced `.elf`/`.bin`.
- Toolchain: arm-none-eabi-gcc (bundled with CubeIDE).

## Hardware config (from `.ioc` + MSP init)

- **Clock:** `SystemClock_Config()` runs on **HSI** (PLLM=8, PLLN=216, PLLP=/2) → 216 MHz
  SYSCLK, over-drive on. APB1 /4, APB2 /2. (Note: `.ioc` lists HSE on PH0/PH1 but the
  generated clock tree uses HSI — confirm before relying on an external crystal.)
- **UARTs** (all 115200 8N1):
  - `USART2` — PD5 TX / PD6 RX. Plain interrupt mode.
  - `USART3` — PD8 TX / PD9 RX. **RX via DMA** (DMA1_Stream1, circular). IRQ enabled.
  - `USART6` — PG9 RX / PG14 TX. **TX via DMA** (DMA2_Stream6, normal).
  Handles: `huart2`, `huart3`, `huart6` in `usart.c`. UARTs are the link to the rest of
  the Tetris system (display / input boards).
- **GPIO inputs:** PF13, PF14, PE9, PE11 (no pull) — likely buttons/inputs.
- **Cache/MPU:** I-Cache enabled; MPU region 0 configured in `MPU_Config()`.

## Conventions

- Keep hand-written code inside `USER CODE` guard comments. Anything outside them is
  owned by CubeMX and will be overwritten on regeneration.
- Peripheral/pin changes: edit the `.ioc` in CubeMX and regenerate rather than hand-editing
  `gpio.c` / `usart.c` / `dma.c`.
- `Drivers/` is vendor HAL/CMSIS — treat as read-only.
- `Error_Handler()` currently just disables IRQs and spins.
