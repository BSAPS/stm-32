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

/** 단색 프레임(BSRR 워드 스트림) 생성: r/g/b는 0..255 */
void color_build_solid_frame(uint32_t *fb, uint8_t r8, uint8_t g8, uint8_t b8);

/* --- 내부에서 사용할 BSRR 유틸 (dma.h의 LED_BSRR_SET/CLR 사용) --- */
/* 주소선 */
#define LED_BSRR_A_SET    LED_BSRR_SET(A_Pin)
#define LED_BSRR_A_CLR    LED_BSRR_CLR(A_Pin)
#define LED_BSRR_B_SET    LED_BSRR_SET(B_Pin)
#define LED_BSRR_B_CLR    LED_BSRR_CLR(B_Pin)
#define LED_BSRR_C_SET    LED_BSRR_SET(C_Pin)
#define LED_BSRR_C_CLR    LED_BSRR_CLR(C_Pin)
#define LED_BSRR_D_SET    LED_BSRR_SET(D_Pin)
#define LED_BSRR_D_CLR    LED_BSRR_CLR(D_Pin)

/* 상단/하단 RGB */
#define LED_BSRR_R1_SET   LED_BSRR_SET(R1_Pin)
#define LED_BSRR_R1_CLR   LED_BSRR_CLR(R1_Pin)
#define LED_BSRR_G1_SET   LED_BSRR_SET(G1_Pin)
#define LED_BSRR_G1_CLR   LED_BSRR_CLR(G1_Pin)
#define LED_BSRR_B1_SET   LED_BSRR_SET(B1_Pin)
#define LED_BSRR_B1_CLR   LED_BSRR_CLR(B1_Pin)

#define LED_BSRR_R2_SET   LED_BSRR_SET(R2_Pin)
#define LED_BSRR_R2_CLR   LED_BSRR_CLR(R2_Pin)
#define LED_BSRR_G2_SET   LED_BSRR_SET(G2_Pin)
#define LED_BSRR_G2_CLR   LED_BSRR_CLR(G2_Pin)
#define LED_BSRR_B2_SET   LED_BSRR_SET(B2_Pin)
#define LED_BSRR_B2_CLR   LED_BSRR_CLR(B2_Pin)

#ifdef __cplusplus
}
#endif
