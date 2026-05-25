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
#include "ui/vars.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ui.h>

extern void LCD_IO_Init(void);

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
	CHARGER_IDLE_NC_BAT,
	CHARGER_IDLE_C_BAT,
    CHARGER_PRECHARGE,
    CHARGER_CHARGING,
    CHARGER_FAULT
} charger_state_t; // Charger states: Idle (battery not connected), Idle (battery connected), Pre-Charge, Charging, Fault


typedef struct {
    uint16_t id;
    uint8_t data[2];
} uart_msg_t;

typedef struct {
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_msg_t; // UNUSED: for future CAN TX development

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

static volatile charger_state_t charger_state = CHARGER_IDLE_NC_BAT; // Initialise charger state at IDLE battery not connected

static lv_display_t * active_lv_display = NULL; // for SPI DMA screen flush callback

static GT911_Object_t gt911; // GT911 touch screen controller

// Variables for storing target voltage/ current received via CAN
static volatile float target_v = 0.0f;
static volatile float target_i = 0.0f;

// Charts and variables for storing voltage/ current measurements received via UART
static lv_chart_series_t * s_o_v = NULL; // output voltage chart
static lv_chart_series_t * s_o_i = NULL; // output current chart
static lv_chart_series_t * s_o_p = NULL; // output power chart

static lv_chart_series_t * s_b_v = NULL; // battery voltage chart

static lv_chart_series_t * s_p_v = NULL; // PFC voltage chart
static lv_chart_series_t * s_p_i = NULL; // PFC current chart

static lv_chart_series_t * s_t_1 = NULL; // temperature 1 chart
static lv_chart_series_t * s_t_2 = NULL; // temperature 2 chart
static lv_chart_series_t * s_t_3 = NULL; // temperature 3 chart

static volatile float output_v = 0.0f; // output voltage
static volatile float output_i = 0.0f; // output current
static volatile float output_p = 0.0f; // output power
static volatile float battery_v = 0.0f; // battery voltage
static volatile float pfc_v = 0.0f; // PFC voltage
static volatile float pfc_i = 0.0f; // PFC current
static volatile float temp[3] = {0.0f}; // temperature 1, 2, 3

static lv_timer_t * charts_timer = NULL; // chart exclusive timer
static float sim_time_s = 0.0f; // DEMO chart simulation time

// UART TX variables
uint8_t tx_frame[1+2+2+1];
static volatile uint8_t uart_tx_busy = 0;

// Buffer for storing UART messages by circular DMA
static uint8_t uart_dma_rx[UART_DMA_RX_SZ];
static volatile uint16_t uart_dma_last_pos = 0;

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
void LCD_IO_DmaTxCpltCallback(SPI_HandleTypeDef *hspi);

// === Live menu updates ===
const char * get_var_voltage_text(float v);
const char * get_var_current_text(float i);
const char * get_var_power_text(float p);
const char * get_var_temperature_text(float t);
void update_debug_states(void);

// === CAN message RX, parsing and processing ===
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);
static void log_can_data(uint8_t *data);

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

  /* LVGL will render to two 1/16 screen sized buffers for 2 bytes/pixel */
  /* One buffer flushes via DMA while the other renders in parallel */
  static uint8_t buf1[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 16 * BYTES_PER_PIXEL];
  static uint8_t buf2[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 16 * BYTES_PER_PIXEL];
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

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
  FDCAN_FilterTypeDef sFilter;
  sFilter.IdType = FDCAN_EXTENDED_ID;
  sFilter.FilterIndex = 0;
  sFilter.FilterType = FDCAN_FILTER_MASK;
  sFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilter.FilterID1 = 0x00000000;
  sFilter.FilterID2 = 0x00000000;
  if(HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilter) != HAL_OK)
  {
      Error_Handler();
  }

  // 2) Global filter: reject non-matching frames
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,
                               FDCAN_REJECT,
                               FDCAN_REJECT,
                               FDCAN_REJECT_REMOTE,
                               FDCAN_REJECT_REMOTE);

  // 3) Start FDCAN
  HAL_FDCAN_Start(&hfdcan2);

  // 4) Enable RX FIFO0 new message interrupt
  HAL_FDCAN_ActivateNotification(&hfdcan2,
                                 FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                 0);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  switch(charger_state) // Charger state machine
	  {
	      case CHARGER_IDLE_NC_BAT:

	    	  // Status widget
	          lv_label_set_text(objects.status_label,
	                            "IDLE");
	          lv_label_set_text(objects.detailed_status_label,
								"Battery not connected");
	          lv_obj_set_style_bg_color(objects.status_container,
	                                    lv_color_hex(0x464646),
	                                    0);

	          // Parameters widget (unlocked)
	          lv_label_set_text(objects.parameters_label,
								"Parameters");

	          // Bottom left stats widget
	          lv_label_set_text(objects.bottom_left_stat_desc,
								"Battery Voltage");
	          lv_label_set_text(objects.main_menu_voltage_label,
								"0.0 V"); // DEMO
	          lv_label_set_text(objects.main_menu_current_label,
								"");

	          // Set parameters button (unlocked)
	          lv_obj_remove_state(objects.set_parameters_button, LV_STATE_DISABLED);

	          break;

	      case CHARGER_IDLE_C_BAT:

	    	  // Status widget
	      	  lv_label_set_text(objects.status_label,
								"IDLE");
	      	  lv_label_set_text(objects.detailed_status_label,
								"Battery connected");
	      	  lv_obj_set_style_bg_color(objects.status_container,
										lv_color_hex(0x464646),
										0);

	      	  // Parameters widget (unlocked)
	      	  lv_label_set_text(objects.parameters_label,
								"Parameters");

	      	  // Bottom left stats widget
	      	  lv_label_set_text(objects.bottom_left_stat_desc,
								"Battery Voltage");
	      	  lv_label_set_text(objects.main_menu_voltage_label,
								"280.0 V"); // Set battery voltage for demo
			  lv_label_set_text(objects.main_menu_current_label,
								"");

	      	  // Set parameters button (unlocked)
	      	  lv_obj_remove_state(objects.set_parameters_button, LV_STATE_DISABLED);
	      	  lv_label_set_text(objects.set_parameters_label,
								"Set\nParameters");

			  break;

	      case CHARGER_PRECHARGE:

	    	  // Status widget
	          lv_label_set_text(objects.status_label,
	                            "PRE-CHARGE");
	          lv_label_set_text(objects.detailed_status_label,
								"Battery connected");
	          lv_obj_set_style_bg_color(objects.status_container,
	                                    lv_color_hex(0xFF8C00),
	                                    0);

	          // Parameters widget (locked)
	          lv_label_set_text(objects.parameters_label,
								"Parameters\n(locked)");
	          if(lv_screen_active() == objects.set_parameters_1 || lv_screen_active() == objects.set_parameters_2)
	          {
	              lv_screen_load(objects.main_menu); // Kicks user out of settings if state changes while in settings
	          }

	          // Bottom left stats widget
	          lv_label_set_text(objects.bottom_left_stat_desc,
								"Battery Voltage");
	          lv_label_set_text(objects.main_menu_voltage_label,
	        		  	  	  	"280.0 V"); // Set battery voltage for DEMO
			  lv_label_set_text(objects.main_menu_current_label,
								"");

	          // Set parameters button (locked)
			  lv_obj_add_state(objects.set_parameters_button, LV_STATE_DISABLED);
			  lv_label_set_text(objects.set_parameters_label,
								"Parameters\nlocked");

	          break;

	      case CHARGER_CHARGING:

	    	  // Status widget
	          lv_label_set_text(objects.status_label,
	                            "CHARGING");
	          lv_label_set_text(objects.detailed_status_label,
								"Battery connected");
	          lv_obj_set_style_bg_color(objects.status_container,
	                                    lv_color_hex(0x008000),
	                                    0);

	          // Parameters widget (locked)
	          lv_label_set_text(objects.parameters_label,
								"Parameters\n(locked)");
	          if(lv_screen_active() == objects.set_parameters_1 || lv_screen_active() == objects.set_parameters_2)
	          {
	              lv_screen_load(objects.main_menu); // Kicks user out of settings if state changes while in settings
	          }

	          // Bottom left stats widget
	          lv_label_set_text(objects.bottom_left_stat_desc,
								"Output");
	          lv_label_set_text(objects.main_menu_voltage_label,
								get_var_voltage_text(output_v));
			  lv_label_set_text(objects.main_menu_current_label,
					  	  	  	get_var_current_text(output_i));

	          // Set parameters button (locked)
			  lv_obj_add_state(objects.set_parameters_button, LV_STATE_DISABLED);
			  lv_label_set_text(objects.set_parameters_label,
								"Parameters\nlocked");

	          break;

	      case CHARGER_FAULT:

	    	  // Status widget
	          lv_label_set_text(objects.status_label,
	                            "FAULT");
	          lv_label_set_text(objects.detailed_status_label,
								"Shutdown circuit opened");
	          lv_obj_set_style_bg_color(objects.status_container,
	                                    lv_color_hex(0xB40000),
	                                    0);

	          // Parameters widget (locked)
	          lv_label_set_text(objects.parameters_label,
								"Parameters\n(locked)");
	          if(lv_screen_active() == objects.set_parameters_1 || lv_screen_active() == objects.set_parameters_2)
	          {
	              lv_screen_load(objects.main_menu); // Kicks user out of settings if state changes while in settings
	          }

	          // Bottom left stats widget
	          lv_label_set_text(objects.bottom_left_stat_desc,
								"Output");
	          lv_label_set_text(objects.main_menu_voltage_label,
								"0.0 V"); // For DEMO
			  lv_label_set_text(objects.main_menu_current_label,
							    "0.000 A"); // For DEMO

	          // Set parameters button (locked)
			  lv_obj_add_state(objects.set_parameters_button, LV_STATE_DISABLED);
			  lv_label_set_text(objects.set_parameters_label,
								"Parameters\nlocked");


	          break;
	  }

	  /* Update debug screen */
	  update_debug_states();

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
  hfdcan2.Init.NominalPrescaler = 17;
  hfdcan2.Init.NominalSyncJumpWidth = 1;
  hfdcan2.Init.NominalTimeSeg1 = 15;
  hfdcan2.Init.NominalTimeSeg2 = 4;
  hfdcan2.Init.DataPrescaler = 1;
  hfdcan2.Init.DataSyncJumpWidth = 1;
  hfdcan2.Init.DataTimeSeg1 = 1;
  hfdcan2.Init.DataTimeSeg2 = 1;
  hfdcan2.Init.StdFiltersNbr = 0;
  hfdcan2.Init.ExtFiltersNbr = 1;
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

    ili9488_DrawRGBImage(area->x1, area->y1, w, h, (uint16_t *)px_map); // Use DMA to flush pixels from buffer to display

    if(!LCD_IO_DmaBusy()) {
        lv_display_flush_ready(display);
        active_lv_display = NULL;
    }
}

