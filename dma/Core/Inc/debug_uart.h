/*
 * debug_uart.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#pragma once
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void debug_uart_init(UART_HandleTypeDef *huart);
void debug_printf(const char *fmt, ...);

// 유틸: TIM1 → update 주파수 / FPS 계산
float debug_tim1_update_hz(TIM_HandleTypeDef *htim);
float debug_tim1_fps(TIM_HandleTypeDef *htim, uint32_t words_per_frame);

// 유틸: 핀 매핑 점검(모두 GPIOB인지, 데이터와 제어 비트 겹침 없는지)
void debug_check_pins(void);

#ifdef __cplusplus
}
#endif
