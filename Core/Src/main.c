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
    uint8_t dlc;
    uint8_t data[8];
} can_msg_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define RESOLUTION_HORIZONTAL 480
#define RESOLUTION_VERTICAL 320
#define BYTES_PER_PIXEL 2

#define RB_SZ 1024

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan1;

I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */

static GT911_Object_t gt911;

static lv_chart_series_t * s_v = NULL; // voltage chart
static lv_chart_series_t * s_i = NULL; // current chart
static lv_timer_t * charts_timer = NULL; // chart exclusive timer

static volatile uint8_t rb[RB_SZ];
static volatile uint16_t rb_w = 0, rb_r = 0;
static uint8_t rx_chunk[128];

static volatile uint16_t print_debug = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_FDCAN1_Init(void);
/* USER CODE BEGIN PFP */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
static uint8_t xor_crc(const uint8_t *p, uint16_t n);
static void textarea_trim(lv_obj_t * ta);
static void log_can_to_textarea(const can_msg_t *m);
void loop_uart_can_logs(void);
static void rb_push(uint8_t b);
static int rb_pop(uint8_t *b);
static int parser_poll(can_msg_t *out);
static void set_voltage_label(float v);
static void set_current_label(float i);
static void charts_init(void);
static void charts_feed_sim_cb(lv_timer_t * t);
void start_sim_charts(void);
void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map);
void touch_read(lv_indev_t * indev, lv_indev_data_t * data);

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
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  MX_FDCAN1_Init();
  /* USER CODE BEGIN 2 */

  /* LCD initialization */

  LCD_IO_Init();
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

  /* LVGL will render to this 1/10 screen sized buffer for 2 bytes/pixel */
  static uint8_t buf[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 10 * BYTES_PER_PIXEL];
  lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  /* Displays rendered image */
  lv_display_set_flush_cb(display, my_flush_cb);

  /* LVGL pointer (touch) initialization */

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read);

  /* EEZ Studio UI initialization */
  ui_init();

  /* Chart initialization */
//  start_sim_charts();

  /* UART initialization */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_chunk, sizeof(rx_chunk));
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  loop_uart_can_logs();
	  /* Provide updates to currently-displayed Widgets here. */
	  lv_timer_handler();
	  HAL_Delay(5);  /* Wait 5 milliseconds before processing LVGL timer again*/

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
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 16;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 1;
  hfdcan1.Init.NominalTimeSeg2 = 1;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x40B285C2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

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
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_BL_Pin|LCD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : TOUCH_INT_Pin */
  GPIO_InitStruct.Pin = TOUCH_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TOUCH_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RS_Pin LCD_CS_Pin */
  GPIO_InitStruct.Pin = LCD_RS_Pin|LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BL_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart->Instance != USART1) return;

    for(uint16_t i = 0; i < Size; i++)
    {
        rb_push(rx_chunk[i]);
    }

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_chunk, sizeof(rx_chunk));
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
}

