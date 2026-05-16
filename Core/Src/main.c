/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

#include "bmp.h"
#include "ili9488.h"
#include "gt911.h"
#include "gt911_port.h"
#include "lcd_io_spi_hal.h"
#include "lcd.h"

#include "lvgl.h"
#include "ui.h"

#include "ui/vars.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern void LCD_IO_Init(void);

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct {
    uint16_t id;
    uint8_t data[2];
} uart_msg_t;

typedef struct {
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_msg_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define RESOLUTION_HORIZONTAL 480
#define RESOLUTION_VERTICAL 320
#define BYTES_PER_PIXEL 2

#define UART_DMA_RX_SZ 256 // UART circular DMA RX buffer size

/* UART message packet protocol */
#define UART_ID_VOLTAGE 0x0001
#define UART_ID_CURRENT 0x0002

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan2;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */

static lv_display_t * active_lv_display = NULL; // for SPI DMA screen flush callback
volatile uint32_t lcd_dma_done_count = 0;
volatile uint32_t LCD_IO_Busy = 0;

volatile uint32_t dbg_flush_count = 0;
volatile uint32_t dbg_flush_pixels = 0;
volatile uint32_t dbg_flush_ready_fallback = 0;

static GT911_Object_t gt911; // GT911 touch screen controller

// Charts and variables for storing voltage/ current measurements received via UART
static lv_chart_series_t * s_v = NULL; // voltage chart
static lv_chart_series_t * s_i = NULL; // current chart
static float output_v = 0.0f;
static float output_i = 0.0f;
static lv_timer_t * charts_timer = NULL; // chart exclusive timer

// UART TX variables
uint8_t tx_frame[1+2+2+1];
static volatile uint8_t uart_tx_busy = 0;

// Buffer for storing UART messages by circular DMA
static uint8_t uart_dma_rx[UART_DMA_RX_SZ];
static volatile uint16_t uart_dma_last_pos = 0;

// UART Debugging
typedef struct {
    volatile uint32_t irq_hits;
    volatile uint32_t rx_cb_hits;
    volatile uint32_t rx_events;
    volatile uint16_t last_size;
    volatile uint8_t last_bytes[8];
} uart_dbg_t;

uart_dbg_t g_uart_dbg = {0};

volatile uint32_t g_uart_err_hits = 0;
volatile uint32_t g_uart_last_err = 0;
volatile uint32_t g_uart_last_isr = 0;

static volatile uint8_t dbg_captured = 0;

static volatile uint16_t uart_idle_hits = 0;

// Send and receive FDCAN messages
/* static FDCAN_TxHeaderTypeDef TxHeader;
static FDCAN_RxHeaderTypeDef RxHeader;
static uint8_t TxData_CAN[64];
static uint8_t RxData_CAN[64];

volatile can_msg_t q[QLEN];
volatile uint8_t qw=0, qr=0;

uint8_t tx_frame[1+1+1+4+1+64+1];
static volatile uint8_t uart_tx_busy = 0; */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_FDCAN2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

// === Display update and touch screen ===
void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map);
void touch_read(lv_indev_t * indev, lv_indev_data_t * data);

// === CAN message RX ===
/*
static uint8_t dlc_to_len(uint32_t dlc);
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);
static void textarea_trim(lv_obj_t * ta);
*/

// === UART Debugging ===
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);
static uint8_t xor_crc(const uint8_t *p, uint16_t n);

// === UART data TX, encoding and framing ===
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
static void uart_send_voltage(float v);
static void uart_send_current(float i);
static void uart_send_msg(uint16_t id, const uint8_t *data);

// === UART data RX, parsing and processing ===
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
static int parser_feed_byte(uint8_t b, uart_msg_t *out);
static void uart_rx_process_dma(void);
static void log_uart_data(const uart_msg_t *m);