void LCD_IO_DmaTxCpltCallback(SPI_HandleTypeDef *hspi)
{
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

// === Live menu updates ===

const char * get_var_voltage_text(float v) // Helper for displaying live voltage values
{
    static char buf[16];
    snprintf(buf,sizeof(buf),"%.1f V", v);
    return buf;
}

const char * get_var_current_text(float i) // Helper for displaying live current values
{
    static char buf[16];
    snprintf(buf,sizeof(buf),"%.3f A", i);
    return buf;
}

const char * get_var_power_text(float p) // Helper for displaying live power values
{
    static char buf[16];
    snprintf(buf,sizeof(buf),"%.3f kW", p);
    return buf;
}

const char * get_var_temperature_text(float t) // Helper for displaying live temperature values
{
    static char buf[16];
    snprintf(buf,sizeof(buf),"%.1f °C", t);
    return buf;
}

void update_debug_states(void)
{
	// TODO: Will parse debug status from HV which will be received as a one-hot code of all debug states. Debug states are boolean and can be OK/FAIL or ON/OFF.
	// TODO: Set thresholds for comms/ display performance and resource usage OK/FAIL
}

// === CAN message RX, parsing and processing ===

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
    {
        FDCAN_RxHeaderTypeDef RxHeader;
        uint8_t RxData[8];

        HAL_FDCAN_GetRxMessage(hfdcan,
                               FDCAN_RX_FIFO0,
                               &RxHeader,
                               RxData);

        // Check if message uses extended ID
        if (RxHeader.IdType == FDCAN_EXTENDED_ID)
        {
            if (RxHeader.Identifier == 0x000000FF)
            {
            	log_can_data(RxData);
            }
        }
    }
}

