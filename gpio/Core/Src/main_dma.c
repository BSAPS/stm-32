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
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include <stdbool.h>
bool led_enabled = true;  // Dummy placeholder for link success

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define debug(msg) HAL_UART_Transmit(&huart2, (uint8_t*)(msg), strlen(msg), HAL_MAX_DELAY)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_up;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);

#define ROW_COUNT       32
#define PIXELS_PER_ROW  64
#define DMA_WORDS_PER_ROW (PIXELS_PER_ROW * 4)  // SET, CLK SET, CLK RESET, COLOR RESET
uint32_t dmaRowBuffer[DMA_WORDS_PER_ROW];       // must be 32-bit for BSRR

uint8_t frameBuffer[ROW_COUNT][PIXELS_PER_ROW][3] = {0};

uint8_t currentRow = 0;
uint32_t lastBlinkTime = 0;
uint8_t ledState = 0;
void debug_uart(const char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

void setRowAddress(uint8_t row) {
    uint32_t setMask = 0, resetMask = 0;

    if (row & 0x01) setMask |= A_Pin; else resetMask |= A_Pin;
    if (row & 0x02) setMask |= B_Pin; else resetMask |= B_Pin;
    if (row & 0x04) setMask |= C_Pin; else resetMask |= C_Pin;
    if (row & 0x08) setMask |= D_Pin; else resetMask |= D_Pin;

    GPIOB->BSRR = (setMask | (resetMask << 16));
}

void prepareDMABufferForRow(uint8_t row) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Preparing DMA buffer for row %d\r\n", row);
    debug_uart(msg);

    int idx = 0;

    for (int col = 0; col < PIXELS_PER_ROW; col++) {
        uint32_t setMask = 0;
        uint32_t clrMask = 0;

        // Upper half (R1-G1-B1)
        uint8_t r1 = frameBuffer[row][col][0];
        uint8_t g1 = frameBuffer[row][col][1];
        uint8_t b1 = frameBuffer[row][col][2];

        // Lower half (R2-G2-B2)
        uint8_t r2 = frameBuffer[row + 16][col][0];
        uint8_t g2 = frameBuffer[row + 16][col][1];
        uint8_t b2 = frameBuffer[row + 16][col][2];

        if (r1) setMask |= R1_Pin; else clrMask |= R1_Pin;
        if (g1) setMask |= G1_Pin; else clrMask |= G1_Pin;
        if (b1) setMask |= B1_Pin; else clrMask |= B1_Pin;
        if (r2) setMask |= R2_Pin; else clrMask |= R2_Pin;
        if (g2) setMask |= G2_Pin; else clrMask |= G2_Pin;
        if (b2) setMask |= B2_Pin; else clrMask |= B2_Pin;

        // Step 1: Set color
        dmaRowBuffer[idx++] = setMask;

        // Step 2: CLK HIGH
        dmaRowBuffer[idx++] = CLK_Pin;

        // Step 3: CLK LOW
        dmaRowBuffer[idx++] = (CLK_Pin << 16);

        // Step 4: Clear color
        dmaRowBuffer[idx++] = (clrMask << 16);
    }
}

