/*
 * debug_uart.c
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */


#include "debug_uart.h"
#include "main.h"
#include "dma.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *s_uart = NULL;

void debug_uart_init(UART_HandleTypeDef *huart) { s_uart = huart; }

void debug_printf(const char *fmt, ...)
{
    if (!s_uart) return;
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    HAL_UART_Transmit(s_uart, (uint8_t*)buf, (uint16_t)n, 10); // 블로킹이지만 매우 짧게, 루프에서만 호출
}

float debug_tim1_update_hz(TIM_HandleTypeDef *htim)
{
    RCC_ClkInitTypeDef clk; uint32_t flashLat;
    HAL_RCC_GetClockConfig(&clk, &flashLat);

    uint32_t pclk2  = HAL_RCC_GetPCLK2Freq();
    uint32_t timclk = (clk.APB2CLKDivider == RCC_HCLK_DIV1) ? pclk2 : (pclk2 * 2U);

    uint32_t psc = htim->Instance->PSC;   // __HAL_TIM_GET_PRESCALER(htim) 대체
    uint32_t arr = htim->Instance->ARR;   // __HAL_TIM_GET_AUTORELOAD(htim) 대체
    if ((psc + 1U) == 0 || (arr + 1U) == 0) return 0.0f;

    return (float)timclk / (float)((psc + 1U) * (arr + 1U));
}

float debug_tim1_fps(TIM_HandleTypeDef *htim, uint32_t words_per_frame)
{
    float fupd = debug_tim1_update_hz(htim);
    return words_per_frame ? (fupd / (float)words_per_frame) : 0.0f;
}

// 핀 매핑 간단 점검: 모두 GPIOB인지, 데이터 vs 제어 비트가 겹치지 않는지
void debug_check_pins(void)
{
    bool allB =
        (R1_GPIO_Port==GPIOB) && (G1_GPIO_Port==GPIOB) && (B1_GPIO_Port==GPIOB) &&
        (R2_GPIO_Port==GPIOB) && (G2_GPIO_Port==GPIOB) && (B2_GPIO_Port==GPIOB) &&
        (A_GPIO_Port==GPIOB)  && (B_GPIO_Port==GPIOB)  && (C_GPIO_Port==GPIOB)  &&
        (D_GPIO_Port==GPIOB)  && (CLK_GPIO_Port==GPIOB)&& (LAT_GPIO_Port==GPIOB)&&
        (OE_GPIO_Port==GPIOB);

    uint32_t dataMask = R1_Pin|G1_Pin|B1_Pin|R2_Pin|G2_Pin|B2_Pin;
    uint32_t ctrlMask = A_Pin|B_Pin|C_Pin|D_Pin|CLK_Pin|LAT_Pin|OE_Pin;

    debug_printf("[PIN] all on GPIOB: %s\r\n", allB ? "YES":"NO");
    if (!allB) {
        debug_printf(" - CHECK: some pins are not on GPIOB → DMA(BSRR) 미적용\r\n");
    }
    if (dataMask & ctrlMask) {
        debug_printf("[PIN] ERROR: DATA and CTRL pins overlap! mask=0x%08lX\r\n",
                     (unsigned long)(dataMask & ctrlMask));
    } else {
        debug_printf("[PIN] data/ctrl overlap: none\r\n");
    }
}