static void log_can_data(uint8_t *data)
{
    uint32_t voltage_bcd = 0;
    uint32_t current_bcd = 0;

    // Combine 4 bytes each into 32-bit values
    voltage_bcd =
        ((uint32_t)data[0] << 24) |
        ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8 ) |
        ((uint32_t)data[3]);

    current_bcd =
        ((uint32_t)data[4] << 24) |
        ((uint32_t)data[5] << 16) |
        ((uint32_t)data[6] << 8 ) |
        ((uint32_t)data[7]);

    // Extract BCD digits for voltage
    uint8_t v_digits[8];
    uint8_t i_digits[8];

    for(int i = 0; i < 8; i++)
    {
        v_digits[i] = (voltage_bcd >> ((7 - i) * 4)) & 0x0F;
        i_digits[i] = (current_bcd >> ((7 - i) * 4)) & 0x0F;

        // Validate BCD digits
        if(v_digits[i] > 9 || i_digits[i] > 9)
            return;
    }

    // Example format:
    // Voltage: xxxxx.xx
    target_v =
        (float)(
            v_digits[1] * 10000 +
            v_digits[2] * 1000  +
            v_digits[3] * 100   +
            v_digits[4] * 10    +
            v_digits[5]
        )
        +
        (float)(
            v_digits[6] * 10 +
            v_digits[7]
        ) / 100.0f;

    // Current: xx.xxxxx
		target_i =
		    (float)(
		        i_digits[0] * 10 +
		        i_digits[1]
		    )
		    +
		    (float)(
		        i_digits[2] * 10000 +
		        i_digits[3] * 1000  +
		        i_digits[4] * 100   +
		        i_digits[5] * 10    +
		        i_digits[6]
		    ) / 100000.0f;
}