void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
	debug("DMA Start_IT issued.\r\n");
    if (hdma == &hdma_tim1_up)
    {
        // Toggle debug LED
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        debug("DMA complete. Switching row.\r\n");

        // Step 1: Latch data
        GPIOB->BSRR = LAT_Pin;              // SET
        __NOP(); __NOP();                  // delay
        GPIOB->BSRR = (LAT_Pin << 16);     // RESET
        debug("Latch LOW\r\n");

        // Step 2: Turn on display (OE low)
        GPIOB->BSRR = (OE_Pin << 16);  // OE LOW (ENABLE)

        debug("OE LOW (Disable display)\r\n");
        // Optional delay for brightness control (PWM-like)
        //HAL_Delay(1);   -> 절대 금지.;

        // Step 3: Turn off display (OE high)
        GPIOB->BSRR = OE_Pin;          // OE HIGH (DISABLE)
        debug("OE High (Enable display)\r\n");
        // Step 4: Move to next row
        currentRow = (currentRow + 1) % 16;
        setRowAddress(currentRow);
        char debugBuf[64];

        // 🔍 디버깅 코드 추가
		char msg[64];
		uint8_t a = HAL_GPIO_ReadPin(GPIOB, A_Pin);
		uint8_t b = HAL_GPIO_ReadPin(GPIOB, B_Pin);
		uint8_t c = HAL_GPIO_ReadPin(GPIOB, C_Pin);
		uint8_t d = HAL_GPIO_ReadPin(GPIOB, D_Pin);

		snprintf(msg, sizeof(msg), "Row: %2d | A:%d B:%d C:%d D:%d\r\n", currentRow, a, b, c, d);
		debug_uart(msg);

        // Step 5: Prepare new data
        prepareDMABufferForRow(currentRow);

        // Step 6: Restart DMA
        HAL_StatusTypeDef dma_status = HAL_DMA_Start_IT(&hdma_tim1_up,
                                 (uint32_t)dmaRowBuffer,
                                 (uint32_t)&GPIOB->BSRR,
                                 DMA_WORDS_PER_ROW);

        if (dma_status != HAL_OK) {
            debug("DMA Start_IT failed!\r\n");
        }

    }
}


void startMatrixRefresh() {
    debug("Initializing DMA-based LED matrix refresh...\r\n");
    currentRow = 0;
    setRowAddress(currentRow);
    prepareDMABufferForRow(currentRow);
    HAL_TIM_Base_Start(&htim1);

    HAL_DMA_Start_IT(&hdma_tim1_up,
                     (uint32_t)dmaRowBuffer,
                     (uint32_t)&GPIOB->BSRR,
                     DMA_WORDS_PER_ROW);

    debug("DMA Start_IT issued2.\r\n");
    __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
    debug("Matrix refresh started.\r\n");
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_RTC_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  debug("System init complete. Starting LED matrix refresh...\r\n");

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  debug("System clock, GPIO, DMA, TIM1, and UART2 init complete.\r\n");
  debug("FrameBuffer initialized. Starting main loop...\r\n");

//  HAL_GPIO_WritePin(GPIOB, OE_Pin, GPIO_PIN_RESET);
//  HAL_Delay(1000);
//  HAL_GPIO_WritePin(GPIOB, OE_Pin, GPIO_PIN_SET);


  // 💡 전체 그린 패턴
  for (int r = 0; r < ROW_COUNT; r++) {
      for (int c = 0; c < PIXELS_PER_ROW; c++) {
          frameBuffer[r][c][0] = 0; // R
          frameBuffer[r][c][1] = 1; // G
          frameBuffer[r][c][2] = 0; // B
      }
  }

  startMatrixRefresh();
  debug("Green full-screen pattern initialized\r\n");

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_12;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 1;
  sTime.Minutes = 0;
  sTime.Seconds = 0;
  sTime.TimeFormat = RTC_HOURFORMAT12_AM;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 1;
  sDate.Year = 0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 83;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 255;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  hdma_tim1_up.XferCpltCallback = HAL_DMA_XferCpltCallback;
  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, R1_Pin|G1_Pin|B1_Pin|A_Pin
                          |B_Pin|C_Pin|D_Pin|R2_Pin
                          |G2_Pin|B2_Pin|CLK_Pin|LAT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : R1_Pin G1_Pin B1_Pin A_Pin
                           B_Pin C_Pin D_Pin R2_Pin
                           G2_Pin B2_Pin CLK_Pin */
  GPIO_InitStruct.Pin = R1_Pin|G1_Pin|B1_Pin|A_Pin
                          |B_Pin|C_Pin|D_Pin|R2_Pin
                          |G2_Pin|B2_Pin|CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : OE_Pin LAT_Pin */
  GPIO_InitStruct.Pin = OE_Pin|LAT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
#ifdef USE_FULL_ASSERT
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
	char buf[64];
	sprintf(buf, "[ASSERT] failed in %s:%lu\r\n", file, line);
	HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

	while(1);
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */


  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */