/**
  ******************************************************************************
  * @file    utils.h
  * @brief   Utility helpers (UART test, etc).
  ******************************************************************************
  */
#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Send "hello" over USART3 (PD8 TX), blocking. */
void USART3_Test_SendHello(void);

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_H__ */