// === UART data plotting ===
static void set_voltage_label(float v);
static void set_current_label(float i);
static void charts_init(void);
static void charts_feed_cb(lv_timer_t * t);
void start_charts(void);

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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_FDCAN2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* LCD initialization */

  ili9488_Init();
  ili9488_DisplayOn();

  /* Touch initialization */

  GT911_IO_t io =
  {
      .Init     = GT911_IO_Init,
      .DeInit   = GT911_IO_DeInit,
      .Address  = GT911_I2C_ADDR,
      .WriteReg = GT911_IO_WriteReg,
      .ReadReg  = GT911_IO_ReadReg,
      .GetTick  = GT911_IO_GetTick
  };

  GT911_RegisterBusIO(&gt911, &io);
  GT911_Init(&gt911);

  /* LVGL initialization */
  lv_init();

  lv_tick_set_cb(HAL_GetTick);

  /* LVGL display initialization */
  lv_display_t * display = lv_display_create(RESOLUTION_HORIZONTAL, RESOLUTION_VERTICAL);

  /* LVGL will render to these two 1/10 screen sized buffers for 2 bytes/pixel */
  /* One buffer renders while the other buffer is flushed by DMA for parallelization  */
  static uint8_t buf1[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 10 * BYTES_PER_PIXEL];
  static uint8_t buf2[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 10 * BYTES_PER_PIXEL];
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  /* UNUSED: benchmark against one 1/5 screen sized buffer*/
  /*
  static uint8_t buf[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 5 * BYTES_PER_PIXEL];
  lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  */

  /* Displays rendered image */
  lv_display_set_flush_cb(display, my_flush_cb);

  /* LVGL pointer (touch) initialization */

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read);

  /* EEZ Studio UI initialization */
  ui_init();

  /* Chart initialization */
  start_charts();

  /* UART initialization */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_dma_rx, UART_DMA_RX_SZ); // RX event interrupt called when buffer is full, half full or line is IDLE for 1 byte

  /* CAN initialization */
  // 1) Configure filter: accept ALL standard IDs into RX FIFO0

  /*
  FDCAN_FilterTypeDef sFilter;
  sFilter.IdType = FDCAN_STANDARD_ID;
  sFilter.FilterIndex = 0;
  sFilter.FilterType = FDCAN_FILTER_MASK;
  sFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilter.FilterID1 = 0x000;     // ID
  sFilter.FilterID2 = 0x000;     // mask 0 = accept all
  if (HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilter) != HAL_OK) Error_Handler();

  // 2) Global filter: accept non-matching frames too
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,
                              FDCAN_ACCEPT_IN_RX_FIFO0,
                              FDCAN_ACCEPT_IN_RX_FIFO0,
                              FDCAN_REJECT_REMOTE,
                              FDCAN_REJECT_REMOTE);

  // 3) Start FDCAN
  if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK) Error_Handler();

  // 4) Enable RX FIFO0 new message interrupt
  if (HAL_FDCAN_ActivateNotification(&hfdcan2,
          FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) Error_Handler();

  // 5) Prepare TX header (classic CAN, 8 bytes)
  TxHeader.Identifier = 0x123;
  TxHeader.IdType = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;     // classic
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;      // classic
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  /* UART TX for target battery voltage setting */
	  // float target_voltage = (float) get_var_target_battery_voltage();
	  // uart_send_voltage(target_voltage);

	  /* Provide updates to currently-displayed Widgets here. */
	  lv_timer_handler();
	  HAL_Delay(2);  /* Wait 2 ms before processing LVGL again */

	  /* Note that UART baud rate is 115200 bits per second, or 86.806 us per byte.
	   * For the 6 byte frame, transmission takes at least 520.836 us. */

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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN2_Init(void)
{

  /* USER CODE BEGIN FDCAN2_Init 0 */

  /* USER CODE END FDCAN2_Init 0 */

  /* USER CODE BEGIN FDCAN2_Init 1 */

  /* USER CODE END FDCAN2_Init 1 */
  hfdcan2.Instance = FDCAN2;
  hfdcan2.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan2.Init.AutoRetransmission = DISABLE;
  hfdcan2.Init.TransmitPause = DISABLE;
  hfdcan2.Init.ProtocolException = DISABLE;
  hfdcan2.Init.NominalPrescaler = 16;
  hfdcan2.Init.NominalSyncJumpWidth = 1;
  hfdcan2.Init.NominalTimeSeg1 = 1;
  hfdcan2.Init.NominalTimeSeg2 = 1;
  hfdcan2.Init.DataPrescaler = 1;
  hfdcan2.Init.DataSyncJumpWidth = 1;
  hfdcan2.Init.DataTimeSeg1 = 1;
  hfdcan2.Init.DataTimeSeg2 = 1;
  hfdcan2.Init.StdFiltersNbr = 0;
  hfdcan2.Init.ExtFiltersNbr = 0;
  hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN2_Init 2 */

  /* USER CODE END FDCAN2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x40B285C2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LCD_CS_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RS_Pin */
  GPIO_InitStruct.Pin = LCD_RS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// === Display update and touch screen ===

void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    active_lv_display = display;

    ili9488_DrawRGBImage(area->x1, area->y1, w, h, (uint16_t *)px_map);

    LCD_IO_Busy = LCD_IO_DmaBusy();

    if(!LCD_IO_DmaBusy()) {
        lv_display_flush_ready(display);
        active_lv_display = NULL;
    }
}

