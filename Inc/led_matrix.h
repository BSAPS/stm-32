#ifndef __LED_MATRIX_H
#define __LED_MATRIX_H

#include "main.h"  // Required for HAL and GPIO pin definitions

#ifdef __cplusplus
extern "C" {
#endif

// === Public Interface for LED Matrix Control ===

extern uint16_t upper_buffer[16];
extern uint16_t lower_buffer[16];

typedef enum {
    COLOR_BLACK = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_WHITE
} LEDColor;


/**
 * @brief Initialize the LED matrix.
 *        Sets OE pin HIGH to turn off display initially.
 *        Also loads a default frame (e.g., all yellow).
 */
void LEDMatrix_Init(void);

/**
 * @brief Turn on the LED matrix display.
 */
void LEDMatrix_TurnOn(void);
void LEDMatrix_SetColor(LEDColor color);

/**
 * @brief Turn off the LED matrix display by setting OE HIGH.
 */
void LEDMatrix_TurnOff(void);

/**
 * @brief Update the current LED matrix frame.
 *        Call this periodically in the main loop or timer interrupt.
 */
void LEDMatrix_Update(void);

/**
 * @brief Set a new frame for the LED matrix.
 * 
 * @param upper 16 rows for the top half (R1, G1, B1)
 * @param lower 16 rows for the bottom half (R2, G2, B2)
 */
void LEDMatrix_SetFrame(uint16_t upper[16], uint16_t lower[16]);

#ifdef __cplusplus
}
#endif

#endif // __LED_MATRIX_H
