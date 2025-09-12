/*
 * test.h
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 간단 테스트 모드
typedef enum {
    TEST_BLINK,    // 초록 ↔ 흰색 토글
    TEST_RAINBOW,   // 무지개 색상 순환
	TEST_BARS,       // ★ 추가: 컬러 바
	TEST_CHECKER     // ★ 추가: 체커보드
} test_mode_t;


/** 메인 루프에서 계속 호출. vsync가 오면 색 갱신 + 비어있는 버퍼 채움 */
void test_run_once(uint32_t *fb0, uint32_t *fb1);

#ifdef __cplusplus
}
#endif
