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
#include "dma.h"
#include "fdcan.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

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

#include "charger_uart.h"

#include "ui/vars.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
	CHARGER_SHUTDOWN,
    CHARGER_FAULT,
	CHARGER_STATE_TOTAL
} charger_state_t; // Charger states: Idle (battery not connected), Idle (battery connected), Pre-Charge, Charging, Fault

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

#define BUTTON_DEBOUNCE_PERIOD 2500 // 2.5 second debounce for start/stop button

// Error code definitions for bitmask
#define ERR_VIN_LOW     0x01
#define ERR_OVERCURRENT 0x02
#define ERR_OVERVOLTAGE 0x04
#define ERR_OVERPOWER   0x08
#define ERR_OVERTEMP    0x10

// Debug screen text update
#define OK_TEXT    "#00FF00 OK#"
#define FAIL_TEXT "#FF0000 FAIL#"

// For updating CV/CC labels from CAN
#define V_MIN 0.0f
#define V_MAX 600.0f
#define I_MIN 0.0f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static volatile charger_state_t charger_state = CHARGER_IDLE_NC_BAT; // Initialize charger state at IDLE and not connected to battery
static volatile uint8_t charger_errorCode = 0x00; // Initialise charger with no debug errors
static volatile uint8_t uart_counter = 0; // Updates every 100 ms, if nothing then UART fail
static volatile uint8_t can_counter = 0; // Updates every 100 ms, if nothing then CAN fail

static lv_display_t * active_lv_display = NULL; // for SPI DMA screen flush callback

static GT911_Object_t gt911; // GT911 touch screen controller

// Variables for storing target voltage/ current and error received via CAN
static volatile float target_v = 0.0f;
static volatile float target_i = 0.0f;
static volatile uint16_t BMS_error_code = 0x0000;
volatile uint8_t can_setpoint_update_pending = 0; // In order to not update labels inside CAN RX ISR

// Charts and variables for storing voltage/ current measurements received via UART
static lv_chart_series_t * s_o_v = NULL; // output voltage chart
static lv_chart_series_t * s_o_i = NULL; // output current chart
static lv_chart_series_t * s_o_p = NULL; // output power chart

static lv_chart_series_t * s_b_v = NULL; // battery voltage chart

static lv_chart_series_t * s_p_v = NULL; // PFC voltage chart

static lv_chart_series_t * s_t_1 = NULL; // temperature 1 chart
static lv_chart_series_t * s_t_2 = NULL; // temperature 2 chart
static lv_chart_series_t * s_t_3 = NULL; // temperature 3 chart

static volatile float OutputVoltage = 0.0f; // output voltage
static volatile float OutputCurrent = 0.0f; // output current
static volatile float OutputPower = 0.0f; // output power
static volatile float BatteryVoltage = 0.0f; // battery voltage
static volatile float PFCVoltage = 0.0f; // PFC voltage
static volatile float temp[3] = {0.0f}; // temperature 1, 2, 3

static lv_timer_t * charts_timer = NULL; // chart exclusive timer
static lv_timer_t * status_timer = NULL;
static lv_timer_t * uart_timer = NULL;
static lv_timer_t * can_timer = NULL;

volatile uint8_t buttonPressed = 0; // button detection
volatile uint32_t lastButtonInterrupt = 0; // button debounce


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
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
static void increment_uart_timer(lv_timer_t * t);
static void increment_can_timer(lv_timer_t * t);
static void update_error_labels(uint8_t errorCode);
extern const char * get_var_constant_voltage_setpoint_text(void);
extern const char * get_var_constant_current_setpoint_text(void);

// === CAN message RX, parsing and processing ===
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);
static void log_can_data(uint8_t *data);
static void update_setpoints_from_can(float voltage, float current);
extern float clamp_float(float x, float lo, float hi);
extern int32_t voltage_to_bar(int32_t v, int32_t MIN, int32_t MAX);
extern float current_to_bar(float i, float MIN, float MAX);
extern float get_current_max_for_voltage(float voltage);

