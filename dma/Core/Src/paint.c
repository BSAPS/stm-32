/*
 * paint.c
 *
 *  Created on: Sep 5, 2025
 *      Author: jioh
 */

#include "paint.h"
#include "dma.h"     // LED_* 매크로, 패널 크기
#include "color.h"   // color_u8_to_u5 or color_u8_to_bits
#include "anim.h"

// --- 내부 유틸: 행 주소(A/B/C/D) 비트 세팅 ---
static inline uint32_t addr_word_from_row(int row) {
    uint32_t w = 0;
    w |= (row & 0x1) ? LED_BSRR_A_SET : LED_BSRR_A_CLR;
    w |= (row & 0x2) ? LED_BSRR_B_SET : LED_BSRR_B_CLR;
    w |= (row & 0x4) ? LED_BSRR_C_SET : LED_BSRR_C_CLR;
    w |= (row & 0x8) ? LED_BSRR_D_SET : LED_BSRR_D_CLR;
    return w;
}

// --- 내부 유틸: 한 픽셀의 RGB 비트(상/하 동일) -> BSRR 워드 ---
static inline uint32_t cw_from_bits(bool r, bool g, bool b) {
    uint32_t w = 0;
    w |= r ? LED_BSRR_R1_SET : LED_BSRR_R1_CLR;
    w |= g ? LED_BSRR_G1_SET : LED_BSRR_G1_CLR;
    w |= b ? LED_BSRR_B1_SET : LED_BSRR_B1_CLR;
    w |= r ? LED_BSRR_R2_SET : LED_BSRR_R2_CLR;
    w |= g ? LED_BSRR_G2_SET : LED_BSRR_G2_CLR;
    w |= b ? LED_BSRR_B2_SET : LED_BSRR_B2_CLR;
    return w;
}

// --- 내부 유틸: 8비트 -> 5비트(LED_BITPLANES에 자동 적응 버전 쓰려면 color_u8_to_bits 사용) ---
static inline uint8_t u8_to_plane(uint8_t v8) {
    return color_u8_to_u5(v8);   // BP=5 전제. 자동적응 버전이면 color_u8_to_bits(v8)
}

// --- 내부 공통 빌더: per-pixel 데이터 출력 ---
// 흐름: [OE High+행주소 클리어] → (각 픽셀: 데이터+CLK L, CLK H) → LAT H → LAT L + OE L
static void build_from_pixel_fn(uint32_t *fb,
                                void (*pixel_rgb)(int x,int y,uint8_t* r,uint8_t* g,uint8_t* b))
{
    uint32_t *p = fb;

    // 상/하 절반(행수=H/2)만 스캔, 각 행은 상/하 페어
    for (int bp = 0; bp < LED_BITPLANES; ++bp) {

        for (int row = 0; row < (LED_PANEL_H/2); ++row) {

            // 1) 프리셋: OE=High(블랭크) + 주소 세팅 + RGB 모두 클리어
            uint32_t rgb_all_clr = LED_BSRR_CLR(R1_Pin|G1_Pin|B1_Pin|R2_Pin|G2_Pin|B2_Pin);
            *p++ = LED_BSRR_OE_SET | addr_word_from_row(row) | rgb_all_clr;

            // 2) 픽셀 시프트: 각 픽셀마다 데이터 세팅 후 CLK 토글
            for (int x = 0; x < LED_PANEL_W; ++x) {
                // (x,row)의 색(상/하 동일)을 8비트로 얻음
                uint8_t r8,g8,b8;
                pixel_rgb(x, row, &r8,&g8,&b8);

                // 8→5비트 → 현재 비트플레인의 bit 추출
                uint8_t rN = u8_to_plane(r8);
                uint8_t gN = u8_to_plane(g8);
                uint8_t bN = u8_to_plane(b8);

                bool r_on = (rN >> bp) & 1;
                bool g_on = (gN >> bp) & 1;
                bool b_on = (bN >> bp) & 1;

                uint32_t cw = cw_from_bits(r_on, g_on, b_on);

                // 데이터 세팅 + CLK Low
                *p++ = cw | LED_BSRR_CLK_CLR;
                // CLK High (래치-into-shift)
                *p++ = LED_BSRR_CLK_SET;
            }

            // 3) LATCH → 표시 시작(OE Low)
            *p++ = LED_BSRR_LAT_SET;
            *p++ = (LED_BSRR_LAT_CLR | LED_BSRR_OE_CLR);
        }
    }
}

