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

#include "hal.h"
#include "radio_reg.h"
#include "pit.h"
#include "pit_reg.h"

#define RADIO_REG_DIOMAPPING2   (0x26)
#define RADIO_CLK_DIV1          (0x00)
#define RADIO_CLK_DIV2          (0x01)
#define RADIO_CLK_DIV4          (0x02)
#define RADIO_CLK_DIV8          (0x03)
#define RADIO_CLK_DIV16         (0x04)
#define RADIO_CLK_DIV32         (0x05)
#define RADIO_CLK_RC            (0x06)
#define RADIO_CLK_OFF           (0x07)

/*

The part that I have has the following pins exposed:
  PTA0
  PTA1
  PTA2
  PTA3
  PTA4
  PTA18
  PTA19
  PTA20
  PTB0
  PTB1
  PTB2
  PTB17
  PTC1
  PTC2
  PTC3
  PTC4
  PTC5 -- Bottom pad, also internal connection to transciever SCK -- SPI clock
  PTC6 -- Bottom pad, also internal connection to transciever MOSI -- SPI MISO
  PTC7 -- Bottom pad, also internal connection to transciever MISO -- SPI MOSI
  PTD0 -- Bottom pad, also internal connection to transciever NSS -- SPI CS
  PTD4
  PTD5
  PTD6
  PTD7
  PTE0
  PTE1
  PTE2 -- Internal connection to transciever GPIO DIO0 -- SPI1_SCK
  PTE3 -- Internal connection to transciever GPIO DIO1 -- SPI1_MOSI
  PTE16
  PTE17
  PTE18
  PTE19
  PTE30
*/

#if HAL_USE_PAL || defined(__DOXYGEN__)
/**
 * @brief   PAL setup.
 * @details Digital I/O ports static configuration as defined in @p board.h.
 *          This variable is used by the HAL when initializing the PAL driver.
 */
