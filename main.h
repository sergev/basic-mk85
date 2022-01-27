/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define RAMSIZE 0x2800u
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void lcd (unsigned int addr, uint8_t data);
void key_wait (void);
int key_scan (void);
void delay_ms (unsigned int x);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define SYNC_Pin GPIO_PIN_4
#define SYNC_GPIO_Port GPIOA
#define SHIFT_Pin GPIO_PIN_5
#define SHIFT_GPIO_Port GPIOA
#define PWR_Pin GPIO_PIN_6
#define PWR_GPIO_Port GPIOA
#define ADO_Pin GPIO_PIN_7
#define ADO_GPIO_Port GPIOA
#define STOP_KEY_Pin GPIO_PIN_0
#define STOP_KEY_GPIO_Port GPIOB
#define STOP_KEY_EXTI_IRQn EXTI0_IRQn
#define OFF_SW_Pin GPIO_PIN_1
#define OFF_SW_GPIO_Port GPIOB
#define OFF_SW_EXTI_IRQn EXTI1_IRQn
#define KB3_Pin GPIO_PIN_10
#define KB3_GPIO_Port GPIOB
#define KB3_EXTI_IRQn EXTI15_10_IRQn
#define KB4_Pin GPIO_PIN_11
#define KB4_GPIO_Port GPIOB
#define KB4_EXTI_IRQn EXTI15_10_IRQn
#define KB5_Pin GPIO_PIN_12
#define KB5_GPIO_Port GPIOB
#define KB5_EXTI_IRQn EXTI15_10_IRQn
#define KB6_Pin GPIO_PIN_13
#define KB6_GPIO_Port GPIOB
#define KB6_EXTI_IRQn EXTI15_10_IRQn
#define KB7_Pin GPIO_PIN_14
#define KB7_GPIO_Port GPIOB
#define KB7_EXTI_IRQn EXTI15_10_IRQn
#define KB8_Pin GPIO_PIN_15
#define KB8_GPIO_Port GPIOB
#define KB8_EXTI_IRQn EXTI15_10_IRQn
#define SDA_Pin GPIO_PIN_13
#define SDA_GPIO_Port GPIOA
#define SCL_Pin GPIO_PIN_14
#define SCL_GPIO_Port GPIOA
#define PP1_Pin GPIO_PIN_6
#define PP1_GPIO_Port GPIOB
#define PP2_Pin GPIO_PIN_7
#define PP2_GPIO_Port GPIOB
#define PP3_Pin GPIO_PIN_8
#define PP3_GPIO_Port GPIOB
#define KB2_Pin GPIO_PIN_9
#define KB2_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */
#define KEYB_GPIO_Port PP1_GPIO_Port
#define PPX_Pins (PP1_Pin | PP2_Pin | PP3_Pin)
#define KBX_Pins (KB2_Pin | KB3_Pin | KB4_Pin | KB5_Pin | KB6_Pin |	\
		KB7_Pin | KB8_Pin)
#define KEYB_Pins (PPX_Pins | KBX_Pins)
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
