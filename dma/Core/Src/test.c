/*
 * test.c
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */


#include "test.h"
#include "dma.h"     // led_dma_take_vsync, led_dma_is_mem_free, ...
#include "color.h"   // color_build_solid_frame
#include "paint.h"

static test_mode_t s_mode;
static uint32_t    s_frames_per_step;
static uint32_t    s_step = 0;
static uint8_t     s_r = 0, s_g = 255, s_b = 0; // 시작: 초록


void test_run_once(uint32_t *fb0, uint32_t *fb1)
{
	// 프레임 경계(=vsync)에서만 상태 갱신/렌더링
    if (!led_dma_take_vsync()) return;
    s_step++;

    switch (s_mode) {
    case TEST_BLINK:
    	// 색상 업데이트
        if ((s_step % s_frames_per_step) == 0) {
            if (s_g == 255 && s_r == 0 && s_b == 0) { s_r=255; s_g=255; s_b=255; }
            else                                    { s_r=0;   s_g=255; s_b=0; }
        }
        break;

    case TEST_RAINBOW:
        if ((s_step % s_frames_per_step) == 0) {
            uint8_t pos = (uint8_t)((s_step / s_frames_per_step) & 0xFF);
            wheel(pos, &s_r, &s_g, &s_b);
        }
        break;

    case TEST_BARS:
        // 정적 패턴 → 한 번만 빌드해도 됨
        if (s_step == 1) {
            if (led_dma_is_mem_free(LED_MEM0)) { paint_color_bars(fb0); led_dma_mark_filled(LED_MEM0); }
            if (led_dma_is_mem_free(LED_MEM1)) { paint_color_bars(fb1); led_dma_mark_filled(LED_MEM1); }
            return;
        }
        break;

    case TEST_CHECKER:
        // 부드럽게 스크롤
        if (led_dma_is_mem_free(LED_MEM0)) { paint_checker_scroll(fb0, 8, s_step); led_dma_mark_filled(LED_MEM0); }
        if (led_dma_is_mem_free(LED_MEM1)) { paint_checker_scroll(fb1, 8, s_step+1); led_dma_mark_filled(LED_MEM1); }
        return;

    default: break;
    }

    // 공통: 단색 모드들 리필
    if (led_dma_is_mem_free(LED_MEM0)) { color_build_solid_frame(fb0, s_r,s_g,s_b); led_dma_mark_filled(LED_MEM0); }
    if (led_dma_is_mem_free(LED_MEM1)) { color_build_solid_frame(fb1, s_r,s_g,s_b); led_dma_mark_filled(LED_MEM1); }
}