const PALConfig pal_default_config =
{
  .ports = {
    {
      .port = IOPORT1,  // PORTA
      .pads = {
#if ORCHARD_BOARD_REV == ORCHARD_REV_EVT1
        /* PTA0*/ PAL_MODE_ALTERNATIVE_7,   /* PTA1*/ PAL_MODE_ALTERNATIVE_3,   /* PTA2*/ PAL_MODE_ALTERNATIVE_3,
#else
        /* ECO10: allow fast Tx filling of radio packets by wiring D1 to PTD1 */
        /* PTA0*/ PAL_MODE_ALTERNATIVE_7,   /* PTA1*/ PAL_MODE_ALTERNATIVE_2,   /* PTA2*/ PAL_MODE_ALTERNATIVE_2,
#endif
        /* PTA3*/ PAL_MODE_ALTERNATIVE_7,   /* PTA4*/ PAL_MODE_ALTERNATIVE_3,   /* PTA5*/ PAL_MODE_UNCONNECTED,
        /* PTA6*/ PAL_MODE_UNCONNECTED,     /* PTA7*/ PAL_MODE_UNCONNECTED,     /* PTA8*/ PAL_MODE_UNCONNECTED,
        /* PTA9*/ PAL_MODE_UNCONNECTED,     /*PTA10*/ PAL_MODE_UNCONNECTED,     /*PTA11*/ PAL_MODE_UNCONNECTED,
        /*PTA12*/ PAL_MODE_UNCONNECTED,     /*PTA13*/ PAL_MODE_UNCONNECTED,     /*PTA14*/ PAL_MODE_UNCONNECTED,
        /*PTA15*/ PAL_MODE_UNCONNECTED,     /*PTA16*/ PAL_MODE_UNCONNECTED,     /*PTA17*/ PAL_MODE_UNCONNECTED,
        /*PTA18*/ PAL_MODE_INPUT_ANALOG,    /*PTA19*/ PAL_MODE_OUTPUT_PUSHPULL, /*PTA20*/ PAL_MODE_ALTERNATIVE_7,
        /*PTA21*/ PAL_MODE_UNCONNECTED,     /*PTA22*/ PAL_MODE_UNCONNECTED,     /*PTA23*/ PAL_MODE_UNCONNECTED,
        /*PTA24*/ PAL_MODE_UNCONNECTED,     /*PTA25*/ PAL_MODE_UNCONNECTED,     /*PTA26*/ PAL_MODE_UNCONNECTED,
        /*PTA27*/ PAL_MODE_UNCONNECTED,     /*PTA28*/ PAL_MODE_UNCONNECTED,     /*PTA29*/ PAL_MODE_UNCONNECTED,
        /*PTA30*/ PAL_MODE_UNCONNECTED,     /*PTA31*/ PAL_MODE_UNCONNECTED,
      },
    },
    {
      .port = IOPORT2,  // PORTB
      .pads = {
#if ORCHARD_BOARD_REV == ORCHARD_REV_EVT1
        /* PTB0*/ PAL_MODE_INPUT_PULLUP,    /* PTB1*/ PAL_MODE_ALTERNATIVE_3,   /* PTB2*/ PAL_MODE_ALTERNATIVE_3,
#else
  /* ECO9: Swap B0/D4 DC/INT to allow GPIOX to fire interrupts */
        /* PTB0*/ PAL_MODE_OUTPUT_PUSHPULL, /* PTB1*/ PAL_MODE_ALTERNATIVE_3,   /* PTB2*/ PAL_MODE_ALTERNATIVE_3,
#endif
        /* PTB3*/ PAL_MODE_UNCONNECTED,     /* PTB4*/ PAL_MODE_UNCONNECTED,     /* PTB5*/ PAL_MODE_UNCONNECTED,
        /* PTB6*/ PAL_MODE_UNCONNECTED,     /* PTB7*/ PAL_MODE_UNCONNECTED,     /* PTB8*/ PAL_MODE_UNCONNECTED,
        /* PTB9*/ PAL_MODE_UNCONNECTED,     /*PTB10*/ PAL_MODE_UNCONNECTED,     /*PTB11*/ PAL_MODE_UNCONNECTED,
        /*PTB12*/ PAL_MODE_UNCONNECTED,     /*PTB13*/ PAL_MODE_UNCONNECTED,     /*PTB14*/ PAL_MODE_UNCONNECTED,
        /*PTB15*/ PAL_MODE_UNCONNECTED,     /*PTB16*/ PAL_MODE_UNCONNECTED,     /*PTB17*/ PAL_MODE_INPUT_ANALOG,
        /*PTB18*/ PAL_MODE_UNCONNECTED,     /*PTB19*/ PAL_MODE_UNCONNECTED,     /*PTB20*/ PAL_MODE_UNCONNECTED,
        /*PTB21*/ PAL_MODE_UNCONNECTED,     /*PTB22*/ PAL_MODE_UNCONNECTED,     /*PTB23*/ PAL_MODE_UNCONNECTED,
        /*PTB24*/ PAL_MODE_UNCONNECTED,     /*PTB25*/ PAL_MODE_UNCONNECTED,     /*PTB26*/ PAL_MODE_UNCONNECTED,
        /*PTB27*/ PAL_MODE_UNCONNECTED,     /*PTB28*/ PAL_MODE_UNCONNECTED,     /*PTB29*/ PAL_MODE_UNCONNECTED,
        /*PTB30*/ PAL_MODE_UNCONNECTED,     /*PTB31*/ PAL_MODE_UNCONNECTED,
      },
    },
    {
      .port = IOPORT3,  // PORTC
      .pads = {
        /* PTC0*/ PAL_MODE_INPUT_ANALOG,    /* PTC1*/ PAL_MODE_ALTERNATIVE_2,   /* PTC2*/ PAL_MODE_ALTERNATIVE_4,
        /* PTC3*/ PAL_MODE_INPUT,           /* PTC4*/ PAL_MODE_INPUT,           /* PTC5*/ PAL_MODE_ALTERNATIVE_2,
        /* PTC6*/ PAL_MODE_ALTERNATIVE_2,   /* PTC7*/ PAL_MODE_ALTERNATIVE_2,   /* PTC8*/ PAL_MODE_UNCONNECTED,
        /* PTC9*/ PAL_MODE_UNCONNECTED,     /*PTC10*/ PAL_MODE_UNCONNECTED,     /*PTC11*/ PAL_MODE_UNCONNECTED,
        /*PTC12*/ PAL_MODE_UNCONNECTED,     /*PTC13*/ PAL_MODE_UNCONNECTED,     /*PTC14*/ PAL_MODE_UNCONNECTED,
        /*PTC15*/ PAL_MODE_UNCONNECTED,     /*PTC16*/ PAL_MODE_UNCONNECTED,     /*PTC17*/ PAL_MODE_UNCONNECTED,
        /*PTC18*/ PAL_MODE_UNCONNECTED,     /*PTC19*/ PAL_MODE_UNCONNECTED,     /*PTC20*/ PAL_MODE_UNCONNECTED,
        /*PTC21*/ PAL_MODE_UNCONNECTED,     /*PTC22*/ PAL_MODE_UNCONNECTED,     /*PTC23*/ PAL_MODE_UNCONNECTED,
        /*PTC24*/ PAL_MODE_UNCONNECTED,     /*PTC25*/ PAL_MODE_UNCONNECTED,     /*PTC26*/ PAL_MODE_UNCONNECTED,
        /*PTC27*/ PAL_MODE_UNCONNECTED,     /*PTC28*/ PAL_MODE_UNCONNECTED,     /*PTC29*/ PAL_MODE_UNCONNECTED,
        /*PTC30*/ PAL_MODE_UNCONNECTED,     /*PTC31*/ PAL_MODE_UNCONNECTED,
      },
    },
    {
      .port = IOPORT4,  // PORTD
      .pads = {
	/* PTD0*/ PAL_MODE_OUTPUT_PUSHPULL, /* PTD1*/ PAL_MODE_UNCONNECTED,     /* PTD2*/ PAL_MODE_UNCONNECTED,
#if ORCHARD_BOARD_REV == ORCHARD_REV_EVT1
        /* PTD3*/ PAL_MODE_UNCONNECTED,     /* PTD4*/ PAL_MODE_OUTPUT_PUSHPULL, /* PTD5*/ PAL_MODE_ALTERNATIVE_2,
#else
  /* ECO9: Swap B0/D4 DC/INT to allow GPIOX to fire interrupts */
        /* PTD3*/ PAL_MODE_UNCONNECTED,     /* PTD4*/ PAL_MODE_ALTERNATIVE_2,    /* PTD5*/ PAL_MODE_ALTERNATIVE_2,
#endif
        /* PTD6*/ PAL_MODE_INPUT_ANALOG,   /* PTD7*/ PAL_MODE_OUTPUT_PUSHPULL,   /* PTD8*/ PAL_MODE_UNCONNECTED,
        /* PTD9*/ PAL_MODE_UNCONNECTED,     /*PTD10*/ PAL_MODE_UNCONNECTED,     /*PTD11*/ PAL_MODE_UNCONNECTED,
        /*PTD12*/ PAL_MODE_UNCONNECTED,     /*PTD13*/ PAL_MODE_UNCONNECTED,     /*PTD14*/ PAL_MODE_UNCONNECTED,
        /*PTD15*/ PAL_MODE_UNCONNECTED,     /*PTD16*/ PAL_MODE_UNCONNECTED,     /*PTD17*/ PAL_MODE_UNCONNECTED,
        /*PTD18*/ PAL_MODE_UNCONNECTED,     /*PTD19*/ PAL_MODE_UNCONNECTED,     /*PTD20*/ PAL_MODE_UNCONNECTED,
        /*PTD21*/ PAL_MODE_UNCONNECTED,     /*PTD22*/ PAL_MODE_UNCONNECTED,     /*PTD23*/ PAL_MODE_UNCONNECTED,
        /*PTD24*/ PAL_MODE_UNCONNECTED,     /*PTD25*/ PAL_MODE_UNCONNECTED,     /*PTD26*/ PAL_MODE_UNCONNECTED,
        /*PTD27*/ PAL_MODE_UNCONNECTED,     /*PTD28*/ PAL_MODE_UNCONNECTED,     /*PTD29*/ PAL_MODE_UNCONNECTED,
        /*PTD30*/ PAL_MODE_UNCONNECTED,     /*PTD31*/ PAL_MODE_UNCONNECTED,
      },
    },
    {
      .port = IOPORT5,  // PORTE
      .pads = {
        /* PTE0*/ PAL_MODE_ALTERNATIVE_2,   /* PTE1*/ PAL_MODE_ALTERNATIVE_2,   /* PTE2*/ PAL_MODE_ALTERNATIVE_2,
        /* PTE3*/ PAL_MODE_ALTERNATIVE_2,   /* PTE4*/ PAL_MODE_UNCONNECTED,     /* PTE5*/ PAL_MODE_UNCONNECTED,
        /* PTE6*/ PAL_MODE_UNCONNECTED,     /* PTE7*/ PAL_MODE_UNCONNECTED,     /* PTE8*/ PAL_MODE_UNCONNECTED,
        /* PTE9*/ PAL_MODE_UNCONNECTED,     /*PTE10*/ PAL_MODE_UNCONNECTED,     /*PTE11*/ PAL_MODE_UNCONNECTED,
        /*PTE12*/ PAL_MODE_UNCONNECTED,     /*PTE13*/ PAL_MODE_UNCONNECTED,     /*PTE14*/ PAL_MODE_UNCONNECTED,
        /*PTE15*/ PAL_MODE_UNCONNECTED,     /*PTE16*/ PAL_MODE_OUTPUT_PUSHPULL,    /*PTE17*/ PAL_MODE_OUTPUT_PUSHPULL,
#if ORCHARD_BOARD_REV == ORCHARD_REV_EVT1
        /*PTE18*/ PAL_MODE_OUTPUT_PUSHPULL, /*PTE19*/ PAL_MODE_INPUT_ANALOG,    /*PTE20*/ PAL_MODE_UNCONNECTED,
#else
        /* ECO7: E19 RESET/DAC swap  */
        /*PTE18*/ PAL_MODE_OUTPUT_PUSHPULL, /*PTE19*/ PAL_MODE_OUTPUT_PUSHPULL, /*PTE20*/ PAL_MODE_UNCONNECTED,
#endif
        /*PTE21*/ PAL_MODE_UNCONNECTED,     /*PTE22*/ PAL_MODE_UNCONNECTED,     /*PTE23*/ PAL_MODE_UNCONNECTED,
        /*PTE24*/ PAL_MODE_UNCONNECTED,     /*PTE25*/ PAL_MODE_UNCONNECTED,     /*PTE26*/ PAL_MODE_UNCONNECTED,
        /*PTE27*/ PAL_MODE_UNCONNECTED,     /*PTE28*/ PAL_MODE_UNCONNECTED,     /*PTE29*/ PAL_MODE_UNCONNECTED,
#if ORCHARD_BOARD_REV == ORCHARD_REV_EVT1
        /*PTE30*/ PAL_MODE_OUTPUT_PUSHPULL, /*PTE31*/ PAL_MODE_UNCONNECTED,
#else
        /* ECO7: E30 DAC/RESET swap */
        /*PTE30*/ PAL_MODE_OUTPUT_PUSHPULL,    /*PTE31*/ PAL_MODE_UNCONNECTED,
#endif
      },
    },
  },
};
#endif

