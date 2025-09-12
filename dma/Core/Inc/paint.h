/*
 * paint.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "main.h"


// 픽셀단 패턴 생성기
void paint_color_bars(uint32_t *fb);                 // W,R,G,B,C,M,Y,K 8바
void paint_checker(uint32_t *fb, uint8_t tile);      // tile=4,8,16 같은 칸 크기
void paint_checker_scroll(uint32_t *fb, uint8_t tile, uint32_t step); // step으로 애니메이션

void paint_rainbow_scroll(uint32_t *fb, uint16_t phase);


#ifdef __cplusplus
extern "C" {
#endif

