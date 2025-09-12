/*
 * dma.c
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

/*
 * dma.c
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */
#include "dma.h"
#include "debug_uart.h"

// 내부 정적 상태
static DMA_HandleTypeDef *s_hdma = NULL;
static GPIO_TypeDef      *s_gpio = NULL;
static volatile bool      s_mem0_free = false;
static volatile bool      s_mem1_free = false;

// 내부 콜백 선언
static void cb_tc (DMA_HandleTypeDef *hdma);        // 공통 Full (M0/M1)
static void cb_ht (DMA_HandleTypeDef *hdma);        // Half (미사용)
static void cb_m1tc(DMA_HandleTypeDef *hdma);       // M1 Full 전용
static void cb_m1ht(DMA_HandleTypeDef *hdma);       // M1 Half (미사용)
static void cb_err(DMA_HandleTypeDef *hdma);

volatile uint32_t led_dma_frame_count = 0;
static volatile bool s_vsync = false;

void led_dma_init(DMA_HandleTypeDef *hdma, GPIO_TypeDef *gpio_bsrr_port)
{
    s_hdma = hdma;
    s_gpio = gpio_bsrr_port;

    // 콜백 등록
    HAL_DMA_RegisterCallback(s_hdma, HAL_DMA_XFER_CPLT_CB_ID,       cb_tc);
    HAL_DMA_RegisterCallback(s_hdma, HAL_DMA_XFER_HALFCPLT_CB_ID,   cb_ht);
    HAL_DMA_RegisterCallback(s_hdma, HAL_DMA_XFER_M1CPLT_CB_ID,     cb_m1tc);
    HAL_DMA_RegisterCallback(s_hdma, HAL_DMA_XFER_M1HALFCPLT_CB_ID, cb_m1ht);
    HAL_DMA_RegisterCallback(s_hdma, HAL_DMA_XFER_ERROR_CB_ID,      cb_err);
}

HAL_StatusTypeDef led_dma_start(uint32_t *fb0, uint32_t *fb1, uint32_t words_per_frame)
{
    s_mem0_free = false;
    s_mem1_free = false;

    // 더블버퍼 + 인터럽트 시작 (목적지는 항상 BSRR)
    return HAL_DMAEx_MultiBufferStart_IT(
        s_hdma,
        (uint32_t)fb0,                       // M0
        (uint32_t)&s_gpio->BSRR,             // PERIPH
		(uint32_t)fb1,                       // ✅ SecondMemAddress (M1)
		words_per_frame                      // ✅ DataLength (NDTR)
    );
}

bool led_dma_is_mem_free(led_dma_mem_t m)
{
    return (m == LED_MEM0) ? s_mem0_free : s_mem1_free;
}

void led_dma_mark_filled(led_dma_mem_t m)
{
    if (m == LED_MEM0) s_mem0_free = false;
    else                  s_mem1_free = false;
}

HAL_StatusTypeDef led_dma_change_memory(led_dma_mem_t which, uint32_t *new_fb)
{
    return HAL_DMAEx_ChangeMemory(
        s_hdma,
        (uint32_t)new_fb,
        (which == LED_MEM0) ? MEMORY0 : MEMORY1
    );
}

uint32_t led_dma_ndtr(void)
{
    return __HAL_DMA_GET_COUNTER(s_hdma);
}

uint32_t led_dma_last_error(void)
{
    return HAL_DMA_GetError(s_hdma);
}

void led_dma_irq_handler(void)
{
    HAL_DMA_IRQHandler(s_hdma);
}

DMA_HandleTypeDef* led_dma_handle(void)
{
    return s_hdma;
}

/* ================== 내부 콜백 구현 ================== */
static void cb_tc(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    // 필요시 후처리 훅
    s_mem0_free = true;        // M0 채워도 됨
    led_dma_frame_count++;     // 프레임 카운트++
	s_vsync = true;            // 프레임 경계 플래그
}
static void cb_ht(DMA_HandleTypeDef *hdma) { (void)hdma; }
static void cb_m1tc(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    // M1 소비 완료 -> CPU가 M1을 다시 채워도 됨
    s_mem1_free = true;
    led_dma_frame_count++;
	s_vsync = true;
}
static void cb_m1ht(DMA_HandleTypeDef *hdma) { (void)hdma; }
static void cb_err(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    // HAL_DMA_GetError(hdma) 로 디버그 가능
    (void)hdma;
	uint32_t e = HAL_DMA_GetError(hdma);
	debug_printf("[DMA] error=0x%08lX\r\n", (unsigned long)e);

}

// vsync 플래그 소비 함수 추가
bool led_dma_take_vsync(void)
{
    bool f = s_vsync;
    s_vsync = false;
    return f;
}


/* ================== 테스트 프레임 빌더 ==================
 * DMA/BSRR 경로가 정상인지 확인하는 데모용 시퀀스:
 * - 각 픽셀마다 CLK Low/High 토글
 * - 라인 끝에서 OE High(블랭크) + LAT High/Low
 * 필요시 OE_CLR을 추가해 실제 표시 구간을 잠깐 열 수 있음.
 */
void led_dma_build_test_frame(uint32_t *fb)
{
    uint32_t *p = fb;

    for (int bp = 0; bp < LED_BITPLANES; ++bp) {
        for (int row = 0; row < LED_ROWS; ++row) {

            // 픽셀 시프트: 데이터선은 0 가정, CLK만 토글
            for (int x = 0; x < LED_PANEL_W; ++x) {
                *p++ = LED_BSRR_CLK_CLR;   // data setup 가정
                *p++ = LED_BSRR_CLK_SET;   // rising edge
            }

            // 라인 래치: 기본은 블랭크 유지
            *p++ = LED_BSRR_OE_SET;       // 블랭크(Off)
            *p++ = LED_BSRR_LAT_SET;      // 래치
            *p++ = LED_BSRR_LAT_CLR;      // 래치 해제

            // 화면 확인용으로 잠깐 열어보려면 아래 줄을 활성화
            // *p++ = LED_BSRR_OE_CLR;    // 표시 구간 시작 (테스트)
        }
    }
}

