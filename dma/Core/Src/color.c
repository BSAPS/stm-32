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