void LCD_IO_DmaTxCpltCallback(SPI_HandleTypeDef *hspi)
{
    lcd_dma_done_count++;

    if(active_lv_display) {
        lv_display_flush_ready(active_lv_display);
        active_lv_display = NULL;
    }
}

void touch_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void) indev;

    GT911_State_t st;

    if(GT911_GetState(&gt911, &st) != GT911_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if(st.TouchDetected) {

    	// Swap + mirror coordinates for LCD rotation

        uint32_t x = st.TouchY;
        uint32_t y = st.TouchX;
        x = (RESOLUTION_HORIZONTAL - 1) - x;          // mirror X

        data->point.x = (lv_coord_t)x;
        data->point.y = (lv_coord_t)y;
        data->state   = LV_INDEV_STATE_PRESSED;
    }
    else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// === CAN message RX ===

/*
static uint8_t dlc_to_len(uint32_t dlc)
{
	switch(dlc) {
		case FDCAN_DLC_BYTES_0:  return 0;
	    case FDCAN_DLC_BYTES_1:  return 1;
	    case FDCAN_DLC_BYTES_2:  return 2;
	    case FDCAN_DLC_BYTES_3:  return 3;
	    case FDCAN_DLC_BYTES_4:  return 4;
	    case FDCAN_DLC_BYTES_5:  return 5;
	    case FDCAN_DLC_BYTES_6:  return 6;
	    case FDCAN_DLC_BYTES_7:  return 7;
	    case FDCAN_DLC_BYTES_8:  return 8;
	    case FDCAN_DLC_BYTES_12: return 12;
	    case FDCAN_DLC_BYTES_16: return 16;
	    case FDCAN_DLC_BYTES_20: return 20;
	    case FDCAN_DLC_BYTES_24: return 24;
	    case FDCAN_DLC_BYTES_32: return 32;
	    case FDCAN_DLC_BYTES_48: return 48;
	    case FDCAN_DLC_BYTES_64: return 64;
	    default: return 8;
	}
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0) return;

    if(HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData_CAN) != HAL_OK)
        return;

    uint32_t id  = RxHeader.Identifier;
    uint8_t  len = dlc_to_len(RxHeader.DataLength);

    uint8_t is_ext = (RxHeader.IdType == FDCAN_EXTENDED_ID) ? 1 : 0;
    uint8_t is_fd  = (RxHeader.FDFormat == FDCAN_FD_CAN) ? 1 : 0;

    send_can(id, is_ext, is_fd, RxData_CAN, len);

}

static void textarea_trim(lv_obj_t * ta)
{
    uint32_t max = lv_textarea_get_max_length(ta); // currently at 258, which is the amount of characters to display 6 lines of CAN messages + 1
    if(max == 0) return;

    const char * txt = lv_textarea_get_text(ta);
    if(!txt) return;

    size_t len = strlen(txt);
    if(len < max) return;

    lv_textarea_set_text(ta, "");
}
*/

// === UART Debugging ===

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    g_uart_err_hits++;
    g_uart_last_err = huart->ErrorCode;
    g_uart_last_isr = huart->Instance->ISR;

    HAL_UART_AbortReceive(huart);
    uart_dma_last_pos = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_dma_rx, UART_DMA_RX_SZ);
}

