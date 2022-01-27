/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : main module, the only hardware dependent one
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>

#include "basic.h"
#include "common.h"
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

/* USER CODE BEGIN PV */
uint8_t __attribute__((section(".Continuous"))) ram[RAMSIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
void lcd_byte(unsigned int x);
int extra_commands(int i);
unsigned int i2c_send_byte(unsigned int x);
unsigned int i2c_receive_byte(unsigned int ack);
unsigned int i2c_reset(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_direction(uint32_t x);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* equivalent of writing to the address range 0x0080-0x00FF,
 the pins SYNC, SHIFT, ADO should be kept at low level whenever possible
 in order to minimize the 74HCT4053 power consumption */
void lcd(unsigned int addr, uint8_t data)
{
    HAL_GPIO_WritePin(SYNC_GPIO_Port, SYNC_Pin, GPIO_PIN_SET);
    lcd_byte(addr);
    HAL_GPIO_WritePin(SYNC_GPIO_Port, SYNC_Pin, GPIO_PIN_RESET);
    lcd_byte(data);
    HAL_GPIO_WritePin(SYNC_GPIO_Port, SYNC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SYNC_GPIO_Port, SYNC_Pin, GPIO_PIN_RESET);
}

/* transmit a byte to the LCD controller */
void lcd_byte(unsigned int x)
{
    unsigned int m;
    for (m = 0x01; m <= 0x80; m <<= 1) {
        ADO_GPIO_Port->BSRR = ((x & m) == 0) ? ADO_Pin : (uint32_t)ADO_Pin << 16u;
        SHIFT_GPIO_Port->BSRR = SHIFT_Pin;                  /* SHIFT=1 */
        SHIFT_GPIO_Port->BSRR = (uint32_t)SHIFT_Pin << 16u; /* SHIFT=0 */
        ADO_GPIO_Port->BSRR = (uint32_t)ADO_Pin << 16u;     /* ADO=0 */
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == STOP_KEY_Pin || GPIO_Pin == OFF_SW_Pin) {
        /* 00F4: */
        ram[0x8265u - RAMSTART] |= 0x01;
    }
}

/* roughly 0144: stops the CPU then waits for a key to be pressed */
void key_wait(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    if (HAL_GPIO_ReadPin(KEYB_GPIO_Port, OFF_SW_Pin) == 0) {
        /* power off, executing this code directly in the interrupt handler routine
           leaves the processor in a state of increased current consumption */
        HAL_GPIO_WritePin(GPIOA, SYNC_Pin | SHIFT_Pin | PWR_Pin | ADO_Pin, GPIO_PIN_RESET);
        GPIO_InitStruct.Pin = PPX_Pins;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        HAL_NVIC_DisableIRQ(EXTI0_IRQn);
        HAL_NVIC_DisableIRQ(EXTI1_IRQn);
        HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
        while (1) {
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        }
    }
    HAL_GPIO_WritePin(KEYB_GPIO_Port, PPX_Pins, GPIO_PIN_RESET);
    if ((~KEYB_GPIO_Port->IDR & KBX_Pins) == 0) {
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
}

#define NDEBOUNCE 4
#define NKEYS 20
#define KEY_SCAN_DELAY 2
#define KEY_REPEAT_DELAY 72
#define KEY_REPEAT_PERIOD 8

/* roughly 0820 to 08DA: returns the scan code of the pressed key, or 0 if the
 key is not pressed or not recognized */
int key_scan(void)
{
    /* 0051: table of keyboard bit patterns */
    const unsigned int tab0051[NKEYS] = {
        KB2_Pin | KB3_Pin, /* W, Mode, AC */
        KB2_Pin | KB4_Pin, /* S, <-, Del */
        KB2_Pin | KB5_Pin, /* F, [S], / */
        KB2_Pin | KB6_Pin, /* Q, U, 9 */
        KB2_Pin | KB7_Pin, /* C, H, 6 */
        KB3_Pin | KB4_Pin, /* E, ->, Init */
        KB3_Pin | KB5_Pin, /* G, [F], 7 */
        KB3_Pin | KB6_Pin, /* X, I, * */
        KB3_Pin | KB7_Pin, /* A, J, - */
        KB4_Pin | KB5_Pin, /* Z, Y, 8 */
        KB4_Pin | KB6_Pin, /* D, O, 4 */
        KB4_Pin | KB7_Pin, /* R, K, 1 */
        KB5_Pin | KB6_Pin, /* B, P, 5 */
        KB5_Pin | KB7_Pin, /* T, L, 2 */
        KB6_Pin | KB7_Pin, /* V, ANS, 3 */
        KB2_Pin | KB8_Pin, /* none, M, + */
        KB3_Pin | KB8_Pin, /* none, N, 0 */
        KB4_Pin | KB8_Pin, /* none, space, dot */
        KB5_Pin | KB8_Pin, /* none, =, EXE */
        KB6_Pin | KB8_Pin  /* none, EE, none */
    };
    /* 0946: conversion table: index to tab0051 -> key scan code */
    const int tab0946[3][20] = {                           /* 0946: row PP3 */
                                 { 0x33, 0x2F, 0x22, 0x2D, /* W, S, F, Q */
                                   0x1F, 0x21, 0x23, 0x34, /* C, E, G, X */
                                   0x1D, 0x36, 0x20, 0x2E, /* A, Z, D, R */
                                   0x1E, 0x30, 0x32, 0x00, /* B, T, V */
                                   0x00, 0x00, 0x00, 0x00 },
                                 /* 0955: row PP2 */
                                 { 0x01, 0x05, 0x02, 0x31, /* Mode, <-, [S], U */
                                   0x24, 0x06, 0x03, 0x25, /* H, ->, [F], I */
                                   0x26, 0x35, 0x2B, 0x27, /* J, Y, O, K */
                                   0x2C, 0x28, 0x09, 0x29, /* P, L, ANS, M */
                                   0x2A, 0x15, 0x1C, 0x1B /* N, space, = EE */ },
                                 /* 0969: row PP1 */
                                 { 0x07, 0x08, 0x1A, 0x14, /* AC, DEL, /, 9 */
                                   0x11, 0x0A, 0x12, 0x19, /* 6, Init, 7, * */
                                   0x17, 0x13, 0x0F, 0x0C, /* -, 8, 4, 1 */
                                   0x10, 0x0D, 0x0E, 0x18, /* 5, 2, 3, + */
                                   0x0B, 0x16, 0x04, 0x00 /* 0, dot, EXE */ }
    };
    const unsigned int rowpins[3] = { PP3_Pin, PP2_Pin, PP1_Pin };
    int i, j;
    uint16_t x, y;

    for (i = 0; i < 3; ++i) {
        /* select the keyboard matrix row */
        HAL_GPIO_WritePin(KEYB_GPIO_Port, PPX_Pins & ~rowpins[i], GPIO_PIN_SET);
        HAL_GPIO_WritePin(KEYB_GPIO_Port, rowpins[i], GPIO_PIN_RESET);
        delay_ms(KEY_SCAN_DELAY);
        x = ~KEYB_GPIO_Port->IDR & KEYB_Pins;
        y = x & KBX_Pins;
        if (y != 0) {
            /* 083C: key debounce loop */
            for (j = 0; j < NDEBOUNCE; ++j) {
                delay_ms(KEY_SCAN_DELAY);
                if (y != (~KEYB_GPIO_Port->IDR & KBX_Pins)) {
                    break;
                }
            }
            if (j == NDEBOUNCE) {
                /* 0848: key accepted */
                if (x != read16(0x825E /*previous keyboard state*/)) {
                    /* 0886: keyboard state changed */
                    ram[0x8266u /*keyboard timer*/ - RAMSTART] = 0;
                } else {
                    /* 085A: still the same key pressed, test the keyboard timer for autorepeat */
                    if (++ram[0x8266u /*keyboard timer*/ - RAMSTART] < KEY_REPEAT_DELAY) {
                        /* 08A4: */
                        HAL_GPIO_WritePin(KEYB_GPIO_Port, PPX_Pins, GPIO_PIN_RESET);
                        return 0;
                    }
                    /* 0872: */
                    ram[0x8266u - RAMSTART] -= KEY_REPEAT_PERIOD;
                    ram[0x8262u - RAMSTART] &= 0x30; /* bits [S], [F] */
                    ram[0x8263u - RAMSTART] &= 0x20; /* bit mode_on */
                                                     /* 087E: copy keyboard mode to mode */
                    ram[0x8256u - RAMSTART] |= ram[0x8262u - RAMSTART];
                    ram[0x8257u - RAMSTART] |= ram[0x8263u - RAMSTART];
                }
                /* 088A: identify the keyboard bit pattern */
                for (j = 0; j < NKEYS; ++j) {
                    if (y == tab0051[j]) {
                        /* 08B4: valid pattern found */
                        write16(0x825E /*previous keyboard state*/, x);
                        /* 08BC: copy mode to keyboard mode */
                        ram[0x8262u - RAMSTART] = ram[0x8256u - RAMSTART];
                        ram[0x8262u - RAMSTART] = ram[0x8256u - RAMSTART];
                        HAL_GPIO_WritePin(KEYB_GPIO_Port, PPX_Pins, GPIO_PIN_RESET);
                        return tab0946[i][j];
                    }
                }
            }
        }
    }
    /* 089C: key not pressed or not identified */
    ram[0x825Eu - RAMSTART] = 0;
    ram[0x825Fu - RAMSTART] = 0;
    ram[0x8266u - RAMSTART] = 0;
    /* 08A4: */
    HAL_GPIO_WritePin(KEYB_GPIO_Port, PPX_Pins, GPIO_PIN_RESET);
    return 0;
}

void delay_ms(unsigned int x)
{
    HAL_Delay(x);
}

#define NEXTRA 2 /* number of extra commands */
#define DEVICEADDRESS 0xA0
#define WRPAGESIZE 64  /* must be power of two */
#define RDPAGESIZE 512 /* must be power of two */
/* RAMSIZE must be a multiple of WRPAGESIZE and RDPAGESIZE */

/* the extra direct mode commands LOAD and SAVE transfer the "ram" buffer
 contents from/to an I2C EEPROM Microchip 24AA128 */
int extra_commands(int i /*error code*/)
{
    const char *tab_extra[NEXTRA] = { "LOAD", "SAVE" };
    unsigned int cmd, mem_addr, page_size, page_addr, attempt, z;
    uint8_t x;

    if (i != ERR2 || ((ram[0x8257u - RAMSTART] & 0x80) != 0 /*program execution mode*/ &&
                      (ram[0x8256u - RAMSTART] & 0x01) == 0 /*not a STOP mode*/)) {
        return i;
    }
    write16(0x8254, 0x816D);
    for (cmd = 0; cmd < NEXTRA; ++cmd) {
        i = 0;
        z = 0x816Du - RAMSTART; /* input line buffer */
        do {
            x = tab_extra[cmd][i++];
        } while (x == ram[z++] && x != 0);
        if (x == 0 && ram[--z] == 0) {
            /* command found */
            ram[0x8265u - RAMSTART] = 0;
            if (i2c_reset() != 0) {
                return ERR9;
            }
            page_size = (cmd == 0) ? RDPAGESIZE - 1 : WRPAGESIZE - 1;
            i = 0;    /* accumulator */
            z = 0x81; /* LCD address */
            x = 0x01; /* LCD data */
                      /* data transfer, can be terminated with the STOP key */
            for (mem_addr = 0; mem_addr < RAMSIZE; ++mem_addr) {
                page_addr = mem_addr & page_size;
                if (page_addr == 0) {
                    if ((*(volatile uint8_t *)&ram[0x8265u - RAMSTART] & 0x01) != 0) {
                        /* the STOP key was pressed */
                        break;
                    }
                    /* keep sending the device address until acknowledged */
                    attempt = 11;
                    while (1) {
                        i2c_start();
                        if (i2c_send_byte(DEVICEADDRESS) == 0) {
                            break;
                        }
                        i2c_stop();
                        if (--attempt == 0) {
                            return ERR9; /* time out */
                        }
                        delay_ms(2);
                    }
                    /* location address */
                    if (i2c_send_byte(mem_addr >> 8) != 0 || i2c_send_byte(mem_addr) != 0) {
                        /* the transmitted bytes weren't acknowledged */
                        i2c_stop();
                        return ERR9;
                    }
                    if (cmd == 0) {
                        /* restart */
                        HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
                        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
                        i2c_start();
                        /* device address + read */
                        if (i2c_send_byte(DEVICEADDRESS | 1) != 0) {
                            /* the transmitted byte wasn't acknowledged */
                            i2c_stop();
                            return ERR9;
                        }
                    }
                }

                if (cmd == 0) {
                    /* the last received byte must not be acknowledged */
                    ram[mem_addr] = i2c_receive_byte(page_addr == page_size);
                } else {
                    if (i2c_send_byte(ram[mem_addr]) != 0) {
                        /* the transmitted byte wasn't acknowledged */
                        i2c_stop();
                        return ERR9;
                    }
                }

                if (page_addr == page_size) {
                    i2c_stop();
                }

                /* bar graph, advanced every RAMSIZE / (5 * SCREEN_WIDTH) transferred bytes */
                i += 5 * SCREEN_WIDTH; /* screen width in pixels */
                if (i >= RAMSIZE) {
                    i -= RAMSIZE;
                    do {
                        lcd(z, x);
                    } while ((++z & 7) != 0);
                    z -= 7;
                    x |= (x << 1);
                    if (x > 0x1F) {
                        x = 0x01;
                        z += 8;
                    }
                }
            }

            /* transfer complete */
            reset();
            /* 0B9C: */
            ram[0x8265u - RAMSTART] = 0;
            ram[0x8264u - RAMSTART] &= 0x20;
            return 0;
        }
    }
    return ERR2; /* command not found */
}

/* send a byte then receive ACK */
unsigned int i2c_send_byte(unsigned int x)
{
    unsigned int m;

    for (m = 0x80; m != 0; m >>= 1) {
        HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, x & m);
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    }
    i2c_direction(GPIO_MODE_INPUT);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    x = HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    i2c_direction(GPIO_MODE_OUTPUT_OD);
    return x;
}

/* receive a byte then send ACK */
unsigned int i2c_receive_byte(unsigned int ack)
{
    unsigned int m, x;

    x = 0;
    i2c_direction(GPIO_MODE_INPUT);
    for (m = 0x80; m != 0; m >>= 1) {
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
        if (HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) != 0) {
            x |= m;
        }
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    }
    i2c_direction(GPIO_MODE_OUTPUT_OD);
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, ack);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    return x;
}

