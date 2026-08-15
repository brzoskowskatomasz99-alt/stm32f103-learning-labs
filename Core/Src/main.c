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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include "gpio.h"
#include "dma.h"
#include "adc.h"
#include "usart.h"
#include "tim.h"
#include "app_main.h"
#include "llcc68_diag.h"
#include "llcc68_p2p.h"
#include "llcc68_p2p_config.h"
#include "esp.h"
#include "mqtt.h"
#include "dht22.h"
#include "light.h"
#include "terminal_sensors.h"
#include "terminal_autonomy.h"
#include "command_link.h"
#include "terminal_table.h"
#include "link_stats.h"
#include "alarm_registry.h"
#include "gateway_data.h"
#include "ui_oled.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* I2C/OLED 实验不使用按键扫描。 */


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_RX )
static uint32_t esp_retry_tick = 0U;
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if 0
/* 原环境监测项目的按键扫描逻辑：本 I2C/OLED 实验暂不使用。 */
typedef enum {
    KEY_IDLE,   // 空闲
    KEY_DOWN,   // 按下
    KEY_UP      // 松开
} KEY_STATE;

typedef enum {
    KEY_NONE,   // 无动作
    KEY_CLICK,  // 单击
    KEY_DOUBLE, // 双击
    KEY_LONG    // 长按
} KEY_EVENT;

KEY_EVENT key_scan(void) {
    static KEY_STATE state = KEY_IDLE;
    static uint32_t press_time = 0;
    static uint32_t release_time = 0;
    static uint8_t click_count = 0;

    // 读取 SW1（PB12）的电平
    uint8_t key = HAL_GPIO_ReadPin(SW1_GPIO_Port, SW1_Pin);

    switch(state) {
        case KEY_IDLE:
            if(key == GPIO_PIN_RESET) {
                press_time = HAL_GetTick();
                state = KEY_DOWN;
            }
            break;
        case KEY_DOWN:
            if(key == GPIO_PIN_SET) {
                state = KEY_UP;
                release_time = HAL_GetTick();
                click_count++;
            }
            break;
        case KEY_UP:
            if(key == GPIO_PIN_RESET) {
                if(HAL_GetTick() - release_time < 300) {
                    state = KEY_DOWN;
                    press_time = HAL_GetTick();
                }
            } else {
                if(HAL_GetTick() - release_time > 300) {
                    state = KEY_IDLE;
                    if(click_count == 1) {
                        click_count = 0;
                        if(HAL_GetTick() - press_time >= 1500) {
                            return KEY_LONG;
                        }
                        return KEY_CLICK;
                    }
                    if(click_count >= 2) {
                        click_count = 0;
                        return KEY_DOUBLE;
                    }
                }
            }
            break;
    }
    return KEY_NONE;
}
#endif

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
  /* TIM1_CH1(PA8) 是终端风机 PWM，两个角色都必须初始化 */
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
	
	
	// HAL_TIM_Base_Start_IT(&htim1);
	// HAL_TIM_Base_Start_IT(&htim3);
	//HAL_TIM_Base_Start_IT(&htim4);
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_RX )
	printf("[FW] GATEWAY CONTROL-SUB FIX 20260814-1\r\n");
#else
	printf("[FW] TERMINAL CONTROL 20260814-1\r\n");
#endif
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
	if (!TerminalSensors_Init())
	{
	    printf("Terminal sensor init warning\r\n");
	}
	if (!TerminalAutonomy_Init())
	{
	    printf("Terminal autonomy init warning\r\n");
	}
#else
	Light_Led2_Init();
#endif
	if (!LLCC68_P2P_Init())
	{
	    printf("LLCC68 P2P Init Error\r\n");
	}
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_RX )
	/* 网关本地模块与 ESP/MQTT 解耦：ESP 失败时 OLED/在线表/命令链路照常工作 */
	CommandLink_Init();
	TerminalTable_Init();
	LinkStats_Init();
	AlarmRegistry_Init();
	GatewayData_Init();
	UiOled_Init();
	esp_retry_tick = HAL_GetTick();
	if (ESP_Init() != 0)
	{
	    printf("ESP INIT FAIL\r\n");
	}
	else
	{
	    mqtt_init();
	}
#endif
	/* 阶段 6 诊断入口运行调用已注释（llcc68_diag 模块文件保留）。 */
	// LLCC68_Diag_RunOnce();
	/* app_main() 内含永续 while(1)（app_main.c:624），与 while(1) 中 P2P 轮询
	   冲突；仅注释其调用，app_main.c 未修改。 */
	// app_main();
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
    TerminalSensors_Process();
    TerminalAutonomy_Process();
#endif
    LLCC68_P2P_Process();
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_RX )
    LinkStats_Process(HAL_GetTick());
    CommandLink_Process();
    mqtt_task_loop();
    /* ESP/MQTT 断线自动重试：每 60 s 一次（T04 断网恢复） */
    if ((!mqtt_is_connected()) &&
        ((HAL_GetTick() - esp_retry_tick) >= 60000U))
    {
        esp_retry_tick = HAL_GetTick();
        printf("ESP RETRY\r\n");
        if (ESP_Init() == 0)
        {
            mqtt_init();
        }
        else
        {
            printf("ESP FAIL\r\n");
        }
    }
    {
        UiOledTelemetry ui_telemetry;
        UiOledStatus ui_status;

        GatewayData_BuildUiData(&ui_telemetry, &ui_status);
        UiOled_SetData(&ui_telemetry, &ui_status);
        UiOled_Process();
        if (UiOled_GetSilenceRequest())
        {
            printf("[UI] SILENCE OK\r\n");
        }
    }
#endif
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

#if 0
/* 串口 printf 重定向：本 I2C/OLED 实验不使用 USART1。 */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);

    return ch;
}
#endif

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
