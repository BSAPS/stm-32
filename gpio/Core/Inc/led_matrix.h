#ifndef __LED_MATRIX_H
#define __LED_MATRIX_H

#include "main.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "led_font.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HIGH GPIO_PIN_SET
#define LOW  GPIO_PIN_RESET

#define HUB75_WIDTH   64
#define HUB75_HEIGHT  32

/* ------------------- 프레임 버퍼 ------------------- */
extern uint8_t rgb_framebuffer[HUB75_HEIGHT][HUB75_WIDTH][3];  // [row][col][RGB]
//6kb
/* ------------------- 핀 구조 정의 ------------------- */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} GPIO_Pin;

typedef struct {
    GPIO_Pin R1, G1, B1;
    GPIO_Pin R2, G2, B2;
    GPIO_Pin CLK, LAT, OE;
    GPIO_Pin A, B, C, D;
} HUB75_Pins;


typedef enum {
    LED_STATE_CLOCK,
    LED_STATE_STOP,
    LED_STATE_MARIO
} LED_DisplayState;

extern LED_DisplayState led_state;

// === extern ===
extern uint8_t rgb_framebuffer[HUB75_HEIGHT][HUB75_WIDTH][3];
extern HUB75_Pins mat_pins;

// === HUB75 function ===
void pulse(GPIO_TypeDef* port, uint16_t pin);
void set_row_address(uint8_t row);
void setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
void clearBuffer(void);
void UpdateMatrix(void);


void drawDigit(uint8_t digit, int y, int x);
void drawClockTime(uint8_t hour, uint8_t minute, const char* ampm);
// === power on/off ===
void LEDMatrix_TurnOn(void);
void LEDMatrix_TurnOff(void);
void drawStopNow(void);
void drawMario(uint8_t start_row, uint8_t start_col);
void drawGo(void);
#ifdef __cplusplus
}
#endif

#endif // __LED_MATRIX_H
