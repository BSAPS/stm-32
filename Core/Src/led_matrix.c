/* Includes ------------------------------------------------------------------*/
#include "led_matrix.h"
#include "main.h"
#include "led_font.h"

#define HIGH GPIO_PIN_SET
#define LOW  GPIO_PIN_RESET
#define HUB75_WIDTH   64
#define HUB75_HEIGHT  32
#define FONT_WIDTH  14
#define FONT_HEIGHT 16
//#define FONT_WIDTH_3B 17
//#define FONT_HEIGHT_3B 24

extern RTC_HandleTypeDef hrtc;
extern uint8_t led_enabled;

/* ------------------- 프레임 버퍼 ------------------- */
uint8_t rgb_framebuffer[HUB75_HEIGHT][HUB75_WIDTH][3] = {0};  // [row][col][RGB]

LED_DisplayState led_state = LED_STATE_CLOCK;

/* ----=== GPIO pin mapping ===--------- */
HUB75_Pins mat_pins = {
    .R1  = {GPIOB, GPIO_PIN_0},
    .G1  = {GPIOB, GPIO_PIN_1},
    .B1  = {GPIOB, GPIO_PIN_2},
    .R2  = {GPIOB, GPIO_PIN_3},  // 추가 정의 필요
    .G2  = {GPIOB, GPIO_PIN_4},
    .B2  = {GPIOB, GPIO_PIN_5},
    .CLK = {GPIOA, GPIO_PIN_4},
    .LAT = {GPIOB, GPIO_PIN_9},
    .OE  = {GPIOB, GPIO_PIN_8},
    .A   = {GPIOC, GPIO_PIN_0},
    .B   = {GPIOC, GPIO_PIN_1},
    .C   = {GPIOC, GPIO_PIN_2},
    .D   = {GPIOC, GPIO_PIN_3},
};


// ==== STOP NOW ! ==== (Pixel M)

const uint8_t bitmap_S[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x00,0x00}, {0x0F,0xB0}, {0x1F,0xF0},
    {0x38,0x70}, {0x30,0x30}, {0x38,0x00}, {0x1F,0x80},
    {0x07,0xE0}, {0x00,0x70}, {0x30,0x30}, {0x38,0x70},
    {0x3F,0xE0}, {0x37,0xC0}, {0x00,0x00}, {0x00,0x00}
};

const uint8_t bitmap_T[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x00,0x00}, {0x3F,0xF0}, {0x3F,0xF0},
    {0x33,0x30}, {0x33,0x30}, {0x33,0x30}, {0x03,0x00},
    {0x03,0x00}, {0x03,0x00}, {0x03,0x00}, {0x03,0x00},
    {0x0F,0xC0}, {0x0F,0xC0}, {0x00,0x00}, {0x00,0x00}
};

const uint8_t bitmap_O[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x00,0x00}, {0x07,0x80}, {0x0F,0xC0},
    {0x1C,0xE0}, {0x38,0x70}, {0x30,0x30}, {0x30,0x30},
    {0x30,0x30}, {0x30,0x30}, {0x38,0x70}, {0x1C,0xE0},
    {0x0F,0xC0}, {0x07,0x80}, {0x00,0x00}, {0x00,0x00}
};

/// stop! -p <-> PM ::: led_font.c
const uint8_t bitmap_P[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x00,0x00}, {0x3F,0xC0}, {0x3F,0xE0},
    {0x18,0x70}, {0x18,0x30}, {0x18,0x30}, {0x18,0x70},
    {0x1F,0xE0}, {0x1F,0xC0}, {0x18,0x00}, {0x18,0x00},
    {0x3F,0x00}, {0x3F,0x00}, {0x00,0x00}, {0x00,0x00}
};

const uint8_t bitmap_N[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x00,0x00}, {0x39,0xF0}, {0x3D,0xF0},
    {0x1C,0x60}, {0x1E,0x60}, {0x1E,0x60}, {0x1B,0x60},
    {0x1B,0x60}, {0x19,0xE0}, {0x19,0xE0}, {0x18,0xE0},
    {0x3E,0xE0}, {0x3E,0x60}, {0x00,0x00}, {0x00,0x00}
};

const uint8_t bitmap_W[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x00,0x00}, {0x7C,0x7C}, {0x7C,0x7C},
    {0x30,0x18}, {0x33,0x98}, {0x33,0x98}, {0x33,0x98},
    {0x36,0xD8}, {0x16,0xD0}, {0x1C,0x70}, {0x1C,0x70},
    {0x1C,0x70}, {0x18,0x30}, {0x00,0x00}, {0x00,0x00}
};

