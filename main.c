/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

#define DLE 0x10
#define STX 0x02
#define ETX 0x03
#define CMD_ACK   0xAA
#define CMD_NACK  0x55

#define CMD_LCD_ON     0x01
#define CMD_LCD_OFF    0x02

#define RX_BUFFER_SIZE 256
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint16_t rx_index = 0;

#define CRC_INIT       0x0000
#define CRC_POLY       0x8005
#define IO_REF         1       
#define XOR_OUT        0x0000

#define MY_ID          1
uint8_t rx_byte;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void UART_SendFrame(uint8_t cmd);
void UART_ReceiveFrame(void);
void ProcessCommand(uint8_t *data, uint16_t len);
unsigned short reverse_value(unsigned short value, int bit);
unsigned short crc16_calc(uint8_t *pData, int length);
void FSM_ParseByte(uint8_t rx);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int fputc(int ch, FILE *f)
{
    uint8_t temp[1]={ch};
    HAL_UART_Transmit(&huart1, temp, 1, 2);
    return ch;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        FSM_ParseByte(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // Continue receiving
    }
}




unsigned short reverse_value(unsigned short value, int bit)
{
    unsigned short ret = 0;
    for (int i = 0; i < bit; i++) {
        ret = (ret << 1) | ((value >> i) & 1);
    }
    return ret;
}

unsigned short crc16_calc(uint8_t *pData, int length)
{
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

void UART_SendFrame(uint8_t cmd) {
    uint8_t start[] = {DLE, STX};
    uint8_t end[] = {DLE, ETX};
		uint8_t frame[3]; // CMD + CRC(2byte)
		
		frame[0] = cmd;
    unsigned short crc = crc16_calc(&cmd, 1);
    frame[1] = (crc >> 8) & 0xFF; //
    frame[2] = crc & 0xFF;
		

    // ??? ?? ??
    HAL_UART_Transmit(&huart1, start, 2, HAL_MAX_DELAY);

    for (int i = 0; i < 3; i++) {  //len
        if (frame[i] == DLE) {
            uint8_t dle_escape[] = {DLE, DLE};
            HAL_UART_Transmit(&huart1, dle_escape, 2, HAL_MAX_DELAY);
        } else {
            HAL_UART_Transmit(&huart1, &frame[i], 1, HAL_MAX_DELAY);
        }
    }

    HAL_UART_Transmit(&huart1, end, 2, HAL_MAX_DELAY);
}

// UART receive FSM per byte, called from interrupt callback
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
                // End of frame ? process buffer
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




// Called when a valid frame is received: [DST_MASK][CMD][CRC1][CRC2]
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
    //uint16_t calc_crc = crc16_calc(&cmd, 1);

    if (recv_crc != calc_crc) {
        char *msg = "CRC Error\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
				UART_SendFrame(CMD_NACK); //send NACK
        return;
    }

    if ((dst_mask >> (MY_ID - 1)) & 0x01) {
    switch (cmd) {
        case CMD_LCD_ON:
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
            {
                char msg[64];
                snprintf(msg, sizeof(msg), "[Board %d] LD2 ON\r\n", MY_ID);
                HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 50);
								UART_SendFrame(CMD_ACK);
            }
            break;
        case CMD_LCD_OFF:
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
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

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

	char *msg = "UART Bitmask FSM Receiver Start\r\n";
	HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // Start interrupt receive

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
