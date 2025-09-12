// uart_protocol.c

#include "uart_protocol.h"
#include "led_matrix.h"
#include "led_font.h"
#include "string.h"
#include "stdio.h"


extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef hrtc;
extern uint8_t rx_byte;
extern uint8_t led_enabled;

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
        crc ^= reverse_value(*pData++, 8) << 8;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x8005 : crc << 1;
        }
    }
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
    if (len < 4) return;

    uint8_t dst_mask = data[0];
    uint8_t cmd = data[1];

    // payload: data[2] ~ data[len - 3]
    uint16_t payload_len = len - 4;
    uint8_t* payload = &data[2];

    uint16_t recv_crc = (data[len - 2] << 8) | data[len - 1];
    uint16_t calc_crc = crc16_calc(data, len - 2);
    if (recv_crc != calc_crc) {
        UART_SendFrame(CMD_NACK);
        return;
    }

    if (((dst_mask >> (MY_ID - 1)) & 0x01) == 0) return;

    switch (cmd) {
        case CMD_LCD_ON:
            led_enabled = 1;
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
            LEDMatrix_TurnOn();
            UART_SendFrame(CMD_ACK);
            break;

        case CMD_LCD_OFF:
            led_enabled = 0;
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
            LEDMatrix_TurnOff();
            UART_SendFrame(CMD_ACK);
            break;

        case CMD_SYNC_TIME:
            if (payload_len == 7) {
                RTC_TimeTypeDef newTime;
                RTC_DateTypeDef newDate;

                newDate.Year  = payload[0];
                newDate.Month = payload[1];
                newDate.Date  = payload[2];
                newDate.WeekDay = RTC_WEEKDAY_MONDAY;

                newTime.Hours   = payload[3];
                newTime.Minutes = payload[4];
                newTime.Seconds = payload[5];
                newTime.TimeFormat = payload[6] ? RTC_HOURFORMAT12_PM : RTC_HOURFORMAT12_AM;
                newTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
                newTime.StoreOperation = RTC_STOREOPERATION_RESET;

                HAL_RTC_SetTime(&hrtc, &newTime, RTC_FORMAT_BIN);
                HAL_RTC_SetDate(&hrtc, &newDate, RTC_FORMAT_BIN);
                UART_SendFrame(CMD_ACK);
            }
            break;

        default:
            UART_SendFrame(CMD_NACK);
            break;
    }
}

