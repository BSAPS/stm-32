/*
 * color.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "main.h"   // A_Pin, B_Pin, ..., R1_Pin 등
#include "dma.h"    // LED_PANEL_W/H, LED_BITPLANES, LED_WORDS_PER_FRAME, LED_BSRR_* 매크로

#ifdef __cplusplus
extern "C" {
#endif

/** 간단 감마 설정 (기본 1.0 = 선형). 필요 없으면 안 불러도 됨. */
void color_set_gamma(float g);

/** 8비트 -> 5비트(0..31) 변환 (내부 감마 적용) */
uint8_t color_u8_to_u5(uint8_t v8);



#ifdef __cplusplus
}
#endif
