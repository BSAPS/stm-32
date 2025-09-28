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
#include "font.h"

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
// -- 상하 분리 09/29
static inline uint32_t cw_from_bits(bool r1, bool g1, bool b1, bool r2, bool g2, bool b2 ) {
    uint32_t w = 0;
    w |= r1 ? LED_BSRR_R1_SET : LED_BSRR_R1_CLR;
    w |= g1 ? LED_BSRR_G1_SET : LED_BSRR_G1_CLR;
    w |= b1 ? LED_BSRR_B1_SET : LED_BSRR_B1_CLR;
    w |= r2 ? LED_BSRR_R2_SET : LED_BSRR_R2_CLR;
    w |= g2 ? LED_BSRR_G2_SET : LED_BSRR_G2_CLR;
    w |= b2 ? LED_BSRR_B2_SET : LED_BSRR_B2_CLR;
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
                // (x,row)의 색을 8비트로 얻음
            	// 상하 색 구분!
                uint8_t r1,g1,b1,r2,g2,b2;
                pixel_rgb(x, row,           &r1,&g1,&b1);          // top half
                pixel_rgb(x, row+LED_ROWS,  &r2,&g2,&b2);          // bottom half

                // 8→5비트 → 현재 비트플레인의 bit 추출
                uint8_t rN1 = u8_to_plane(r1);
                uint8_t gN1 = u8_to_plane(g1);
                uint8_t bN1 = u8_to_plane(b1);
                uint8_t rN2 = u8_to_plane(r2);
                uint8_t gN2 = u8_to_plane(g2);
                uint8_t bN2 = u8_to_plane(b2);

                // 이번 비트플레인의 on/off
                bool r1_on = (rN1 >> bp) & 1;
                bool g1_on = (gN1 >> bp) & 1;
                bool b1_on = (bN1 >> bp) & 1;
                bool r2_on = (rN2 >> bp) & 1;
                bool g2_on = (gN2 >> bp) & 1;
                bool b2_on = (bN2 >> bp) & 1;

                //uint32_t cw = cw_from_bits(r_on, g_on, b_on);
                //cw_from_bits 사용!
                uint32_t cw = cw_from_bits(r1_on,g1_on,b1_on, r2_on,g2_on,b2_on);

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


/* ================== 패턴: 무지개 스크롤(수평 진행) ================== */
static inline void wheel(uint8_t pos, uint8_t *r, uint8_t *g, uint8_t *b)
{
    const uint8_t region = pos / 43;     // 0..5
    const uint8_t rem    = pos % 43;     // 0..42
    const uint8_t p      = rem * 6;      // 0..252

    switch (region) {
    case 0: *r=255;   *g=p;     *b=0;   break;
    case 1: *r=255-p; *g=255;   *b=0;   break;
    case 2: *r=0;     *g=255;   *b=p;   break;
    case 3: *r=0;     *g=255-p; *b=255; break;
    case 4: *r=p;     *g=0;     *b=255; break;
    default:*r=255;   *g=0;     *b=255-p; break;
    }
}

void paint_rainbow_scroll(uint32_t *fb, uint16_t phase)
{
    // 픽셀당 고정 위상 증가량(정확히 1px당 색상 진행)
    const uint32_t per_pixel = 65536u / LED_PANEL_W;

    // 무지개 픽셀 콜백(상/하 공통으로 같은 색)
    static uint16_t s_phase;  // build 호출마다 갱신용
    s_phase = phase;

    auto void rainbow_px(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
        (void)y;
        // 16비트 누적 위상 → 상위 8비트만 hue로 사용
        const uint32_t acc = (uint32_t)s_phase + (uint32_t)per_pixel * (uint32_t)x;
        const uint8_t  hue = (uint8_t)(acc >> 8);
        wheel(hue, r,g,b);
    }

    build_from_pixel_fn(fb, rainbow_px);
}


// ---- 내부: 비트 읽기 헬퍼 ----
static inline bool small_get_px(const uint8_t bmp[SMALL_FONT_HEIGHT][1], int x, int y) {
    if ((unsigned)y >= SMALL_FONT_HEIGHT || (unsigned)x >= SMALL_FONT_WIDTH) return false;
    uint8_t byte = bmp[y][0];
    return (byte >> (7 - x)) & 1;
}
static inline bool big_get_px(const uint8_t bmp[BIG_FONT_HEIGHT][2], int x, int y) {
    if ((unsigned)y >= BIG_FONT_HEIGHT || (unsigned)x >= BIG_FONT_WIDTH) return false;
    const uint8_t byte = bmp[y][x >> 3];          // 0 or 1
    const uint8_t bit  = 7 - (x & 7);
    return (byte >> bit) & 1;
}

// ---- 내부: 숫자/콜론 포인터 선택 ----
static const uint8_t (*big_digit(int d))[2] {
    switch (d) {
        case 0: return big_bitmap_0;
        case 1: return big_bitmap_1;
        case 2: return big_bitmap_2;
        case 3: return big_bitmap_3;
        case 4: return big_bitmap_4;
        case 5: return big_bitmap_5;
        case 6: return big_bitmap_6;
        case 7: return big_bitmap_7;
        case 8: return big_bitmap_8;
        case 9: return big_bitmap_9;
        default: return NULL;
    }
}

// ---- 레이아웃(64x32 패널 기준) ----
// 상단 AM/PM: (x,y) = (24, 2) 부근 (네가 쓰던 값과 유사)
// 큰 시/분: y=16 부근에 16px 높이로 배치
typedef struct {
    int ax, ay;     // 'AM' 또는 'cP' 좌상단
    int hx0, hy0;   // 시(ten)
    int hx1, hy1;   // 시(one)
    int cx,  cy;    // 콜론
    int mx0, my0;   // 분(ten)
    int mx1, my1;   // 분(one)
} layout_t;

static layout_t make_layout(void) {
    layout_t L;
    L.ax = 25; L.ay = 2;

    // 가독성 위해 약간 좌우 간격을 조정한 배치
    L.hy0 = 16; L.hx0 =  2;                 // 시 10의 자리
    L.hy1 = 16; L.hx1 =  2 + BIG_FONT_WIDTH + 2; // 시 1의 자리

    L.cy  = 16; L.cx  =  L.hx1 + BIG_FONT_WIDTH + 3;  // 콜론 x
    L.my0 = 16; L.mx0 =  L.cx + 8 + 3;           // 분 10의 자리
    L.my1 = 16; L.mx1 =  L.mx0 + BIG_FONT_WIDTH + 2;
    return L;
}

// ---- 픽셀 콜백: 시계 한 프레임 ----
typedef struct {
    int hour, minute;
    bool is_am;
    uint8_t fr, fg, fb;   // foreground
    uint8_t br, bg, bb;   // background
    layout_t L;
} time_ctx_t;

static void time_pixel_cb(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b)
{
    static time_ctx_t ctx;           // build 호출 직전에 memcpy로 채워짐
    extern time_ctx_t __time_ctx_src;
    memcpy(&ctx, &__time_ctx_src, sizeof(ctx));

    // 기본 배경
    *r = ctx.br; *g = ctx.bg; *b = ctx.bb;

    // ===== 상단 am/pm =====
    if (x >= ctx.L.ax && x < ctx.L.ax + SMALL_FONT_WIDTH &&
        y >= ctx.L.ay && y < ctx.L.ay + SMALL_FONT_HEIGHT) {
        int sx = x - ctx.L.ax, sy = y - ctx.L.ay;
        bool on = false;
        if (ctx.is_am) {
            if (small_get_px(bitmap_A, sx, sy)) on = true;
        } else {
            // 'cP' + 'M' (네 bitmap_cP, bitmap_M 그대로 활용)
            if (small_get_px(bitmap_cP, sx, sy)) on = true;
        }
        if (on) {  // 전경색 적용
               *r = ctx.fr; *g = ctx.fg; *b = ctx.fb;
        }
    }
    // M (오른쪽 8px)
	if (x >= ctx.L.ax + SMALL_FONT_WIDTH && x < ctx.L.ax + 2*SMALL_FONT_WIDTH &&
		y >= ctx.L.ay && y < ctx.L.ay + SMALL_FONT_HEIGHT)
	{
		int sx = x - (ctx.L.ax + SMALL_FONT_WIDTH), sy = y - ctx.L.ay;
		if (small_get_px(bitmap_M, sx, sy)) {                // (x,y) 순서
			*r = ctx.fr; *g = ctx.fg; *b = ctx.fb;
		}
	}

    // ===== 큰 숫자/콜론 =====
    const int h10 = (ctx.hour   / 10) % 10;
    const int h01 = (ctx.hour   % 10);
    const int m10 = (ctx.minute / 10) % 10;
    const int m01 = (ctx.minute % 10);

    const uint8_t (*H10)[2] = big_digit(h10);
    const uint8_t (*H01)[2] = big_digit(h01);
    const uint8_t (*M10)[2] = big_digit(m10);
    const uint8_t (*M01)[2] = big_digit(m01);

    // 시 10의 자리
    if (x >= ctx.L.hx0 && x < ctx.L.hx0 + BIG_FONT_WIDTH &&
        y >= ctx.L.hy0 && y < ctx.L.hy0 + BIG_FONT_HEIGHT && H10) {
        const int sx = x - ctx.L.hx0, sy = y - ctx.L.hy0;
        if (big_get_px(H10, sx, sy)) { *r = ctx.fr; *g = ctx.fg; *b = ctx.fb; return; }
    }
    // 시 1의 자리
    if (x >= ctx.L.hx1 && x < ctx.L.hx1 + BIG_FONT_WIDTH &&
        y >= ctx.L.hy1 && y < ctx.L.hy1 + BIG_FONT_HEIGHT && H01) {
        const int sx = x - ctx.L.hx1, sy = y - ctx.L.hy1;
        if (big_get_px(H01, sx, sy)) { *r = ctx.fr; *g = ctx.fg; *b = ctx.fb; return; }
    }
    // 콜론(폭 16이지만 실제 점은 중앙 근처)
    if (x >= ctx.L.cx && x < ctx.L.cx + BIG_FONT_WIDTH &&
        y >= ctx.L.cy && y < ctx.L.cy + BIG_FONT_HEIGHT) {
        const int sx = x - ctx.L.cx, sy = y - ctx.L.cy;
        if (big_get_px(big_bitmap_COLON, sx, sy)) { *r = ctx.fr; *g = ctx.fg; *b = ctx.fb; return; }
    }
    // 분 10의 자리
    if (x >= ctx.L.mx0 && x < ctx.L.mx0 + BIG_FONT_WIDTH &&
        y >= ctx.L.my0 && y < ctx.L.my0 + BIG_FONT_HEIGHT && M10) {
        const int sx = x - ctx.L.mx0, sy = y - ctx.L.my0;
        if (big_get_px(M10, sx, sy)) { *r = ctx.fr; *g = ctx.fg; *b = ctx.fb; return; }
    }
    // 분 1의 자리
    if (x >= ctx.L.mx1 && x < ctx.L.mx1 + BIG_FONT_WIDTH &&
        y >= ctx.L.my1 && y < ctx.L.my1 + BIG_FONT_HEIGHT && M01) {
        const int sx = x - ctx.L.mx1, sy = y - ctx.L.my1;
        if (big_get_px(M01, sx, sy)) { *r = ctx.fr; *g = ctx.fg; *b = ctx.fb; return; }
    }
}

// 빌더가 static 콜백만 받으므로, 컨텍스트를 전역 복사로 넘김
time_ctx_t __time_ctx_src;

void paint_time_frame(uint32_t *fb,
                      int hour, int minute, bool is_am,
                      uint8_t fr, uint8_t fg, uint8_t fbk,
                      uint8_t br, uint8_t bg, uint8_t bb)
{
    time_ctx_t ctx;
    ctx.hour   = hour;
    ctx.minute = minute;
    ctx.is_am  = is_am;
    ctx.fr = fr; ctx.fg = fg; ctx.fb = fbk;
    ctx.br = br; ctx.bg = bg; ctx.bb = bb;
    ctx.L  = make_layout();

    memcpy(&__time_ctx_src, &ctx, sizeof(ctx));
    build_from_pixel_fn(fb, time_pixel_cb);
}