const uint8_t bitmap_EXCL[FONT_HEIGHT][2] = {
    {0x00,0x00}, {0x07,0x00}, {0x07,0x00}, {0x07,0x00},
    {0x07,0x00}, {0x07,0x00}, {0x07,0x00}, {0x07,0x00},
    {0x02,0x00}, {0x02,0x00}, {0x00,0x00}, {0x00,0x00},
    {0x07,0x00}, {0x07,0x00}, {0x00,0x00}, {0x00,0x00}
};

//GO! (Pixel L)
const uint8_t G_bitmap[24][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0x03, 0xEC, 0x00}, {0x0F, 0xFC, 0x00}, {0x1C, 0x1C, 0x00},
    {0x18, 0x0C, 0x00}, {0x30, 0x0C, 0x00}, {0x30, 0x00, 0x00},
    {0x30, 0x00, 0x00}, {0x30, 0xFE, 0x00}, {0x30, 0xFE, 0x00},
    {0x30, 0x0C, 0x00}, {0x38, 0x0C, 0x00}, {0x1C, 0x1C, 0x00},
    {0x0F, 0xFC, 0x00}, {0x03, 0xF0, 0x00}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

const uint8_t O_bitmap[24][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0x03, 0xC0, 0x00}, {0x0F, 0xF0, 0x00}, {0x1C, 0x38, 0x00},
    {0x18, 0x18, 0x00}, {0x38, 0x1C, 0x00}, {0x30, 0x0C, 0x00},
    {0x30, 0x0C, 0x00}, {0x30, 0x0C, 0x00}, {0x30, 0x0C, 0x00},
    {0x38, 0x1C, 0x00}, {0x18, 0x18, 0x00}, {0x1C, 0x38, 0x00},
    {0x0F, 0xF0, 0x00}, {0x03, 0xC0, 0x00}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

