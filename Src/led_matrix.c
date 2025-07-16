/* Includes ------------------------------------------------------------------*/
#include "led_matrix.h"
#include "main.h"

#define HIGH GPIO_PIN_SET
#define LOW  GPIO_PIN_RESET
#define HUB75_WIDTH   64
#define HUB75_HEIGHT  32

/* ------------------- 프레임 버퍼 ------------------- */
uint8_t rgb_framebuffer[HUB75_HEIGHT][HUB75_WIDTH][3] = {0};  // [row][col][RGB]

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

            HAL_GPIO_WritePin(mat_pins.R1.port, mat_pins.R1.pin, r1);
            HAL_GPIO_WritePin(mat_pins.G1.port, mat_pins.G1.pin, g1);
            HAL_GPIO_WritePin(mat_pins.B1.port, mat_pins.B1.pin, b1);
            HAL_GPIO_WritePin(mat_pins.R2.port, mat_pins.R2.pin, r2);
            HAL_GPIO_WritePin(mat_pins.G2.port, mat_pins.G2.pin, g2);
            HAL_GPIO_WritePin(mat_pins.B2.port, mat_pins.B2.pin, b2);

            pulse(mat_pins.CLK.port, mat_pins.CLK.pin);
        }

        pulse(mat_pins.LAT.port, mat_pins.LAT.pin);
        HAL_GPIO_WritePin(mat_pins.OE.port, mat_pins.OE.pin, GPIO_PIN_RESET);

        HAL_Delay(1);  // 간단한 프레임 유지 시간
    }
}

void LEDMatrix_TurnOn(void) {
    clearBuffer();

    // 사람 아이콘 (간단한 형태)
    setPixel(13, 31, 1, 1, 1);  // 머리
    setPixel(14, 31, 1, 1, 1);  // 몸통
    setPixel(15, 30, 1, 1, 1);  // 왼팔
    setPixel(15, 32, 1, 1, 1);  // 오른팔
	
}

void LEDMatrix_TurnOff(void) {
    clearBuffer();

    // 숫자 3 예시 (붉은색)
    for (int i = 20; i < 44; i++) {
        setPixel(10, i, 1, 0, 0); // 윗선
        setPixel(16, i, 1, 0, 0); // 아랫선
    }
    for (int i = 11; i < 17; i++) {
        setPixel(i, 44, 1, 0, 0); // 우측 기둥
    }
    setPixel(13, 35, 1, 0, 0);  // 가운데 점
}