// === UART data plotting ===
static void charts_init(void);
static void charts_feed_cb(lv_timer_t * t);
void start_charts(void);

// === Button detection and debounce ===
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

// === Status/ fault update to HV ===
static void LV_status_share(lv_timer_t * t);

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

  /* LVGL will render to two 1/16
   *  screen sized buffers for 2 bytes/pixel */
  /* One buffer flushes via DMA while the other renders in parallel */
  static uint8_t buf1[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 20 * BYTES_PER_PIXEL];
  static uint8_t buf2[RESOLUTION_HORIZONTAL * RESOLUTION_VERTICAL / 20 * BYTES_PER_PIXEL];
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  /* Displays rendered image */
  lv_display_set_flush_cb(display, my_flush_cb);

  /* LVGL pointer (touch) initialization */
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read);

  /* EEZ Studio UI initialization */
  ui_init();

  /* UART initalization */
  uart_init();

  /* Chart initialization */
  start_charts();

  /* Timers initalization */
  status_timer = lv_timer_create(LV_status_share, 100, NULL);
  uart_timer   = lv_timer_create(increment_uart_timer, 100, NULL);
  can_timer    = lv_timer_create(increment_can_timer, 100, NULL);

  /* CAN initialization */
  // 1) Configure filter: accept ALL extended IDs into RX FIFO0
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
	  if (buttonPressed)
	  {
	      buttonPressed = 0;

	      if (charger_state == CHARGER_IDLE_C_BAT) // only send CC/CV setpoint when starting charging
	      {
	          printfDma("\\V %3.0f\n", get_var_constant_voltage_setpoint());
	          printfDma("\\C %3.2f\n", get_var_constant_current_setpoint());
	      }

	      if (charger_state == CHARGER_IDLE_C_BAT ||
	          charger_state == CHARGER_CHARGING) // toggles charging to start or stop
	      {
	          printfDma("\\S\n");
	      }
	  }

	  /* Update debug screen */
      update_error_labels(charger_errorCode);

      if(can_setpoint_update_pending)
      {
          can_setpoint_update_pending = 0;
          update_setpoints_from_can(target_v, target_i);
      }

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
								get_var_voltage_text(BatteryVoltage));
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
								get_var_voltage_text(BatteryVoltage));
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
								get_var_voltage_text(BatteryVoltage));
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
								get_var_voltage_text(OutputVoltage));
			  lv_label_set_text(objects.main_menu_current_label,
					  	  	  	get_var_current_text(OutputCurrent));

	          // Set parameters button (locked)
			  lv_obj_add_state(objects.set_parameters_button, LV_STATE_DISABLED);
			  lv_label_set_text(objects.set_parameters_label,
								"Parameters\nlocked");

	          break;

	      case CHARGER_SHUTDOWN:
	    	  charger_state = CHARGER_IDLE_C_BAT;

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
								get_var_voltage_text(OutputVoltage));
			  lv_label_set_text(objects.main_menu_current_label,
								get_var_current_text(OutputCurrent));

	          // Set parameters button (locked)
			  lv_obj_add_state(objects.set_parameters_button, LV_STATE_DISABLED);
			  lv_label_set_text(objects.set_parameters_label,
								"Parameters\nlocked");

			  break;

	      	default:
	      	  break;

	  }

	  /* Provide updates to currently-displayed Widgets here. */
	  lv_timer_handler();
	  HAL_Delay(2);  /* Wait 2 ms before processing LVGL again */

	  /* Note that UART baud rate is 115200 bits per second, or 86.806 us per byte. */

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

static void increment_can_timer(lv_timer_t * t)
{
    (void)t;
    if(can_counter < 255)
        can_counter++;
}

static void increment_uart_timer(lv_timer_t * t)
{
    (void)t;
    if(uart_counter < 255)
        uart_counter++;
}

