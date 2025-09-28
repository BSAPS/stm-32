/*
 * dma.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */
#pragma once
#include "stm32f4xx_hal.h"
#include "main.h"            // CLK_Pin, LAT_Pin, OE_Pin 등 핀 매크로 사용
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ====== HUB75 / 프레임 구성 기본값 (원하면 빌드옵션에서 오버라이드) ======
#ifndef LED_PANEL_W
#define LED_PANEL_W        64
#endif
#ifndef LED_PANEL_H
#define LED_PANEL_H        32
#endif
#ifndef LED_BITPLANES
#define LED_BITPLANES       5
#endif

enum {
    LED_ROWS             = LED_PANEL_H / 2,
    LED_WORDS_PER_SHIFT  = 2,  // CLK low + CLK high (데모/검증용 최소)
    LED_WORDS_PER_LATCH  = 3,  // OE High, LAT High, LAT Low (데모)
    LED_WORDS_PER_ROW    = (LED_PANEL_W * LED_WORDS_PER_SHIFT) + LED_WORDS_PER_LATCH,
    LED_WORDS_PER_FRAME  = LED_ROWS * LED_BITPLANES * LED_WORDS_PER_ROW
};

// ====== BSRR 유틸 ======
#define LED_BSRR_SET(mask)   ((uint32_t)(mask))
#define LED_BSRR_CLR(mask)   ((uint32_t)(mask) << 16U)

#define LED_BSRR_CLK_SET     LED_BSRR_SET(CLK_Pin)
#define LED_BSRR_CLK_CLR     LED_BSRR_CLR(CLK_Pin)
#define LED_BSRR_LAT_SET     LED_BSRR_SET(LAT_Pin)
#define LED_BSRR_LAT_CLR     LED_BSRR_CLR(LAT_Pin)
#define LED_BSRR_OE_SET      LED_BSRR_SET(OE_Pin)
#define LED_BSRR_OE_CLR      LED_BSRR_CLR(OE_Pin)


// ====== DMA 드라이버 API ======
typedef enum { LED_MEM0 = 0, LED_MEM1 = 1 } led_dma_mem_t;

void               led_dma_init(DMA_HandleTypeDef *hdma, GPIO_TypeDef *gpio_bsrr_port);
HAL_StatusTypeDef  led_dma_start(uint32_t *fb0, uint32_t *fb1, uint32_t words_per_frame);
bool               led_dma_is_mem_free(led_dma_mem_t m);
void               led_dma_mark_filled(led_dma_mem_t m);
HAL_StatusTypeDef  led_dma_change_memory(led_dma_mem_t which, uint32_t *new_fb);
uint32_t           led_dma_ndtr(void);
uint32_t           led_dma_last_error(void);
void               led_dma_irq_handler(void);
DMA_HandleTypeDef* led_dma_handle(void);

extern volatile uint32_t led_dma_frame_count;   // 누적 프레임 수
// vsync 플래그 소비 함수 추가
bool led_dma_take_vsync(void);                  // vsync 플래그 읽고 클리어

// ====== 편의 함수: 상수/테스트 프레임 ======
// 상수 값을 함수로도 노출(매크로 대신 호출해서 써도 됨)
static inline uint32_t led_dma_words_per_frame_const(void) { return (uint32_t)LED_WORDS_PER_FRAME; }

// DMA 경로/BSRR 검증용 테스트 프레임 빌더 (CLK 토글 + LAT/OE 제어 최소 시퀀스)
void led_dma_build_test_frame(uint32_t *fb);

#ifdef __cplusplus
}
#endif