// === UART Debugging ===

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

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

static void charts_init(void) // LVGL charts can only plot integers. Scaling used for decimals.
{
    // Output voltage chart
    lv_obj_t * c_o_v = objects.output_voltage_chart;
    lv_chart_set_point_count(c_o_v, 120); // last 120 samples on screen
    lv_chart_set_type(c_o_v, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(c_o_v, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
    lv_chart_set_axis_range(c_o_v, LV_CHART_AXIS_PRIMARY_Y, 0, 600*10); // volts (scaled by 10 for 1 d.p.)
    lv_obj_set_style_size(c_o_v, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
    lv_chart_set_div_line_count(c_o_v, 5+2, 3+2); // Chart grid setting
    s_o_v = lv_chart_add_series(c_o_v, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    // Output current chart
    lv_obj_t * c_o_i = objects.output_current_chart;
    lv_chart_set_point_count(c_o_i, 120); // last 120 samples on screen
    lv_chart_set_type(c_o_i, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(c_o_i, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
    lv_chart_set_axis_range(c_o_i, LV_CHART_AXIS_PRIMARY_Y, 0, 10*1000); // amps (scaled by 1000 for 3 d.p.)
    lv_obj_set_style_size(c_o_i, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
    lv_chart_set_div_line_count(c_o_i, 4+2, 3+2); // Chart grid setting
    s_o_i = lv_chart_add_series(c_o_i, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    // Output power chart
	lv_obj_t * c_o_p = objects.output_power_chart;
	lv_chart_set_point_count(c_o_p, 120); // last 120 samples on screen
	lv_chart_set_type(c_o_p, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_o_p, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_o_p,LV_CHART_AXIS_PRIMARY_Y, 0, 4 * 1000); // kilowatts (scaled by 1000 for 3 d.p.)
	lv_obj_set_style_size(c_o_p, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_o_p, 3+2, 3+2); // Chart grid setting
	s_o_p = lv_chart_add_series(c_o_p, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

	// Battery voltage chart
	lv_obj_t * c_b_v = objects.battery_voltage_chart;
	lv_chart_set_point_count(c_b_v, 120); // last 120 samples on screen
	lv_chart_set_type(c_b_v, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_b_v, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_b_v, LV_CHART_AXIS_PRIMARY_Y, 0, 600*10); // volts (scaled by 10 for 1 d.p.)
	lv_obj_set_style_size(c_b_v, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_b_v, 5+2, 3+2); // Chart grid setting
	s_b_v = lv_chart_add_series(c_b_v, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

	// PFC voltage chart
	lv_obj_t * c_p_v = objects.pfc_voltage_chart;
	lv_chart_set_point_count(c_p_v, 120); // last 120 samples on screen
	lv_chart_set_type(c_p_v, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_p_v, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_p_v, LV_CHART_AXIS_PRIMARY_Y, 0, 600*10); // volts (scaled by 10 for 1 d.p.)
	lv_obj_set_style_size(c_p_v, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_p_v, 5+2, 3+2); // Chart grid setting
	s_p_v = lv_chart_add_series(c_p_v, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

	// PFC current chart
	lv_obj_t * c_p_i = objects.pfc_current_chart;
	lv_chart_set_point_count(c_p_i, 120); // last 120 samples on screen
	lv_chart_set_type(c_p_i, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_p_i, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_p_i, LV_CHART_AXIS_PRIMARY_Y, 0, 10*1000); // amps (scaled by 1000 for 3 d.p.)
	lv_obj_set_style_size(c_p_i, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_p_i, 4+2, 3+2); // Chart grid setting
	s_p_i = lv_chart_add_series(c_p_i, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

	// Temperature 1 chart
	lv_obj_t * c_t_1 = objects.temperature_chart_1;
	lv_chart_set_point_count(c_t_1, 120); // last 120 samples on screen
	lv_chart_set_type(c_t_1, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_t_1, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_t_1, LV_CHART_AXIS_PRIMARY_Y, 0, 100*10); // degrees celsius (scaled by 10 for 1 d.p.)
	lv_obj_set_style_size(c_t_1, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_t_1, 4+2, 3+2); // Chart grid setting
	s_t_1 = lv_chart_add_series(c_t_1, lv_palette_main(LV_PALETTE_BROWN), LV_CHART_AXIS_PRIMARY_Y);

	// Temperature 2 chart
	lv_obj_t * c_t_2 = objects.temperature_chart_2;
	lv_chart_set_point_count(c_t_2, 120); // last 120 samples on screen
	lv_chart_set_type(c_t_2, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_t_2, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_t_2, LV_CHART_AXIS_PRIMARY_Y, 0, 100*10); // degrees celsius (scaled by 10 for 1 d.p.)
	lv_obj_set_style_size(c_t_2, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_t_2, 4+2, 3+2); // Chart grid setting
	s_t_2 = lv_chart_add_series(c_t_2, lv_palette_main(LV_PALETTE_BROWN), LV_CHART_AXIS_PRIMARY_Y);

	// Temperature 3 chart
	lv_obj_t * c_t_3 = objects.temperature_chart_3;
	lv_chart_set_point_count(c_t_3, 120); // last 120 samples on screen
	lv_chart_set_type(c_t_3, LV_CHART_TYPE_LINE);
	lv_chart_set_update_mode(c_t_3, LV_CHART_UPDATE_MODE_SHIFT); // scrolling chart
	lv_chart_set_axis_range(c_t_3, LV_CHART_AXIS_PRIMARY_Y, 0, 100*10); // degrees celsius (scaled by 10 for 1 d.p.)
	lv_obj_set_style_size(c_t_3, 0, 0, LV_PART_INDICATOR); // remove chart dot for pure line chart
	lv_chart_set_div_line_count(c_t_3, 4+2, 3+2); // Chart grid setting
	s_t_3 = lv_chart_add_series(c_t_3, lv_palette_main(LV_PALETTE_BROWN), LV_CHART_AXIS_PRIMARY_Y);
}

static void charts_feed_cb(lv_timer_t * t) // DEMO version charts_feed_cb, simulated data
{
    (void)t;

    sim_time_s += 0.5f; // timer period = 500 ms

    // --- Simulated charger data ---

    // Output voltage ramp: +20 V/min from 280 V
    output_v = 280.0f + (20.0f / 60.0f) * sim_time_s;

    if(output_v > 400.0f)
        output_v = 400.0f;

    // Constant-current phase
    output_i = 7.5f;

    // Add ±1% ripple, except temperature
    float r1 = 1.0f + 0.01f * sinf(2.0f * 3.14159f * 0.8f * sim_time_s);
    float r2 = 1.0f + 0.01f * sinf(2.0f * 3.14159f * 0.6f * sim_time_s + 1.2f);
    float r3 = 1.0f + 0.01f * sinf(2.0f * 3.14159f * 0.9f * sim_time_s + 2.0f);

    output_v *= r1;
    output_i *= r2;

    output_p = (output_v * output_i) / 1000.0f; // kW

    if (charger_state != CHARGER_IDLE_NC_BAT)
    	battery_v = output_v;

    pfc_v = 400.0f * r3;

    // 100% efficiency assumption
    pfc_i = (output_p * 1000.0f) / pfc_v;

    // Simulated temperatures
    temp[0] = 25.0f + 0.020f * sim_time_s;
    temp[1] = 24.0f + 0.015f * sim_time_s;
    temp[2] = 23.0f + 0.010f * sim_time_s;

    // Copy values locally
    float o_v = output_v;
    float o_i = output_i;
    float o_p = output_p;
    float b_v = battery_v;
    float p_v = pfc_v;
    float p_i = pfc_i;

    float temperature[3];
    temperature[0] = temp[0];
    temperature[1] = temp[1];
    temperature[2] = temp[2];

    // Clamp to chart ranges
    if(o_v < 0) o_v = 0;
    if(o_v > 600) o_v = 600;

    if(o_i < 0) o_i = 0;
    if(o_i > 10) o_i = 10;

    if(o_p < 0) o_p = 0;
    if(o_p > 4) o_p = 4;

    if(b_v < 0) b_v = 0;
    if(b_v > 600) b_v = 600;

    if(p_v < 0) p_v = 0;
    if(p_v > 600) p_v = 600;

    if(p_i < 0) p_i = 0;
    if(p_i > 10) p_i = 10;

    for(int k = 0; k < 3; k++) {
        if(temperature[k] < 0) temperature[k] = 0;
        if(temperature[k] > 100) temperature[k] = 100;
    }

    // Set chart series
    lv_chart_set_next_value(objects.output_voltage_chart, s_o_v, (int32_t)(o_v * 10.0f + 0.5f));
    lv_chart_set_next_value(objects.output_current_chart, s_o_i, (int32_t)(o_i * 1000.0f + 0.5f));
    lv_chart_set_next_value(objects.output_power_chart,   s_o_p, (int32_t)(o_p * 1000.0f + 0.5f));

    lv_chart_set_next_value(objects.battery_voltage_chart, s_b_v, (int32_t)(b_v * 10.0f + 0.5f));
    lv_chart_set_next_value(objects.pfc_voltage_chart,     s_p_v, (int32_t)(p_v * 10.0f + 0.5f));
    lv_chart_set_next_value(objects.pfc_current_chart,     s_p_i, (int32_t)(p_i * 1000.0f + 0.5f));

    lv_chart_set_next_value(objects.temperature_chart_1, s_t_1, (int32_t)(temperature[0] * 10.0f + 0.5f));
    lv_chart_set_next_value(objects.temperature_chart_2, s_t_2, (int32_t)(temperature[1] * 10.0f + 0.5f));
    lv_chart_set_next_value(objects.temperature_chart_3, s_t_3, (int32_t)(temperature[2] * 10.0f + 0.5f));

    // Set chart labels
    lv_label_set_text(objects.output_voltage_label, get_var_voltage_text(o_v));
    lv_label_set_text(objects.output_current_label, get_var_current_text(o_i));
    lv_label_set_text(objects.output_power_label,   get_var_power_text(o_p));

    lv_label_set_text(objects.battery_voltage_label, get_var_voltage_text(b_v));
    lv_label_set_text(objects.pfc_voltage_label,     get_var_voltage_text(p_v));
    lv_label_set_text(objects.pfc_current_label,     get_var_current_text(p_i));

    lv_label_set_text(objects.temperature_label_1, get_var_temperature_text(temperature[0]));
    lv_label_set_text(objects.temperature_label_2, get_var_temperature_text(temperature[1]));
    lv_label_set_text(objects.temperature_label_3, get_var_temperature_text(temperature[2]));

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
