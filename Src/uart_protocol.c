// uart_protocol.c

#include "uart_protocol.h"
#include "led_matrix.h"
#include "string.h"
#include "stdio.h"

uint8_t rx_byte = 0;
uint16_t rx_index = 0;
uint8_t rx_buffer[RX_BUFFER_SIZE] = {0};


// Callback function triggered when a UART byte is received (Interrupt)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        FSM_ParseByte(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // Continue receiving next byte
    }
}

// Reverse the bit order of a value (used in CRC computation)
unsigned short reverse_value(unsigned short value, int bit) {
    unsigned short ret = 0;
    for (int i = 0; i < bit; i++) {
        ret = (ret << 1) | ((value >> i) & 1);
    }
    return ret;
}

// Calculate 16-bit CRC using polynomial 0x8005
unsigned short crc16_calc(uint8_t *pData, int length) {
    unsigned short crc = 0x0000;
    while (length--) {
        if (IO_REF == 0)
            crc ^= *(unsigned char *)pData++ << 8;
        else
            crc ^= reverse_value(*pData++, 8) << 8;

        for (int i = 0; i < 8; i++) {
            crc = crc & 0x8000 ? (crc << 1) ^ 0x8005 : crc << 1;
        }
    }

    if (IO_REF == 1)
        crc = reverse_value(crc, 16);

    return crc ^ 0x0000;
}

// Send a framed UART command with DLE-STX/ETX framing and CRC
void UART_SendFrame(uint8_t cmd) {
    uint8_t start[] = {DLE, STX};
    uint8_t end[] = {DLE, ETX};
    uint8_t frame[3]; // CMD + CRC(2 bytes)

    frame[0] = cmd;
    unsigned short crc = crc16_calc(&cmd, 1);
    frame[1] = (crc >> 8) & 0xFF;
    frame[2] = crc & 0xFF;

    HAL_UART_Transmit(&huart1, start, 2, HAL_MAX_DELAY);

    for (int i = 0; i < 3; i++) {
        if (frame[i] == DLE) {
            uint8_t dle_escape[] = {DLE, DLE};
            HAL_UART_Transmit(&huart1, dle_escape, 2, HAL_MAX_DELAY);
        } else {
            HAL_UART_Transmit(&huart1, &frame[i], 1, HAL_MAX_DELAY);
        }
    }

    HAL_UART_Transmit(&huart1, end, 2, HAL_MAX_DELAY);
}

// UART receive FSM for parsing framed protocol data byte-by-byte
void FSM_ParseByte(uint8_t rx) {
    static uint8_t dle_flag = 0;
    static uint8_t receiving = 0;

    if (!receiving) {
        if (dle_flag && rx == STX) {
            rx_index = 0;
            receiving = 1;
            dle_flag = 0;
        } else if (rx == DLE) {
            dle_flag = 1;
        } else {
            dle_flag = 0;
        }
    } else {
        if (dle_flag) {
            if (rx == ETX) {
                ProcessCommand(rx_buffer, rx_index);
                receiving = 0;
                rx_index = 0;
            } else if (rx == DLE) {
                if (rx_index < RX_BUFFER_SIZE) rx_buffer[rx_index++] = DLE;
            }
            dle_flag = 0;
        } else if (rx == DLE) {
            dle_flag = 1;
        } else {
            if (rx_index < RX_BUFFER_SIZE) rx_buffer[rx_index++] = rx;
        }
    }
}

// Process a fully received command frame
void ProcessCommand(uint8_t *data, uint16_t len) {
    if (len < 4) {
        char *msg = "Invalid Frame Length\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
        return;
    }

    uint8_t dst_mask = data[0];
    uint8_t cmd = data[1];
    uint16_t recv_crc = (data[2] << 8) | data[3];
    uint16_t calc_crc = crc16_calc(&data[0], 2);

    if (recv_crc != calc_crc) {
        char *msg = "CRC Error\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
        UART_SendFrame(CMD_NACK); // Send NACK if CRC fails
        return;
    }

    if ((dst_mask >> (MY_ID - 1)) & 0x01) {
        switch (cmd) {
            case CMD_LCD_ON:
                HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
                LEDMatrix_TurnOn();
                {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "[Board %d] LD2 ON\r\n", MY_ID);
                    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
                    UART_SendFrame(CMD_ACK);
                }
                break;
            case CMD_LCD_OFF:
                HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
                LEDMatrix_TurnOff();
                {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "[Board %d] LD2 OFF\r\n", MY_ID);
                    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
                    UART_SendFrame(CMD_ACK);
                }
                break;
            default:
                {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "[Board %d] Unknown CMD\r\n", MY_ID);
                    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
                }
                break;
        }
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "[Board %d] Ignored Frame\r\n", MY_ID);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
    }
}