static uint8_t xor_crc(const uint8_t *p, uint16_t n)
{
	uint8_t c = 0;
	for(uint16_t i=0;i<n;i++) c ^= p[i];
	return c;
}

// === UART data TX, encoding and framing ===

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1) uart_tx_busy = 0;
}

static void uart_send_voltage(float v)
{
	int val = (int)(v * 10.0f + 0.5f);   // convert to xxx.x format
	uint8_t data[2];

	uint8_t d0 = (val / 1000) % 10;
	uint8_t d1 = (val / 100)  % 10;
	uint8_t d2 = (val / 10)   % 10;
	uint8_t d3 =  val % 10;

	data[0] = (d0 << 4) | d1;
	data[1] = (d2 << 4) | d3;

    uart_send_msg(UART_ID_VOLTAGE, data);
}

static void uart_send_current(float i)
{
	int val = (int)(i * 100.0f + 0.5f);   // convert to xx.xx format
	uint8_t data[2];

	uint8_t d0 = (val / 1000) % 10;
	uint8_t d1 = (val / 100)  % 10;
	uint8_t d2 = (val / 10)   % 10;
	uint8_t d3 =  val % 10;

	data[0] = (d0 << 4) | d1;
	data[1] = (d2 << 4) | d3;

    uart_send_msg(UART_ID_CURRENT, data);
}

static void uart_send_msg(uint16_t id, const uint8_t *data)
{
    while (uart_tx_busy) {}      // wait until previous TX is complete
    uart_tx_busy = 1;

    uint16_t idx = 0;
    tx_frame[idx++] = 0xA5;                 // SOF
    tx_frame[idx++] = (uint8_t)(id & 0xFF); // ID low byte
    tx_frame[idx++] = (uint8_t)(id >> 8);   // ID high byte

    tx_frame[idx++] = data[0];
    tx_frame[idx++] = data[1];

    uint8_t crc = xor_crc(tx_frame, idx);
    tx_frame[idx++] = crc;

    if(HAL_UART_Transmit_DMA(&huart1, tx_frame, idx) != HAL_OK) {
        uart_tx_busy = 0;
    }

}

// === UART data RX, parsing and processing ===

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if (huart->Instance == USART1)
	{
		uart_rx_process_dma(); // Process received message
		uart_idle_hits++;
	}
}

static int parser_feed_byte(uint8_t b, uart_msg_t *out)
{
    enum { S_SOF, S_ID0, S_ID1, S_DATA0, S_DATA1, S_CRC };
    static uint8_t st = S_SOF;
    static uint8_t buf[1+2+2];
    static uint8_t idx = 0;

    switch (st)
    {
    case S_SOF:
        if (b == 0xA5) {
            idx = 0;
            buf[idx++] = b;
            st = S_ID0;
        }
        break;

    case S_ID0:
        buf[idx++] = b;
        st = S_ID1;
        break;

    case S_ID1:
        buf[idx++] = b;
        st = S_DATA0;
        break;

    case S_DATA0:
        buf[idx++] = b;
        st = S_DATA1;
        break;

    case S_DATA1:
        buf[idx++] = b;
        st = S_CRC;
        break;

    case S_CRC:
    {
        uint8_t crc = xor_crc(buf, idx);
        if (crc == b)
        {
            out->id = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
            out->data[0] = buf[3];
            out->data[1] = buf[4];
            st = S_SOF;
            return 1;
        }
        st = S_SOF;
        break;
    }
    }

    return 0;
}

