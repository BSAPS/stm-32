#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "main.h"
#include "stdint.h"

// Protocol constants
#define DLE        0x10
#define STX        0x02
#define ETX        0x03
#define CMD_ACK    0xAA
#define CMD_NACK   0x55
#define CMD_LCD_ON  0x01
#define CMD_LCD_OFF 0x02

#define RX_BUFFER_SIZE 256
#define CRC_INIT    0x0000
#define CRC_POLY    0x8005
#define IO_REF      1
#define XOR_OUT     0x0000
#define MY_ID       4

extern uint8_t rx_byte;
extern uint8_t rx_buffer[RX_BUFFER_SIZE];
extern uint16_t rx_index;

extern UART_HandleTypeDef huart1;

// Functions
void UART_SendFrame(uint8_t cmd);
void FSM_ParseByte(uint8_t rx);
void ProcessCommand(uint8_t *data, uint16_t len);
unsigned short crc16_calc(uint8_t *pData, int length);

#endif