static void update_error_labels(uint8_t errorCode) // Helper for updating debug menu
{
    char hv_text[64];
    char overall_text[32];
    char input_text[32];
    char lv_text[64];

    snprintf(hv_text, sizeof(hv_text),
             "%s\n%s\n%s",
             (errorCode & ERR_OVERVOLTAGE) ? FAIL_TEXT : OK_TEXT,
             (errorCode & ERR_OVERCURRENT) ? FAIL_TEXT : OK_TEXT,
             (errorCode & ERR_OVERPOWER)   ? FAIL_TEXT : OK_TEXT);

    snprintf(overall_text, sizeof(overall_text),
             "%s",
             (errorCode & ERR_OVERTEMP) ? FAIL_TEXT : OK_TEXT);

    snprintf(input_text, sizeof(input_text),
             "%s",
             (errorCode & ERR_VIN_LOW) ? FAIL_TEXT : OK_TEXT);

    snprintf(lv_text, sizeof(lv_text),
            "%s\n%s",
            (uart_counter > 10) ? FAIL_TEXT : OK_TEXT,
            (can_counter > 10)  ? FAIL_TEXT : OK_TEXT);

    lv_label_set_recolor(objects.hv_states, true);
    lv_label_set_recolor(objects.overall_states, true);
    lv_label_set_recolor(objects.input_states, true);
    lv_label_set_recolor(objects.lv_states, true);

    lv_label_set_text(objects.hv_states, hv_text);
    lv_label_set_text(objects.overall_states, overall_text);
    lv_label_set_text(objects.input_states, input_text);
    lv_label_set_text(objects.lv_states, lv_text);
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
                can_counter = 0;
            	log_can_data(RxData);
            	can_setpoint_update_pending = 1;
            }
        }
    }
}

static void log_can_data(uint8_t *data)
{
    uint32_t voltage_bcd = 0;
    uint32_t current_bcd = 0;

    voltage_bcd =
        ((uint32_t)data[0] << 16) |
        ((uint32_t)data[1] << 8 ) |
        ((uint32_t)data[2]);

    current_bcd =
        ((uint32_t)data[3] << 16) |
        ((uint32_t)data[4] << 8 ) |
        ((uint32_t)data[5]);

    BMS_error_code =
        ((uint16_t)data[6] << 8) |
        ((uint16_t)data[7]);

    uint8_t v_digits[6];
    uint8_t i_digits[6];

    for (int i = 0; i < 6; i++)
    {
        v_digits[i] = (voltage_bcd >> ((5 - i) * 4)) & 0x0F;
        i_digits[i] = (current_bcd >> ((5 - i) * 4)) & 0x0F;

        if (v_digits[i] > 9 || i_digits[i] > 9)
            return;
    }

    // Voltage format: xxx.xxx
    target_v =
        (float)(
            v_digits[0] * 100 +
            v_digits[1] * 10  +
            v_digits[2]
        )
        +
        (float)(
            v_digits[3] * 100 +
            v_digits[4] * 10  +
            v_digits[5]
        ) / 1000.0f;

    // Current format: x.xxxxx
    target_i =
        (float)i_digits[0]
        +
        (float)(
            i_digits[1] * 10000 +
            i_digits[2] * 1000  +
            i_digits[3] * 100   +
            i_digits[4] * 10    +
            i_digits[5]
        ) / 100000.0f;
}

