/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef _BOARD_H_
#define _BOARD_H_

/*
 * Setup for Defcon 25 Ides of March badge board
 */

/*
 * Board identifier.
 */
#define BOARD_KOSAGI_ORCHARD
#define BOARD_NAME                  "KW01 Ides Of March"

/* Ides of March KW012 when using FEI mode. */

#if notdef
#define KINETIS_SYSCLK_FREQUENCY    47972352UL
#define KINETIS_MCG_MODE            KINETIS_MCG_MODE_FEI
#endif

/* Ides of March KW012 when using PEE mode. */

/* External 32 MHz clock from radio with PLL for 48 MHz core/system clock. */
#define KINETIS_SYSCLK_FREQUENCY    48000000UL 
#define KINETIS_MCG_MODE            KINETIS_MCG_MODE_PEE

/* GPIO pin assignments */

#define MMC_CHIP_SELECT_PORT	GPIOB
#define MMC_CHIP_SELECT_PIN	0

#define XPT_CHIP_SELECT_PORT	GPIOE
#define XPT_CHIP_SELECT_PIN	19

#define SCREEN_CHIP_SELECT_PORT	GPIOD
#define SCREEN_CHIP_SELECT_PIN	4

#define SCREEN_CMDDATA_PORT	GPIOE
#define SCREEN_CMDDATA_PIN	18

#define LED_CHAIN_PORT		GPIOE
#define LED_CHAIN_PIN		17

#define RED_LED_PORT		GPIOE
#define RED_LED_PIN		16

/* Note: actually PTB1 on Freedom board */

#define GREEN_LED_PORT		GPIOD
#define GREEN_LED_PIN		7

#define BLUE_LED_PORT		GPIOB
#define BLUE_LED_PIN		0

/* Also used for LED chain. */

#define BLUE_SOLO_LED_PORT	GPIOE
#define BLUE_SOLO_LED_PIN	17

#define TPM0_PORT		GPIOC
#define TPM0_PIN		2

#define TPM1_PORT		GPIOB
#define TPM1_PIN		1

#define TPM2_PORT		GPIOB
#define TPM2_PIN		2

#define RADIO_CHIP_SELECT_PORT	GPIOD
#define RADIO_CHIP_SELECT_PIN	0

#define RADIO_RESET_PORT	GPIOE
#define RADIO_RESET_PIN		30

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif

  extern uint32_t __storage_start__[];
  extern uint32_t __storage_size__[];
  extern uint32_t __storage_end__[];

  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */
