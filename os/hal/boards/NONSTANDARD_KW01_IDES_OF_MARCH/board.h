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

#define BOARD_KW01_IDES_OF_MARCH
#define BOARD_NAME                  "KW01 Ides Of March"

/*
 * The Ides of March design is derived from the Freescale/NXP
 * Freedom KW019032 reference board. The following configuration actually works
 * with both boards, except for the green blinky LED used to indicate radio RX
 * activity. (We need the pin for the Freedom board's green LED for one of the
 * sound channels.) If you ignore that, they both work the same.
 * 
 * There are two possible CPU clocking options: one is to use the internal
 * 32.768KHz reference oscillator (FEI mode) and the other is to use the clock
 * from radio, which uses a 32MHz reference crystal (PEE mode). Both yield a
 * 48Mhz CPU clock and 24MHz bus clock. It's not clear just how accurate or
 * stable the internal reference clock is though, so we prefer the external
 * reference clock for now.
 */

/* Ides of March KW01 when using FEI mode. */

#if notdef
#define KINETIS_SYSCLK_FREQUENCY    47972352UL
#define KINETIS_MCG_MODE            KINETIS_MCG_MODE_FEI
#endif

/* Ides of March KW01 when using PEE mode. */

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

#define GREEN_LED_PORT		GPIOB
#define GREEN_LED_PIN		1

#define BLUE_LED_PORT		GPIOB
#define BLUE_LED_PIN		0

/* Also used for LED chain. */

#define BLUE_SOLO_LED_PORT	GPIOE
#define BLUE_SOLO_LED_PIN	17

#define TPM2_PORT		GPIOB
#define TPM2_PIN		2

#define RADIO_CHIP_SELECT_PORT	GPIOD
#define RADIO_CHIP_SELECT_PIN	0

#define RADIO_RESET_PORT	GPIOC
#define RADIO_RESET_PIN		2

/* DAC audio */

#define DAC_PORT		GPIOE
#define DAC_PIN			30

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