static uint8_t xor_crc(const uint8_t *p, uint16_t n)
{
	uint8_t c = 0;
	for(uint16_t i=0;i<n;i++) c ^= p[i];
	return c;
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

static void log_can_to_textarea(const can_msg_t *m)
{
    char line[96];
    int n = snprintf(line, sizeof(line), "ID:%03X DLC:%u DATA:", m->id, m->dlc);

    for(int i=0;i<m->dlc && n < (int)sizeof(line)-4; i++)
        n += snprintf(&line[n], sizeof(line)-n, " %02X", m->data[i]);

    textarea_trim(objects.uart_logs);
    lv_textarea_add_text(objects.uart_logs, line);

    const char * txt = lv_textarea_get_text(objects.uart_logs);
    size_t len = strlen(txt);
    if(len < lv_textarea_get_max_length(objects.uart_logs)) {
    	lv_textarea_add_text(objects.uart_logs, "\n"); // new line only if not last line
    }

    //keep view pinned to bottom
    lv_textarea_set_cursor_pos(objects.uart_logs, LV_TEXTAREA_CURSOR_LAST);
    print_debug++;
}

void loop_uart_can_logs(void)
{
    can_msg_t m;
    while (parser_poll(&m)) {
        log_can_to_textarea(&m);
    }
}

static void rb_push(uint8_t b){
    uint16_t n = (rb_w + 1) % RB_SZ;
    if(n == rb_r) return; // overflow drop
    rb[rb_w] = b;
    rb_w = n;
}

static int rb_pop(uint8_t *b){
    if(rb_r == rb_w) return 0;
    *b = rb[rb_r];
    rb_r = (rb_r + 1) % RB_SZ;
    return 1;
}

static int parser_poll(can_msg_t *out)
{
    enum { S_SOF, S_TYPE, S_FLAGS, S_ID0, S_ID1, S_ID2, S_ID3, S_LEN, S_DATA, S_CRC };
    static uint8_t st = S_SOF;
    static uint8_t buf[1+1+1+4+1+64];   // everything except CRC
    static uint8_t idx = 0;
    static uint8_t need = 0;

    uint8_t b;

    while (rb_pop(&b))
    {
        switch (st)
        {
        case S_SOF:
            if (b == 0xA5) {
                idx = 0;
                buf[idx++] = b;
                st = S_TYPE;
            }
            break;

        case S_TYPE:
            if (b != 0x01) { st = S_SOF; break; }
            buf[idx++] = b;
            st = S_FLAGS;
            break;

        case S_FLAGS:
            buf[idx++] = b;
            st = S_ID0;
            break;

        case S_ID0: buf[idx++] = b; st = S_ID1; break;
        case S_ID1: buf[idx++] = b; st = S_ID2; break;
        case S_ID2: buf[idx++] = b; st = S_ID3; break;
        case S_ID3: buf[idx++] = b; st = S_LEN; break;

        case S_LEN:
            need = b;                 // number of data bytes to read
            if (need > 64) { st = S_SOF; break; }
            buf[idx++] = b;
            st = (need == 0) ? S_CRC : S_DATA;
            break;

        case S_DATA:
            buf[idx++] = b;
            if (--need == 0) st = S_CRC;
            break;

        case S_CRC:
        {
            uint8_t crc = xor_crc(buf, idx);
            if (crc == b)
            {
                // decode into can_msg_t
                uint32_t id =  (uint32_t)buf[3]
                             | ((uint32_t)buf[4] << 8)
                             | ((uint32_t)buf[5] << 16)
                             | ((uint32_t)buf[6] << 24);

                out->id  = (uint16_t)id;     // you defined id as uint16_t
                out->dlc = buf[7];
                if(out->dlc > 8) out->dlc = 8;
                for (int i=0; i<out->dlc; i++) out->data[i] = buf[8+i];

                st = S_SOF;
                return 1;
            }
            st = S_SOF;
            break;
        }
        }
    }

    return 0;
}

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
    lv_chart_set_axis_range(ci, LV_CHART_AXIS_PRIMARY_Y, 0, 20); // amps
    s_i = lv_chart_add_series(ci, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    // Seed with starting values so charts aren't empty
    for(int k = 0; k < 120; k++) {
        lv_chart_set_next_value(cv, s_v, get_var_target_battery_voltage() / 2);
        lv_chart_set_next_value(ci, s_i, 20 / 2);
    }
}

static void charts_feed_sim_cb(lv_timer_t * t)
{
    (void)t;

    // Screen-specific to delete off-screen graph?
    // if(!lv_obj_is_valid(objects.output_voltage_chart)) return;

    static float ts = 0.0f;
    ts += 0.5f; // 500 ms step if timer period is 500 ms

    // Simulated CC->CV-ish shape:
    float v = 350.0f + 70.0f * (1.0f - expf(-ts / 60.0f)) + 0.8f * sinf(ts * 0.7f);
    float i = 10.0f  + 11.0f * expf(-ts / 45.0f)        + 2.0f * sinf(ts * 0.9f);

    // Clamp to chart ranges
    if(v < 0) v = 0;
    if(v > 600) v = 600;
    if(i < 0) i =   0;
    if(i > 20) i = 20;

    // Set chart series
    lv_chart_set_next_value(objects.output_voltage_chart, s_v, (int32_t)v);
    lv_chart_set_next_value(objects.output_current_chart, s_i, (int32_t)i);

    // Set chart labels
    set_voltage_label(v);
    set_current_label(i);
}

void start_sim_charts(void)
{
    charts_init();
    if(charts_timer == NULL) {
        charts_timer = lv_timer_create(charts_feed_sim_cb, 500, NULL);
    }
}

/* flush callback for display */
void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{
    const uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
    const uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);

    uint16_t * p = (uint16_t *)px_map;

    // Sets window and streams w*h pixels
    ili9488_DrawRGBImage((uint16_t)area->x1, (uint16_t)area->y1, w, h, p);

    // Indicate that the buffer is available
    lv_display_flush_ready(display);
}

/* input device read callback for touch */
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