void update_setpoints_from_can(float voltage, float current) {
    
    // Reduce precision first
    voltage = roundf(voltage * 10.0f) / 10.0f;        // xxx.x
    current = roundf(current * 1000.0f) / 1000.0f;    // x.xxx

    // Clamp voltage before setting
    voltage = clamp_float(voltage, V_MIN, V_MAX);

    set_var_constant_voltage_setpoint(voltage);

    // Update voltage labels
    lv_label_set_text(objects.constant_voltage_setpoint_label,
                      get_var_constant_voltage_setpoint_text());
    lv_label_set_text(objects.main_menu_cv_label,
                      get_var_constant_voltage_setpoint_text());
    lv_label_set_text(objects.constant_voltage_setpoint_label_1,
                      get_var_constant_voltage_setpoint_text());

    // Update voltage bars
    lv_bar_set_value(objects.constant_voltage_setpoint_bar,
                     voltage_to_bar(voltage, V_MIN, V_MAX),
                     LV_ANIM_ON);
    lv_bar_set_value(objects.constant_voltage_setpoint_bar_1,
                     voltage_to_bar(voltage, V_MIN, V_MAX),
                     LV_ANIM_ON);

    // Current max may depend on voltage
    float I_MAX = get_current_max_for_voltage(voltage);

    // Clamp current before setting
    current = clamp_float(current, I_MIN, I_MAX);

    set_var_constant_current_setpoint(current);

    // Update current labels
    lv_label_set_text(objects.constant_current_setpoint_label,
                      get_var_constant_current_setpoint_text());
    lv_label_set_text(objects.main_menu_cc_label,
                      get_var_constant_current_setpoint_text());
    lv_label_set_text(objects.constant_current_setpoint_label_1,
                      get_var_constant_current_setpoint_text());

    // Update current bars
    lv_bar_set_value(objects.constant_current_setpoint_bar,
                     current_to_bar(current, I_MIN, I_MAX),
                     LV_ANIM_ON);
    lv_bar_set_value(objects.constant_current_setpoint_bar_1,
                     current_to_bar(current, I_MIN, I_MAX),
                     LV_ANIM_ON);
}



// === UART Initialization ===

void uart_parseRxFrame(uint8_t* buffer, uint32_t len)
{
    (void)len;

    uart_counter = 0;

    int state_tmp;
    unsigned int err_tmp;

    if (sscanf((char *)buffer,
               "VI:%7f, VO:%7f, IO:%7f, VB:%7f, PO:%9f, T1:%7f, T2:%7f, T3:%7f, ST:%1d, ER:%02x\n",
               &PFCVoltage,
               &OutputVoltage,
               &OutputCurrent,
               &BatteryVoltage,
               &OutputPower,
               &temp[0],
               &temp[1],
               &temp[2],
               &state_tmp,
               &err_tmp) == 10)
    {
        charger_state = (charger_state_t)state_tmp;
        charger_errorCode = (uint8_t)err_tmp;

        printfDma("\\ Received\n");
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
	lv_chart_set_axis_range(c_o_p, LV_CHART_AXIS_PRIMARY_Y, 0, 10*1000); // kilowatts (scaled by 1000 for 3 d.p.)
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

static void charts_feed_cb(lv_timer_t * t)
{
	(void)t;

    float o_v = OutputVoltage;
    float o_i = OutputCurrent;
    float o_p = OutputPower;
    float b_v = BatteryVoltage;
    float p_v = PFCVoltage;
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

	lv_chart_set_next_value(objects.temperature_chart_1, s_t_1, (int32_t)(temperature[0] * 10.0f + 0.5f));
	lv_chart_set_next_value(objects.temperature_chart_2, s_t_2, (int32_t)(temperature[1] * 10.0f + 0.5f));
	lv_chart_set_next_value(objects.temperature_chart_3, s_t_3, (int32_t)(temperature[2] * 10.0f + 0.5f));

	// Set chart labels
	lv_label_set_text(objects.output_voltage_label, get_var_voltage_text(o_v));
	lv_label_set_text(objects.output_current_label, get_var_current_text(o_i));
	lv_label_set_text(objects.output_power_label,   get_var_power_text(o_p));

	lv_label_set_text(objects.battery_voltage_label, get_var_voltage_text(b_v));
	lv_label_set_text(objects.pfc_voltage_label,     get_var_voltage_text(p_v));

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

// === Button detection and debounce ===

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_3)
    {
        uint32_t now = HAL_GetTick();

        if ((now - lastButtonInterrupt) > BUTTON_DEBOUNCE_PERIOD)
        {
            lastButtonInterrupt = now;
            buttonPressed = 1;
        }
    }

}

// === Status/ fault update to HV ===

static void LV_status_share(lv_timer_t * t)
{
    (void)t;

    if (BMS_error_code == 0xFFFF) 
    {
        printfDma("\\F 1\n");
    } else if (BMS_error_code == 0x00FF || BMS_error_code == 0x0000) 
    {
        printfDma("\\F 0\n");
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
