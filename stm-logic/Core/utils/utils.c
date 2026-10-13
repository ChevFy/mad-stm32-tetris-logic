/**
  ******************************************************************************
  * @file    utils.c
  * @brief   Utility helpers (UART test, etc).
  ******************************************************************************
  */
#include "utils.h"
#include "usart.h"
#include <string.h>

/**
  * @brief  Test: send "hello" over USART3 (PD8 TX), blocking.
  */
void USART3_Test_SendHello(void)
{
  const char msg[] = "hello\r\n";
  HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}