// ---- 패턴 1: 컬러 바 (8등분) ----
static void pixel_color_bars(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b)
{
    (void)y;
    int region = (x * 9) / LED_PANEL_W;   // 0..7
    switch (region) {
        case 0: *r=255; *g=255; *b=255; break; // White
        case 1: *r=255; *g=0;   *b=0;   break; // Red
        case 2: *r=0;   *g=255; *b=0;   break; // Green
        case 3: *r=0;   *g=0;   *b=255; break; // Blue
        case 4: *r=107; *g=142; *b=35; break; // olive
        case 5: *r=255; *g=0;   *b=255; break; // Magenta
        case 6: *r=255; *g=255; *b=0;   break; // Yellow
        case 7: *r=184; *g=134; *b=11;   break; // darkgolden
        default:*r=0;   *g=0;   *b=0;   break; // Black
    }
}
void paint_color_bars(uint32_t *fb) {
    build_from_pixel_fn(fb, pixel_color_bars);
}

// ---- 패턴 2: 체커보드 ----
static uint8_t s_tile = 8;
static uint32_t s_step = 0;

static void pixel_checker(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b)
{
    uint8_t tile = s_tile ? s_tile : 8;
    uint8_t cx = ((x + (int)(s_step)) / tile) & 1;
    uint8_t cy = (y / tile) & 1;
    uint8_t on = (cx ^ cy) ? 255 : 0;

    // 높은 대비(화이트/블랙)
    *r = on; *g = on; *b = on;
}
void paint_checker(uint32_t *fb, uint8_t tile)
{
    s_tile = tile;
    s_step = 0;
    build_from_pixel_fn(fb, pixel_checker);
}
void paint_checker_scroll(uint32_t *fb, uint8_t tile, uint32_t step)
{
    s_tile = tile; s_step = step;
    build_from_pixel_fn(fb, pixel_checker);
}


//---------------------
// --- 0..255 -> RGB 휠 (HSV H only) ---
static inline void wheel(uint8_t pos, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = pos / 43;        // 0..5
    uint8_t rem    = pos % 43;        // 0..42
    uint8_t p      = rem * 6;         // 0..252

    switch (region) {
    case 0: *r=255; *g=p;   *b=0;   break;
    case 1: *r=255-p; *g=255; *b=0; break;
    case 2: *r=0;   *g=255; *b=p;   break;
    case 3: *r=0;   *g=255-p; *b=255; break;
    case 4: *r=p;   *g=0;   *b=255; break;
    default:*r=255; *g=0;   *b=255-p; break;
    }
}

void paint_rainbow_scroll(uint32_t *fb, uint16_t phase)
{
    uint32_t *p = fb;
    const uint32_t per_pixel = 65536u / LED_PANEL_W;  // 픽셀당 phase 증가량(정확히 1px 이동 기준)

    for (int bp = 0; bp < LED_BITPLANES; ++bp) {
            for (int row = 0; row < (LED_PANEL_H/2); ++row) {
                uint32_t rgb_all_clr = LED_BSRR_CLR(R1_Pin|G1_Pin|B1_Pin|R2_Pin|G2_Pin|B2_Pin);
                *p++ = LED_BSRR_OE_SET | addr_word_from_row(row) | rgb_all_clr;

                uint32_t acc = phase;  // 16-bit 누적 사용
                for (int x = 0; x < LED_PANEL_W; ++x) {
                    uint8_t hue = (uint8_t)(acc >> 8);  // 여기서만 8비트로 변환
                    uint8_t r8,g8,b8; wheel(hue, &r8,&g8,&b8);

                    uint8_t rN = u8_to_plane(r8);
                    uint8_t gN = u8_to_plane(g8);
                    uint8_t bN = u8_to_plane(b8);

                    bool r_on = (rN >> bp) & 1;
                    bool g_on = (gN >> bp) & 1;
                    bool b_on = (bN >> bp) & 1;

                    uint32_t cw = cw_from_bits(r_on, g_on, b_on);
                    *p++ = cw | LED_BSRR_CLK_CLR;
                    *p++ = LED_BSRR_CLK_SET;

                    acc += per_pixel;  // 다음 픽셀의 hue advance
                }

                *p++ = LED_BSRR_LAT_SET;
                *p++ = (LED_BSRR_LAT_CLR | LED_BSRR_OE_CLR);
            }
        }
}

