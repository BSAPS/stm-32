/*
 * anim.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ANIM_CHECKER_SCROLL = 0,
  ANIM_RAINBOW_SCROLL = 1
} anim_mode_t;

/** 체크보드 스크롤 초기화
 *  @param tile             타일 크기(4/8/16 등)
 *  @param frames_per_step  몇 프레임에 한 칸 이동할지(느리게: 8, 빠르게: 1~2)
 *  @param fb0, fb1         DMA 더블버퍼에 초기 프레임 바로 채움(권장: DMA 시작 전 호출)
 */
void anim_init_checker(uint8_t tile, uint32_t frames_per_step,
                       uint32_t *fb0, uint32_t *fb1);

/** 레인보우(가로 그라데이션) 스크롤 초기화
 *  @param hue_step_per_step  단계당 색상 페이즈 증가량(1~4 권장)
 *  @param frames_per_step    몇 프레임마다 페이즈를 한 번 증가할지
 */
void anim_init_rainbow(uint8_t hue_step_per_step, uint32_t frames_per_step,
                       uint32_t *fb0, uint32_t *fb1);

/** 매 프레임 경계(vsync)에서 빈 버퍼만 갱신.
 *  메인 while(1)에서 주기적으로 호출하세요(바쁜일 없음).
 */
void anim_service(uint32_t *fb0, uint32_t *fb1);

#ifdef __cplusplus
}
#endif