static void uart_rx_process_dma(void)
{
    uart_msg_t m;
    uint16_t pos = UART_DMA_RX_SZ - __HAL_DMA_GET_COUNTER(huart1.hdmarx);

    if (pos != uart_dma_last_pos)
    {
        if (pos > uart_dma_last_pos)
        {
            for (uint16_t i = uart_dma_last_pos; i < pos; i++)
            {
                if (parser_feed_byte(uart_dma_rx[i], &m)) {
                    log_uart_data(&m);
                }
            }
        }
        else
        {
            for (uint16_t i = uart_dma_last_pos; i < UART_DMA_RX_SZ; i++)
            {
                if (parser_feed_byte(uart_dma_rx[i], &m)) {
                    log_uart_data(&m);
                }
            }
            for (uint16_t i = 0; i < pos; i++)
            {
                if (parser_feed_byte(uart_dma_rx[i], &m)) {
                    log_uart_data(&m);
                }
            }
        }

        uart_dma_last_pos = pos;
    }
}

static void log_uart_data(const uart_msg_t *m)
{
    uint8_t d0 = (m->data[0] >> 4) & 0x0F;
    uint8_t d1 =  m->data[0]       & 0x0F;
    uint8_t d2 = (m->data[1] >> 4) & 0x0F;
    uint8_t d3 =  m->data[1]       & 0x0F;

    // Check that each nibble must be a decimal digit 0..9
    if(d0 > 9 || d1 > 9 || d2 > 9 || d3 > 9) return;

    if(m->id == 0x01)
    {
        // Voltage format: xxx.x
        output_v = (float)(d0 * 100 + d1 * 10 + d2) + ((float)d3 / 10.0f);
    }
    else if(m->id == 0x02)
    {
        // Current format: xx.xx
        output_i = (float)(d0 * 10 + d1) + ((float)(d2 * 10 + d3) / 100.0f);
    }
}

// === UART data plotting ===

static void set_voltage_label(float v)
{
    // 1 decimal place
    int32_t v10 = (int32_t)(v * 10.0f + (v >= 0 ? 0.5f : -0.5f)); // rounded
    int32_t whole = v10 / 10;
    int32_t frac  = v10 % 10;
    if(frac < 0) frac = -frac;

    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld.%01ld V", (long)whole, (long)frac);
    lv_label_set_text(objects.output_voltage_label, buf);
}

static void set_current_label(float i)
{
    // 2 decimal places
    int32_t i100 = (int32_t)(i * 100.0f + (i >= 0 ? 0.5f : -0.5f)); // rounded
    int32_t whole = i100 / 100;
    int32_t frac  = i100 % 100;
    if(frac < 0) frac = -frac;

    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld.%02ld A", (long)whole, (long)frac);
    lv_label_set_text(objects.output_current_label, buf);
}

static void charts_init(void)
{
    // Voltage chart
    lv_obj_t * cv = objects.output_voltage_chart;
    lv_chart_set_type(cv, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(cv, 120); // last 120 samples on screen
    lv_chart_set_update_mode(cv, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
    lv_chart_set_axis_range(cv, LV_CHART_AXIS_PRIMARY_Y, 0, 600); // volts
    s_v = lv_chart_add_series(cv, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);


    // Current chart
    lv_obj_t * ci = objects.output_current_chart;
    lv_chart_set_type(ci, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ci, 120);
    lv_chart_set_update_mode(ci, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(ci, LV_CHART_AXIS_PRIMARY_Y, 0, 10); // amps
    s_i = lv_chart_add_series(ci, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
}

static void charts_feed_cb(lv_timer_t * t)
{
    (void)t;

    static float ts = 0.0f;
    ts += 0.5f; // 500 ms step as timer also has 500 ms step

    float v = output_v;
    float i = output_i;

    // Clamp to chart ranges
    if(v < 0) v = 0;
    if(v > 600) v = 600;
    if(i < 0) i = 0;
    if(i > 10) i = 10;

    // Set chart series
    lv_chart_set_next_value(objects.output_voltage_chart, s_v, (int32_t)v);
    lv_chart_set_next_value(objects.output_current_chart, s_i, (int32_t)i);

    // Set chart labels
    set_voltage_label(v);
    set_current_label(i);
}

void start_charts(void)
{
    charts_init();
    if(charts_timer == NULL) {
        charts_timer = lv_timer_create(charts_feed_cb, 500, NULL); // 500 ms timer for updating charts
    }
}

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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
