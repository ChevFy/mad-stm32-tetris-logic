# Pinout — currently enabled

MCU: **STM32F767ZITx** (Nucleo-144). Source: `stm-logic/stm-logic.ioc` + `usart.c` MSP init.
Only the pins listed here are configured; everything else is at reset default.

## UART

| Pin  | Function   | AF  | Dir | Notes |
|------|------------|-----|-----|-------|
| PD5  | USART2_TX  | AF7 | out | 921600? no — USART2 = 115200 8N1 |
| PD6  | USART2_RX  | AF7 | in  | |
| PD8  | USART3_TX  | AF7 | out | **921600** 8N1 — link to Hub |
| PD9  | USART3_RX  | AF7 | in  | RX via DMA1_Stream1 (circular) |
| PG9  | USART6_RX  | AF8 | in  | 115200 8N1 |
| PG14 | USART6_TX  | AF8 | out | TX via DMA2_Stream6 (normal) |

- **USART3 (PD8/PD9)** is the Hub link. On Nucleo-144 this is also the ST-LINK
  virtual COM port, so it shows up on the USB serial when the board is plugged in.
- All UARTs: 8 data bits, no parity, 1 stop, no flow control.

## GPIO inputs

| Pin  | Mode        | Runtime pull | Use |
|------|-------------|---------|------------|
| PE9  | GPIO_INPUT  | PULLUP | Right, active-low |
| PE11 | GPIO_INPUT  | PULLUP | Rotate left, active-low |
| PF12 | GPIO_INPUT  | PULLUP | Soft drop (hold = fall faster), active-low |
| PF13 | GPIO_INPUT  | PULLUP | Left, active-low |
| PF14 | GPIO_INPUT  | PULLUP | Hold / skip lobby wait, active-low |
| PC13 | GPIO_INPUT  | PULLDOWN | Blue USER button: join/start, active-high |

- `buttons_init()` applies the runtime pull configuration. Connect each external
  control button between its pin and GND. All buttons use polling and software
  debounce; no GPIO interrupts or DMA are required.

## Clock / debug / system

| Pin  | Function          | Notes |
|------|-------------------|-------|
| PA13 | SWDIO             | SWD debug (ST-LINK) |
| PA14 | SWCLK             | SWD debug (ST-LINK) |
| PH0  | OSC_IN (HSE)      | HSE pin enabled in `.ioc`… |
| PH1  | OSC_OUT (HSE)     | …but `SystemClock_Config()` runs on **HSI**, not HSE |

- Clock mismatch to watch: `.ioc` marks PH0/PH1 as HSE external clock, yet the
  generated clock tree uses HSI (PLLM=8, PLLN=216 → 216 MHz SYSCLK). Confirm
  before assuming the crystal is in use.

## DMA streams

| Stream        | Channel | Peripheral | Dir           | Mode     |
|---------------|---------|------------|---------------|----------|
| DMA1_Stream1  | CH4     | USART3_RX  | periph→mem    | circular |
| DMA2_Stream6  | CH5     | USART6_TX  | mem→periph    | normal   |

## Interrupts (NVIC, all priority 0)

- USART3_IRQn
- USART6_IRQn
- DMA1_Stream1_IRQn
- DMA2_Stream6_IRQn

---
Regenerate this table if you change pins in CubeMX. Source of truth is the `.ioc`.