const uint8_t EXCL_bitmap[24][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x03, 0x80, 0x00},
    {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00},
    {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00},
    {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00},
    {0x01, 0x00, 0x00}, {0x01, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x03, 0x80, 0x00}, {0x03, 0x80, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* ------------------- 유틸리티 함수 ------------------- */
void pulse(GPIO_TypeDef* port, uint16_t pin) {
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

void set_row_address(uint8_t row) {
    HAL_GPIO_WritePin(mat_pins.A.port, mat_pins.A.pin, (row >> 0) & 0x01);
    HAL_GPIO_WritePin(mat_pins.B.port, mat_pins.B.pin, (row >> 1) & 0x01);
    HAL_GPIO_WritePin(mat_pins.C.port, mat_pins.C.pin, (row >> 2) & 0x01);
    HAL_GPIO_WritePin(mat_pins.D.port, mat_pins.D.pin, (row >> 3) & 0x01);
}

void setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
    if (row < HUB75_HEIGHT && col < HUB75_WIDTH) {
        rgb_framebuffer[row][col][0] = r;
        rgb_framebuffer[row][col][1] = g;
        rgb_framebuffer[row][col][2] = b;
    }
}

void clearBuffer(void) {
    memset(rgb_framebuffer, 0, sizeof(rgb_framebuffer));
}

/* ------------------- 화면 갱신 ------------------- */
void HUB75_UpdateScreen(void) {
    for (uint8_t row = 0; row < HUB75_HEIGHT / 2; row++) {
        set_row_address(row);

        HAL_GPIO_WritePin(mat_pins.OE.port, mat_pins.OE.pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(mat_pins.LAT.port, mat_pins.LAT.pin, GPIO_PIN_RESET);

        for (uint8_t col = 0; col < HUB75_WIDTH; col++) {
            uint8_t r1 = rgb_framebuffer[row][col][0];
            uint8_t g1 = rgb_framebuffer[row][col][1];
            uint8_t b1 = rgb_framebuffer[row][col][2];
            uint8_t r2 = rgb_framebuffer[row + HUB75_HEIGHT / 2][col][0];
            uint8_t g2 = rgb_framebuffer[row + HUB75_HEIGHT / 2][col][1];
            uint8_t b2 = rgb_framebuffer[row + HUB75_HEIGHT / 2][col][2];

            HAL_GPIO_WritePin(mat_pins.R1.port, mat_pins.R1.pin, r1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(mat_pins.G1.port, mat_pins.G1.pin, g1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(mat_pins.B1.port, mat_pins.B1.pin, b1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(mat_pins.R2.port, mat_pins.R2.pin, r2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(mat_pins.G2.port, mat_pins.G2.pin, g2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(mat_pins.B2.port, mat_pins.B2.pin, b2 ? GPIO_PIN_SET : GPIO_PIN_RESET);

            pulse(mat_pins.CLK.port, mat_pins.CLK.pin);
        }

        pulse(mat_pins.LAT.port, mat_pins.LAT.pin);
        HAL_GPIO_WritePin(mat_pins.OE.port, mat_pins.OE.pin, GPIO_PIN_RESET);

        HAL_Delay(0.1);
    }
}


void LEDMatrix_TurnOn(void) {
    static uint32_t last_tick = 0;

    if (!led_enabled) {
        led_state = LED_STATE_CLOCK;
        return;
    }

    uint32_t now = HAL_GetTick();
    if (now - last_tick >= 2000) {  // 2초마다 전환
        last_tick = now;

        if (led_state == LED_STATE_STOP)
            led_state = LED_STATE_MARIO;
        else
            led_state = LED_STATE_STOP;
    }
}


//void LEDMatrix_TurnOn(void) {
//    static uint32_t last_tick = 0;
//    static int state = 0;
//
//    if (!led_enabled) return;
//
//    uint32_t now = HAL_GetTick();
//    if (now - last_tick < 1000) return;
//    last_tick = now;
//
//    clearBuffer();
//    if (state == 0) {
//        drawStopNow();
//    } else {
//        drawMario(0, 16);
//    }
//    state = 1 - state;
//}

void LEDMatrix_TurnOff(void) {
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    const char* ampm;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    ampm = (sTime.TimeFormat == RTC_HOURFORMAT12_AM) ? "AM" : "PM";

    clearBuffer();  // 기존 화면 초기화
    drawClockTime(sTime.Hours, sTime.Minutes, ampm);
}




void drawCharBitmap(uint8_t row, uint8_t col, const uint8_t bitmap[][2]) {
    for (uint8_t r = 0; r < FONT_HEIGHT; r++) {
        for (uint8_t c = 0; c < FONT_WIDTH; c++) {
            uint8_t byte_index = c / 8;
            uint8_t bit_index = 7 - (c % 8);
            if (bitmap[r][byte_index] & (1 << bit_index)) {
                setPixel(row + r, col + c, 1, 0, 0);  // 빨간색
            }
        }
    }
}

//GO! Bitmap
void drawCharBitmap3B(uint8_t row, uint8_t col, const uint8_t bitmap[][3]) {
    for (uint8_t r = 0; r < 24; r++) {
        for (uint8_t c = 0; c < 17; c++) {  // 3바이트 → 24비트
            uint8_t byte_index = c / 8;
            uint8_t bit_index = 7 - (c % 8);
            if (bitmap[r][byte_index] & (1 << bit_index)) {
                setPixel(row + r, col + c, 0, 1, 0);  // green
            }
        }
    }
}


//------------time---------------
void drawDigit(uint8_t digit, int y, int x) {
    const uint8_t (*bmp)[2] = NULL;
    switch (digit) {
        case 0: bmp = big_bitmap_0; break;
        case 1: bmp = big_bitmap_1; break;
        case 2: bmp = big_bitmap_2; break;
        case 3: bmp = big_bitmap_3; break;
        case 4: bmp = big_bitmap_4; break;
        case 5: bmp = big_bitmap_5; break;
        case 6: bmp = big_bitmap_6; break;
        case 7: bmp = big_bitmap_7; break;
        case 8: bmp = big_bitmap_8; break;
        case 9: bmp = big_bitmap_9; break;
        default: return;
    }

    for (int row = 0; row < BIG_FONT_HEIGHT; ++row) {
        for (int col = 0; col < BIG_FONT_WIDTH; ++col) {
            if ((bmp[row][col / 8] >> (7 - (col % 8))) & 0x01) {
                setPixel(y + row, x + col+2, 1, 1, 0);
            } else {
                setPixel(y + row, x + col+2, 0, 0, 0);
            }
        }
    }
}

void drawClockTime(uint8_t hour, uint8_t minute, const char* ampm) {
	if (ampm[0] == 'A') {
	    for (int row = 0; row < SMALL_FONT_HEIGHT; ++row) {
	        for (int col = 0; col < SMALL_FONT_WIDTH; ++col) {
	            if ((bitmap_A[row][0] >> (7 - col)) & 0x01)
	                setPixel(2 + row, 25 + col, 1, 1, 0);
	            if ((bitmap_M[row][0] >> (7 - col)) & 0x01)
	                setPixel(2 + row, 33 + col, 1, 1, 0);
	        	}
	    	}
		} else {
			for (int row = 0; row < SMALL_FONT_HEIGHT; ++row) {
				for (int col = 0; col < SMALL_FONT_WIDTH; ++col) {
					if ((bitmap_cP[row][0] >> (7 - col)) & 0x01)
						setPixel(2 + row, 25 + col, 1, 1, 0);
					if ((bitmap_M[row][0] >> (7 - col)) & 0x01)
						setPixel(2 + row, 33 + col, 1, 1, 0);
				}
			}
		}


    drawDigit(hour / 10, 16, 0);
    drawDigit(hour % 10, 16, 12);

    for (int row = 0; row < BIG_FONT_HEIGHT; ++row) {
        for (int col = 0; col < BIG_FONT_WIDTH; ++col) {
            if ((big_bitmap_COLON[row][col / 8] >> (7 - (col % 8))) & 0x01)
                setPixel(16 + row, 27 + col, 1, 1, 0);
            else
                setPixel(16 + row, 27 + col, 0, 0, 0);
        }
    }

    drawDigit(minute / 10, 16, 38);
    drawDigit(minute % 10, 16, 50);

}

// ==== 메시지 표시 ====
void drawStopNow(void) {
    clearBuffer();

    uint8_t row1 = 0;
    uint8_t row2 = 16;
    uint8_t spacing = 2;
    uint8_t charWidth = FONT_WIDTH;
    uint8_t totalWidth = charWidth * 4 + spacing * 3; // 56
    uint8_t startCol = (HUB75_WIDTH - totalWidth) / 2;  // 4

    // Line 1: S T O P
    drawCharBitmap(row1, startCol + (charWidth + spacing) * 0, bitmap_S);
    drawCharBitmap(row1, startCol + (charWidth + spacing) * 1, bitmap_T);
    drawCharBitmap(row1, startCol + (charWidth + spacing) * 2, bitmap_O);
    drawCharBitmap(row1, startCol + (charWidth + spacing) * 3, bitmap_P);

    // Line 2:   N O W !
    drawCharBitmap(row2, startCol + (charWidth + spacing) * 0, bitmap_N);
    drawCharBitmap(row2, startCol + (charWidth + spacing) * 1, bitmap_O);
    drawCharBitmap(row2, startCol + (charWidth + spacing) * 2, bitmap_W);
    drawCharBitmap(row2, startCol + (charWidth + spacing) * 3, bitmap_EXCL);
}

void drawGo(void) {
    clearBuffer();

    uint8_t spacing = 1;
    uint8_t start_row = 0;
    uint8_t total_width = 17 * 3 + spacing * 2;  // 17*3 + 2 = 53
    uint8_t start_col = (64 - total_width) / 2; // 5

    drawCharBitmap3B(start_row, start_col + (17 + spacing) * 0, G_bitmap);          // G
    drawCharBitmap3B(start_row, start_col + (17 + spacing) * 1, O_bitmap);     // O
    drawCharBitmap3B(start_row, start_col + (17 + spacing) * 2, EXCL_bitmap); // !
}


void drawMario(uint8_t start_row, uint8_t start_col) {
    for (uint8_t r = 0; r < 32; r++) {
        for (uint8_t c = 0; c < 32; c++) {
            uint8_t color = marioBits[r][c];

            uint8_t red = 0, green = 0, blue = 0;

            switch (color) {
                case 0x00: red = 0; green = 0; blue = 0; break;  // 검정
                case 0x01: red = 0; green = 0; blue = 1; break;  // 파랑
                case 0x04: red = 1; green = 0; blue = 0; break;  // 빨강
                case 0x06: red = 1; green = 1; blue = 0; break;  // 노랑
                case 0x07: red = 1; green = 1; blue = 1; break;  // 흰색
                default:   red = 0; green = 0; blue = 0; break;  // 예외: 검정 처리
            }

            setPixel(start_row + r, start_col + c, red, green, blue);
        }
    }
}

