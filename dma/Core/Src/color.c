/*
 * color.c
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

/*
 * color.c - HUB75 bitplane packing (solid color), BP=5
 */
#include "color.h"
#include <math.h>   // 감마 적용 시 사용
#include "debug_uart.h"

static float s_gamma = 1.0f;

void color_set_gamma(float g) {
    if (g < 0.1f) g = 0.1f;
    if (g > 5.0f) g = 5.0f;
    s_gamma = g;
}

uint8_t color_u8_to_u5(uint8_t v8) {
    if (s_gamma == 1.0f) {
        // 선형: 0..255 -> 0..31
        // 반올림: (v*31 + 127)/255
        return (uint8_t)((v8 * 31 + 127) / 255);
    } else {
        float vf = (float)v8 / 255.0f;
        float vg = powf(vf, s_gamma);
        int v5 = (int)lroundf(vg * 31.0f);
        if (v5 < 0) v5 = 0;
        if (v5 > 31) v5 = 31;
        return (uint8_t)v5;
    }
}

/* 내부: row의 하위비트를 A/B/C/D에 반영해 한 워드로 구성 */
static inline uint32_t addr_word_from_row(int row) {
    uint32_t w = 0;
    w |= (row & 0x1) ? LED_BSRR_A_SET : LED_BSRR_A_CLR;
    w |= (row & 0x2) ? LED_BSRR_B_SET : LED_BSRR_B_CLR;
    w |= (row & 0x4) ? LED_BSRR_C_SET : LED_BSRR_C_CLR;
    w |= (row & 0x8) ? LED_BSRR_D_SET : LED_BSRR_D_CLR;
    return w;
}

/* 내부: 한 비트플레인의 RGB 상태(상/하 동일색) -> 한 워드 */
static inline uint32_t color_word(bool r_on, bool g_on, bool b_on) {
    uint32_t w = 0;
    w |= r_on ? LED_BSRR_R1_SET : LED_BSRR_R1_CLR;
    w |= g_on ? LED_BSRR_G1_SET : LED_BSRR_G1_CLR;
    w |= b_on ? LED_BSRR_B1_SET : LED_BSRR_B1_CLR;

    w |= r_on ? LED_BSRR_R2_SET : LED_BSRR_R2_CLR;
    w |= g_on ? LED_BSRR_G2_SET : LED_BSRR_G2_CLR;
    w |= b_on ? LED_BSRR_B2_SET : LED_BSRR_B2_CLR;
    return w;
}

/*
 * NOTE (중요):
 * - 네 dma.h의 행 워드 수는 (2*W + 3) 로 정의되어 있음.
 *   여기서는 그 "3워드"를 아래처럼 구성해 맞춥니다:
 *   [PRESHIFT] OE_SET | 색선 | 주소      (1워드)
 *   [LATCH1]   LAT_SET                 (1워드)
 *   [LATCH2]   LAT_CLR | OE_CLR        (1워드) -> 표시 시작
 *
 * - 별도의 HOLD(표시 유지) 워드를 추가하지 않습니다. (메모리/정의 일치)
 *   밝기가 너무 어두우면 TIM 주기를 늘리거나, 나중에 dma.h를 확장(hold 추가)해도 됩니다.
 */
void color_build_solid_frame(uint32_t *fb, uint8_t r8, uint8_t g8, uint8_t b8)
{
    uint32_t *p = fb;

    uint8_t r5 = color_u8_to_u5(r8);
    uint8_t g5 = color_u8_to_u5(g8);
    uint8_t b5 = color_u8_to_u5(b8);

    for (int bp = 0; bp < LED_BITPLANES; ++bp) {
        bool r_on = (r5 >> bp) & 1;
        bool g_on = (g5 >> bp) & 1;
        bool b_on = (b5 >> bp) & 1;

        uint32_t cw   = color_word(r_on, g_on, b_on);
        uint32_t lat1 = LED_BSRR_LAT_SET;
        uint32_t lat2 = (LED_BSRR_LAT_CLR | LED_BSRR_OE_CLR);  // 표시 시작

        for (int row = 0; row < (LED_PANEL_H/2); ++row) {
            // [PRESHIFT] (OE=High로 끄고, 색/주소 세팅)
            *p++ = LED_BSRR_OE_SET | cw | addr_word_from_row(row);

            // [SHIFT] 픽셀 수만큼 CLK 토글 (데이터는 cw로 이미 고정)
            for (int x = 0; x < LED_PANEL_W; ++x) {
                *p++ = LED_BSRR_CLK_CLR;
                *p++ = LED_BSRR_CLK_SET;
            }

            // [LATCH]
            *p++ = lat1;
            *p++ = lat2;   // OE를 Low로 내려 표시 시작 (HOLD는 추가하지 않음)
        }
    }
}