unsigned int i2c_reset(void)
{
    int i;

    HAL_GPIO_WritePin(GPIOA, SDA_Pin | SCL_Pin, GPIO_PIN_SET);
    i2c_direction(GPIO_MODE_INPUT);
    /* send up to 9 SCL pulses until there is a high level on the SDA line */
    i = 9;
    while (1) {
        if (HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) != 0) {
            i2c_direction(GPIO_MODE_OUTPUT_OD);
            i2c_start();
            i2c_stop();
            return 0; /* succeeded */
        }
        if (i == 0) {
            return 1; /* failed to unlock the I2C bus */
        }
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
        --i;
    }
}

void i2c_start(void)
{
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
}

void i2c_stop(void)
{
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
}

/* big and slow, but safe */
void i2c_direction(uint32_t x /* GPIO_MODE_OUTPUT_OD or GPIO_MODE_INPUT */)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin = SDA_Pin;
    GPIO_InitStruct.Mode = x;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SDA_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */
    int i;
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
    /* USER CODE BEGIN 2 */
    reset();
    /* 0B9C: */
    ram[0x8265u - RAMSTART] = 0;
    ram[0x8264u - RAMSTART] &= 0x20;
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        do {
            ram[0x8258u - RAMSTART] = 0;
            ram[0x8259u - RAMSTART] = 0;
            i = extra_commands(do_direct());
        } while (i >= 0 || i == DONE);
        error_handler(i);
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
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV4;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, SYNC_Pin | SHIFT_Pin | ADO_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, PWR_Pin | SDA_Pin | SCL_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, PP1_Pin | PP2_Pin | PP3_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : LED_Pin */
    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : PC14 PC15 */
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : PD0 PD1 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /*Configure GPIO pins : PA0 PA1 PA2 PA3
                             PA8 PA9 PA10 PA11
                             PA12 PA15 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_8 |
                          GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : SYNC_Pin SHIFT_Pin PWR_Pin ADO_Pin */
    GPIO_InitStruct.Pin = SYNC_Pin | SHIFT_Pin | PWR_Pin | ADO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : STOP_KEY_Pin OFF_SW_Pin KB3_Pin KB4_Pin
                             KB5_Pin KB6_Pin KB7_Pin KB8_Pin */
    GPIO_InitStruct.Pin =
        STOP_KEY_Pin | OFF_SW_Pin | KB3_Pin | KB4_Pin | KB5_Pin | KB6_Pin | KB7_Pin | KB8_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : PB2 PB3 PB4 PB5 */
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : SDA_Pin SCL_Pin */
    GPIO_InitStruct.Pin = SDA_Pin | SCL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : PP1_Pin PP2_Pin PP3_Pin */
    GPIO_InitStruct.Pin = PP1_Pin | PP2_Pin | PP3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : KB2_Pin */
    GPIO_InitStruct.Pin = KB2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KB2_GPIO_Port, &GPIO_InitStruct);

    /*Configure peripheral I/O remapping */
    __HAL_AFIO_REMAP_PD01_ENABLE();

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
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
    while (1) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(100);
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
       tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
