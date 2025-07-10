#include "led_matrix.h"
#include <string.h>  // For memcpy

// === Internal State ===
static volatile uint8_t matrix_on = 0;
uint16_t upper_buffer[16];
uint16_t lower_buffer[16];

static LEDColor current_color;
#define OE_ON_DURATION 200


// === Test Pattern (for initial visual check) ===
uint16_t upper[16] = {
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,

    0x10C4,
    0x13C4,
    0x17C4,
    0x27E4,
    0x1FF0,
    0x1FF0,
    0x1FF0,
    0x1FF0
};

uint16_t lower[16] = {
    0x1FF0,
    0x1FF0,
    0x1FF0,
    0x1FF0,
    0x1FF0,
    0x2008,
    0x2008,
    0x2008,

    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

// RGB bit representation
typedef struct {
    uint8_t r : 1;
    uint8_t g : 1;
    uint8_t b : 1;
} RGBBits;

// Color map table
static const RGBBits color_map[] = {
    [COLOR_BLACK]   = {0, 0, 0},
    [COLOR_RED]     = {1, 0, 0},
    [COLOR_GREEN]   = {0, 1, 0},
    [COLOR_BLUE]    = {0, 0, 1},
    [COLOR_YELLOW]  = {1, 1, 0},
    [COLOR_CYAN]    = {0, 1, 1},
    [COLOR_MAGENTA] = {1, 0, 1},
    [COLOR_WHITE]   = {1, 1, 1}
};

// === Internal Functions ===

/**
 * @brief Shifts out a single row of upper and lower RGB data to the LED matrix.
 */
void shiftRowColor(uint16_t pattern, LEDColor color)
{
    RGBBits c = color_map[color];

    for (int i = 0; i < 16; i++) {
        uint8_t bit = (pattern >> i) & 0x01;

        HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, (bit && c.r) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(G1_GPIO_Port, G1_Pin, (bit && c.g) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(B1_GPIO_Port, B1_Pin, (bit && c.b) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, (bit && c.r) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(G2_GPIO_Port, G2_Pin, (bit && c.g) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(B2_GPIO_Port, B2_Pin, (bit && c.b) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        HAL_GPIO_WritePin(CLK_GPIO_Port, CLK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CLK_GPIO_Port, CLK_Pin, GPIO_PIN_RESET);
    }
}


static void shiftRow(uint16_t rowPatternUpper, uint16_t rowPatternLower) {
    for (int i = 0; i < 16; i++) {
        // Upper (R1, G1), no B1 for yellow color
        HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, (rowPatternUpper >> i) & 0x01);
        HAL_GPIO_WritePin(G1_GPIO_Port, G1_Pin, (rowPatternUpper >> i) & 0x01);
        HAL_GPIO_WritePin(B1_GPIO_Port, B1_Pin, (rowPatternUpper >> i) & 0x01);

        // Lower (R2, G2), no B2
        HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, (rowPatternLower >> i) & 0x01);
        HAL_GPIO_WritePin(G2_GPIO_Port, G2_Pin, (rowPatternLower >> i) & 0x01);
        HAL_GPIO_WritePin(B2_GPIO_Port, B2_Pin, (rowPatternLower >> i) & 0x01);

        // Clock pulse
        HAL_GPIO_WritePin(CLK_GPIO_Port, CLK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CLK_GPIO_Port, CLK_Pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief Displays a row on the LED matrix by setting address and shifting RGB data.
 */


static void displayRow(int rowIndex, uint16_t upper, uint16_t lower) {
    // Set address (A-E)
    HAL_GPIO_WritePin(A_GPIO_Port, A_Pin, (rowIndex >> 0) & 0x01);
    HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, (rowIndex >> 1) & 0x01);
    HAL_GPIO_WritePin(C_GPIO_Port, C_Pin, (rowIndex >> 2) & 0x01);
    HAL_GPIO_WritePin(D_GPIO_Port, D_Pin, (rowIndex >> 3) & 0x01);
    HAL_GPIO_WritePin(E_GPIO_Port, E_Pin, (rowIndex >> 4) & 0x01);

    // === NEW: Merge into one shift ===
    for (int i = 0; i < 16; i++) {
        uint8_t bitU = (upper >> i) & 0x01;
        uint8_t bitL = (lower >> i) & 0x01;

        HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, (bitU && (current_color == COLOR_RED || current_color == COLOR_YELLOW || current_color == COLOR_MAGENTA || current_color == COLOR_WHITE)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(G1_GPIO_Port, G1_Pin, (bitU && (current_color == COLOR_GREEN || current_color == COLOR_YELLOW || current_color == COLOR_CYAN || current_color == COLOR_WHITE)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(B1_GPIO_Port, B1_Pin, (bitU && (current_color == COLOR_BLUE || current_color == COLOR_CYAN || current_color == COLOR_MAGENTA || current_color == COLOR_WHITE)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, (bitL && (current_color == COLOR_RED || current_color == COLOR_YELLOW || current_color == COLOR_MAGENTA || current_color == COLOR_WHITE)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(G2_GPIO_Port, G2_Pin, (bitL && (current_color == COLOR_GREEN || current_color == COLOR_YELLOW || current_color == COLOR_CYAN || current_color == COLOR_WHITE)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(B2_GPIO_Port, B2_Pin, (bitL && (current_color == COLOR_BLUE || current_color == COLOR_CYAN || current_color == COLOR_MAGENTA || current_color == COLOR_WHITE)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        HAL_GPIO_WritePin(CLK_GPIO_Port, CLK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CLK_GPIO_Port, CLK_Pin, GPIO_PIN_RESET);
    }

    // Latch
    HAL_GPIO_WritePin(LAT_GPIO_Port, LAT_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LAT_GPIO_Port, LAT_Pin, GPIO_PIN_RESET);

    // Output enable
    HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
    for (volatile int i = 0; i < OE_ON_DURATION; i++);
    HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
}


/**
 * @brief Displays a full frame (16 rows) using the upper and lower buffers.
 */
static void displayFrame(uint16_t upperRows[16], uint16_t lowerRows[16]) {
    for (int row = 0; row < 16; row++) {
        displayRow(row, upperRows[row], lowerRows[row]);
    }
}

// === Public Interface ===

/**
 * @brief Initializes the LED matrix system. Turns display off and loads a test pattern.
 */
void LEDMatrix_Init(void) {
    HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);  // Turn off display
    matrix_on = 0;

    // Load default frame (yellow fill)
    for (int i = 0; i < 16; i++) {
        upper_buffer[i] = 0xFFFF;
        lower_buffer[i] = 0xFFFF;
    }
}

/**
 * @brief Turns on the LED matrix (enables display updates).
 */
void LEDMatrix_TurnOn(void) {
    matrix_on = 1;
}


void LEDMatrix_SetColor(LEDColor color) {
    current_color = color;
}


/**
 * @brief Turns off the LED matrix (disables display output).
 */
void LEDMatrix_TurnOff(void) {
    matrix_on = 0;
    HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);  // Disable output
}

/**
 * @brief Updates the LED matrix display by writing the current frame buffer.
 *        Should be called repeatedly in the main loop or a timer.
 */
void LEDMatrix_Update(void) {
    if (!matrix_on) return;
    displayFrame(upper_buffer, lower_buffer);
}

/**
 * @brief Sets a new frame buffer for the LED matrix.
 * 
 * @param upper 16 lines of upper (R1, G1) data
 * @param lower 16 lines of lower (R2, G2) data
 */
void LEDMatrix_SetFrame(uint16_t upper[16], uint16_t lower[16]) {
    memcpy(upper_buffer, upper, sizeof(uint16_t) * 16);
    memcpy(lower_buffer, lower, sizeof(uint16_t) * 16);
}



/* interrupt method*/
extern TIM_HandleTypeDef htim10;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    static uint8_t current_row = 0;

    if (htim->Instance == TIM10) {
        if (matrix_on) {
            displayRow(current_row, upper_buffer[current_row], lower_buffer[current_row]);
            current_row = (current_row + 1) % 16;
        } else {
            HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);  // Turn OFF
        }
    }
}