#if KINETIS_MCG_MODE == KINETIS_MCG_MODE_PEE
static void early_usleep(int usec) {
  int j, k;

  for (j = 0; j < usec; j++)
    for (k = 0; k < 30; k++)
        asm("nop");
}
#endif /* KINETIS_MCG_MODE_PEE */

/**
 * @brief   Early initialization code.
 * @details This initialization must be performed just after stack setup
 *          and before any other initialization.
 */
void __early_init(void) {
  PITDriver * pit;

  /*
   * Enable the second PIT so that we can use it to count how many
   * cycles it takes to boot. This tends to vary so we can use it
   * as a source of entropy. We do not enable interrupts for this.
   * Only the first PIT will be used with interrupts enabled as
   * a delay timer for the FatFs code.
   */

  SIM->SCGC6 |= SIM_SCGC6_PIT;
  pit = &PIT1;
  pit->pit_base = (uint8_t *)PIT_BASE;
  CSR_WRITE_4(pit, PIT_LDVAL1, 0xFFFFFFFF);
  CSR_WRITE_4(pit, PIT_TCTRL1, PIT_TCTRL_TEN);

#if KINETIS_MCG_MODE == KINETIS_MCG_MODE_PEE

  /*
   * The Freescale/NXP KW019032 board has a 32MHz crystal attached to
   * the radio, and the CLKOUT pin of the radion wired to the PTA18/EXTAL0
   * pin of the MCU. We have the option of either using the KW01's
   * internal 32.768KHz reference (FEI mode) or the 32MHz reference from
   * the radion (PEE mode). (At power on or reset, the CPU is running in
   * FEI mode and we can customize the configuration in the clock init code
   * in the hal_lld module.) However there's one catch: at power on/reset,
   * the radio applies a divisor of 32 to the CLKOUT signal, meaning we
   * actually get a 1MHz signal by default instead of 32MHz. To correct
   * this, we need to write a 0 to the DIOMAP2 register in the radio, but
   * we have to do it before doing the clock initialization in the hal_lld
   * module. This means we have to do a bit of low level setup of ithe SPI0
   * channel and a couple of the GPIO pins here. To make sure the radio is
   * in a known state each time we boot, we perform a hard reset by asserting
   * its reset pin, then quickly do a SPI transaction to update the
   * clock divisor.
   */

  /* Enable clock gating for SPI0 and GPIO MUX blocks */

  SIM->SCGC5 |= SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE;
  SIM->SCGC4 |= SIM_SCGC4_SPI0;

  /* Mux PTD0 as a GPIO, since it's used for Chip Select.*/

  palSetPadMode (RADIO_CHIP_SELECT_PORT, RADIO_CHIP_SELECT_PIN,
    PAL_MODE_OUTPUT_PUSHPULL);

  /* Mux PTC5 as SCK */

  palSetPadMode (GPIOC, 5, PAL_MODE_ALTERNATIVE_2);

  /* Mux PTC6 as MISO */

  palSetPadMode (GPIOC, 6, PAL_MODE_ALTERNATIVE_2);

  /* Mux PTC7 as MOSI */

  palSetPadMode (GPIOC, 7, PAL_MODE_ALTERNATIVE_2);

  /* Force radio SPI select high. */

  palSetPad (RADIO_CHIP_SELECT_PORT, RADIO_CHIP_SELECT_PIN);

  /* Reset the radio */

  palSetPadMode (GPIOE, 30, PAL_MODE_OUTPUT_PUSHPULL);

  palSetPad (RADIO_RESET_PORT, RADIO_RESET_PIN);
  early_usleep (100);
  palClearPad (RADIO_RESET_PORT, RADIO_RESET_PIN);
  early_usleep (500);

  /* Initialize SPI0 channel. Set for 16-bit writes. */

  SPI0->C1 = 0;
  SPI0->C2 = SPIx_C2_SPMODE;
  SPI0->BR = 0;
  SPI0->C1 |= SPIx_C1_SPE | SPIx_C1_MSTR;
  (void)SPI0->S;
 
  /* Now program the radio to generate a 32Mhz clock output */

  palClearPad (RADIO_CHIP_SELECT_PORT, RADIO_CHIP_SELECT_PIN);
  while ((SPI0->S & SPIx_S_SPTEF) == 0)
        ;
  SPI0->DH = KW01_DIOMAP2 | 0x80;
  SPI0->DL = KW01_CLKOUT_FXOSC;
  while ((SPI0->S & SPIx_S_SPRF) == 0)
        ;
  (void) SPI0->DL;
  palSetPad (RADIO_CHIP_SELECT_PORT, RADIO_CHIP_SELECT_PIN);

#endif /* KINETIS_MCG_MODE_PEE */

  kl1x_clock_init();
}

/**
 * @brief   Board-specific initialization code.
 * @todo    Add your board-specific code, if any.
 */
void boardInit(void) {
}
