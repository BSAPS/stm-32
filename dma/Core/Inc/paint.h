/*
 * paint.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"


///* --- 내부에서 사용할 BSRR 유틸 --- */
///* 주소선 */
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


// 픽셀단 패턴 생성기
void paint_color_bars(uint32_t *fb);                 // W,R,G,B,C,M,Y,K 8바
void paint_checker(uint32_t *fb, uint8_t tile);      // tile=4,8,16 같은 칸 크기
void paint_checker_scroll(uint32_t *fb, uint8_t tile, uint32_t step); // step으로 애니메이션

void paint_rainbow_scroll(uint32_t *fb, uint16_t phase);

/* ===== 폰트/스프라이트 ===== */
/* 1bpp(모노) 비트맵: 너비= w, 높이= h, 메모리형식= 각 행이 바이트 배열(LSB..MSB) */
void paint_mono_bitmap(uint32_t *fb,
                       const uint8_t *rows, int w, int h, int bytes_per_row,
                       int x0, int y0,
                       uint8_t fr,uint8_t fg,uint8_t fb8,   // FG 색
                       uint8_t br,uint8_t bg,uint8_t bb,     // BG 색
                       bool transparent_bg);

/* 16열(=2바이트) big digit 포맷(led_font.h의 big_bitmap_X)을 렌더링 */
void paint_big_digit(uint32_t *fb,
                     const uint8_t (*bmp)[2], int w, int h,  // 보통 11x16, 배열은 열 16bit (=2byte)
                     int x0, int y0,
                     uint8_t fr,uint8_t fg,uint8_t fb8,
                     uint8_t br,uint8_t bg,uint8_t bb,
                     bool transparent_bg);

/* 0..7 팔레트 인덱스 스프라이트(예: marioBits 32x32) */
void paint_sprite_palette(uint32_t *fb,
                          const uint8_t *pix, int w, int h, int stride,
                          int x0, int y0,
                          const uint8_t palette[8][3],   // [idx][r,g,b]
                          bool transparent0);




// "am/pm + hh:mm" 한 프레임을 바로 채움 (포그라운드/배경 RGB 0..255)
void paint_time_frame(uint32_t *fb,
                      int hour, int minute, bool is_am,
                      uint8_t fr, uint8_t fg, uint8_t fbk,
                      uint8_t br, uint8_t bg, uint8_t bb);

#ifdef __cplusplus
extern "C" {
#endif

