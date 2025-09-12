/*
 * anim.c
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */
#include "anim.h"
#include "dma.h"
#include "paint.h"   // paint_checker_scroll, paint_rainbow_scroll

// 상태
static anim_mode_t s_mode;
static uint32_t    s_frames_per_step = 1;
static uint32_t    s_step = 0;

// checker
static uint8_t     s_tile = 8;
static uint32_t    s_scroll = 0;

// rainbow
static uint8_t     s_hue_step = 1;   // 한 번 움직일 때의 페이즈 증가량(1~4)
static uint16_t    s_phase = 0;      // 0..65535 랩어라운드

void anim_init_checker(uint8_t tile, uint32_t frames_per_step,
                       uint32_t *fb0, uint32_t *fb1)
{
    s_mode = ANIM_CHECKER_SCROLL;
    s_tile = tile ? tile : 8;
    s_frames_per_step = frames_per_step ? frames_per_step : 1;
    s_step = 0;
    s_scroll = 0;

    // 초기 프레임 두 장 (DMA 시작 전 호출 권장)
    paint_checker_scroll(fb0, s_tile, s_scroll);
    paint_checker_scroll(fb1, s_tile, s_scroll+1);
}

void anim_init_rainbow(uint8_t hue_step_per_step, uint32_t frames_per_step,
                       uint32_t *fb0, uint32_t *fb1)
{
    s_mode = ANIM_RAINBOW_SCROLL;
    s_hue_step = (hue_step_per_step == 0) ? 1 : hue_step_per_step;
    s_frames_per_step = frames_per_step ? frames_per_step : 1;
    s_step = 0;
    s_phase = 0;

    // 초기 프레임 두 장
    paint_rainbow_scroll(fb0, s_phase);
    paint_rainbow_scroll(fb1, s_phase + s_hue_step);
}

void anim_service(uint32_t *fb0, uint32_t *fb1)
{
    if (!led_dma_take_vsync()) return;  // 프레임 경계에서만 작업

    s_step++;
    bool advance = ((s_step % s_frames_per_step) == 0);

    switch (s_mode) {
    case ANIM_CHECKER_SCROLL:
        if (advance) s_scroll++;
        if (led_dma_is_mem_free(LED_MEM0)) {
            paint_checker_scroll(fb0, s_tile, s_scroll);
            led_dma_mark_filled(LED_MEM0);
        }
        if (led_dma_is_mem_free(LED_MEM1)) {
            paint_checker_scroll(fb1, s_tile, s_scroll+1);
            led_dma_mark_filled(LED_MEM1);
        }
        break;

    case ANIM_RAINBOW_SCROLL:
        if (advance) s_phase += s_hue_step;  // 16-bit 랩 자동
        if (led_dma_is_mem_free(LED_MEM0)) {
            paint_rainbow_scroll(fb0, s_phase);
            led_dma_mark_filled(LED_MEM0);
        }
        if (led_dma_is_mem_free(LED_MEM1)) {
            paint_rainbow_scroll(fb1, s_phase + s_hue_step);
            led_dma_mark_filled(LED_MEM1);
        }
        break;
    }
}


