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

/* define this to halt on SD card Failure */
#define HALT_ON_SDFAIL

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "spi.h"
#include "pit_lld.h"
#include "tpm_lld.h"
#include "dac_lld.h"
#include "dma_lld.h"
#include "sound.h"
#include "rand.h"

#include "shell.h"
#include "chprintf.h"

#include "orchard.h"
#include "orchard-shell.h"
#include "orchard-events.h"
#include "orchard-ui.h"
#include "orchard-app.h"

#include "oled.h"
#include "led.h"
#include "radio_lld.h"
#include "radio_reg.h"
#include "flash.h"

#if HAL_USE_MMC_SPI
#include "mmc_spi.h"
#else
#include "mmc.h"
#endif

#include "gfx.h"
#include "images.h"
#include "unlocks.h"
#include "userconfig.h"

#include "gitversion.h"   /* Autogenerated by make */
#include "buildtime.h"    /* Autogenerated by make */

#include "ides_gfx.h"     /* for putimage */

/*
 * 1 = 0
 * 2 = 1
 * 3 = 2
 * 4 = 4
 * 5 = 5
 * 6 = 6
 * 7 = 8
 * 8 = 9
 * 9 = 10
 * * = 12
 * 0 = 13
 * # = 14
 */

static uint8_t number_sequence[] = {
  /* 5*45*59*22#6666 */  
  5, 12, 4, 5, 12, 5, 10, 12, 1, 1, 14, 6, 6, 6, 6, 255
};

extern void tonePlay (GWidgetObject *, uint8_t, uint32_t);

#ifdef MAKE_DTMF
extern int make_dtmf(void);
#endif

struct evt_table orchard_events;

uint8_t pee_pbe(void);
uint8_t pbe_fbe(void);
int32_t fll_freq(int32_t fll_ref);
uint8_t fbe_fei(void);
void cpuStop(void);
void cpuVLLS0(void);

uint8_t led_fb[48];

static const SPIConfig spi1_config = {
  NULL,
  /* HW dependent part.*/
  RADIO_CHIP_SELECT_PORT,
  RADIO_CHIP_SELECT_PIN,
  0
};

/* jna: change the 0x00 here to 0x10 to support the older boards */
static const SPIConfig spi2_config = {
  NULL,
  /* HW dependent part.*/
  MMC_CHIP_SELECT_PORT,
  MMC_CHIP_SELECT_PIN,
  0x00
};

#if HAL_USE_MMC_SPI
MMCDriver MMCD1;

static const MMCConfig mmccfg = {
	&SPID2,
	&spi2_config,
	&spi2_config
};

bool mmc_lld_is_card_inserted(MMCDriver *mmcp)
{
	(void)mmcp;
	return (TRUE);
}

bool mmc_lld_is_write_protected(MMCDriver *mmcp)
{
	(void)mmcp;
	return (FALSE);
}
#endif

static void shell_termination_handler(eventid_t id) {
  static int i = 1;
  (void)id;

  chprintf(stream, "\r\nRespawning shell (shell #%d, event %d)\r\n", ++i, id);
  orchardShellRestart();
}

static void orchard_app_restart(eventid_t id) {
  (void)id;

  orchardAppRestart();
}

extern int print_hex(BaseSequentialStream *chp,
                     const void *block, int count, uint32_t start);

static void default_radio_handler(KW01_PKT * pkt)
{
  chprintf(stream, "\r\nNo handler for packet found.  %02x -> %02x : %02x (signal strength: -%ddBm)\r\n",
           pkt->kw01_hdr.kw01_src,
           pkt->kw01_hdr.kw01_dst,
           pkt->kw01_hdr.kw01_prot,
           (uint8_t)KW01_RSSI(pkt->kw01_rssi));

  print_hex(stream, pkt->kw01_payload, pkt->kw01_length, 0);
}

static void print_mcu_info(void) {
  uint32_t sdid = SIM->SDID;
  const char *famid[] = {
    "KL0%d (low-end)",
    "KL1%d (basic)",
    "KL2%d (USB)",
    "KL3%d (Segment LCD)",
    "KL4%d (USB and Segment LCD)",
  };
  const uint8_t ram[] = {
    0,
    1,
    2,
    4,
    8,
    16,
    32,
    64,
  };

  const uint8_t pins[] = {
    16,
    24,
    32,
    36,
    48,
    64,
    80,
  };

  if (((sdid >> 20) & 15) != 1) {
    chprintf(stream, "Device is not Kinetis KL-series\r\n");
    return;
  }

  chprintf(stream, famid[(sdid >> 28) & 15], (sdid >> 24) & 15);
  chprintf(stream, " with %d kB of ram detected"
                   " (Rev: %04x  Die: %04x  Pins: %d).\r\n",
                   ram[(sdid >> 16) & 15],
                   (sdid >> 12) & 15,
                   (sdid >> 7) & 31,
                   pins[(sdid >> 0) & 15]);
  chprintf(stream, "CPU clock: %dMHz Bus clock: %dMHz\r\n",
     (KINETIS_SYSCLK_FREQUENCY / 1000000),
     (KINETIS_BUSCLK_FREQUENCY / 1000000));
}

static void radio_ping_handler(KW01_PKT *pkt) {
  peer * u;
#ifndef LEADERBOARD_AGENT
  userconfig *c = getConfig();
#endif
  
#ifdef DEBUG_PINGS
  chprintf(stream, "\r\nGot a ping --  %02x -> %02x : %02x (signal strength: -%ddBm)\r\n",
	   pkt->kw01_hdr.kw01_src,
           pkt->kw01_hdr.kw01_dst,
           pkt->kw01_hdr.kw01_prot,
           (uint8_t)KW01_RSSI(pkt->kw01_rssi));
#endif
  
  u = (peer *)pkt->kw01_payload;

  if (c->unlocks & UL_PINGDUMP) {
    chprintf(stream, "PING: {\"name\":\"%s\"," \
             "\"badgeid\":\"%08x\"," \
             "\"ptype\":\"%d\"," \
             "\"ctype\":\"%d\"," \
             "\"hp\":%d," \
             "\"xp\":%d," \
             "\"level\":%d,",
             u->name,
             u->netid,
             u->current_type,
             u->p_type,
             u->hp,
             u->xp,
             u->level);

    chprintf(stream,
             "\"won\":%d,"  \
             "\"lost\":%d," \
             "\"agl\":%d," \
             "\"might\":%d," \
             "\"luck\":%d" \
             "}\r\n",
             u->won,
             u->lost,
             u->agl,
             u->might,
             u->luck
             );
  }

  enemyAdd(u);

  /* set the clock if any */
  if ((u->rtc != 0) && (rtc == 0)) {
    rtc = u->rtc;
    rtc_set_at = chVTGetSystemTime(); // note this is in ticks
  }
  
  orchardAppRadioCallback (pkt);
}

/*
 * Application entry point.
 */
int main(void)
{
  userconfig * config;
  config = NULL;
  int i;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * The Freescale/NXP KW019032 board has two LEDs connected to the CPU:
   * - One single-color blue LED on port E pin 17
   * - One tri-color LED with the following pin connections:
   *   red - port E pin 16
   *   green - port B pin 1
   *   blue - port B pin 0
   *
   * We turn on the blue LED and leave the tri-color one off for now.
   */

  /* Single color LED */

  palSetPad (BLUE_SOLO_LED_PORT, BLUE_SOLO_LED_PIN); /* Blue LED */
 
  /* Multi-color LED */
 
  palSetPad (RED_LED_PORT, RED_LED_PIN);  /* Red */
  palSetPad (GREEN_LED_PORT, GREEN_LED_PIN);   /* Green */
  palSetPad (BLUE_LED_PORT, BLUE_LED_PIN);   /* Blue */

  /* Turn on the blue LED */
  palClearPad (BLUE_SOLO_LED_PORT, BLUE_SOLO_LED_PIN);
  evtTableInit(orchard_events, 5);

  /* init the shell and show our banners */
  orchardShellInit();
  chprintf(stream, SHELL_BANNER);
  chprintf(stream, "\r\n     ()==[:::::::::::::> gitrev %s", gitversion);
  chprintf(stream, "\r\n                         %s\r\n\r\n", BUILDTIME);
  
  flashStart();
  print_mcu_info();

  /* start the engines */
#if HAL_USE_MMC_SPI
  pit0Start (&PIT1, NULL);
#else
  pit0Start (&PIT1, disk_timerproc);
#endif
  dacStart (&DAC1);
  dmaStart ();
  pwmStart ();
  spiStart(&SPID1, &spi1_config);
  orchardEventsStart();
  radioStart(&SPID1);

  evtTableHook(orchard_events, shell_terminated, shell_termination_handler);

  radioDefaultHandlerSet (radioDriver, default_radio_handler);

  // eventually get rid of this
  chprintf(stream, "User flash start: 0x%x  user flash end: 0x%x  length: 0x%x\r\n",
	   __storage_start__, __storage_end__, __storage_size__);

  /*
   * Force all the chip select pins for the SD card, display and
   * touch controller high so that they all start out in the
   * deselected state. We don't know what state the pins will be
   * in when we get here, and if the chip select for the screen
   * or touch controller happens to be low, it will interfere
   * with the probe for the SD card.
   */

  palSetPad (MMC_CHIP_SELECT_PORT, MMC_CHIP_SELECT_PIN);
  palSetPad (SCREEN_CHIP_SELECT_PORT, SCREEN_CHIP_SELECT_PIN);
  palSetPad (XPT_CHIP_SELECT_PORT, XPT_CHIP_SELECT_PIN);
  palSetPad (SCREEN_CMDDATA_PORT, SCREEN_CMDDATA_PIN);

  /*
   * The following code works around what seems to be a problem with
   * the Texas Instruments level converters on the Freescale/NXP KW019032
   * board. Pin PTE0/SPI1_MISO seems to end up pulled to ground from the
   * output side, and since the level converter is bi-directional, this
   * seems to cause the it to latch the input side low too. For the MISO
   * pin, this is bad because it makes it look like every device is
   * driving 0 bits onto the SPI bus.
   *
   * To get around this, we set PTE0 as an output, set it high and then
   * enable the internal pullup resistor. This seems to unlatch the
   * level converter and allow the pin to work properly again. We set
   * it back to SPI1_MISO mode after we're done. For good measure, we
   * do the same thing to the PTE1/SPI1_MOSI pin.
   *
   * This may not be necessary with the final design since will likely
   * not include level converters, but it should be harmless to leave
   * it in.
   */

  /* Set to output */
  palSetPadMode (SPI1_MISO_PORT, SPI1_MISO_PIN, PAL_MODE_OUTPUT_PUSHPULL);
  /* Pull high */
  palSetPad (SPI1_MISO_PORT, SPI1_MISO_PIN);
  /* Enable pullup */
  palSetPadMode (SPI1_MISO_PORT, SPI1_MISO_PIN, PAL_MODE_INPUT_PULLUP);
  /* Restore SPI mode */
  palSetPadMode (SPI1_MISO_PORT, SPI1_MISO_PIN, PAL_MODE_ALTERNATIVE_2);

  palSetPadMode (SPI1_MOSI_PORT, SPI1_MOSI_PIN, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad (SPI1_MOSI_PORT, SPI1_MOSI_PIN);
  palSetPadMode (SPI1_MOSI_PORT, SPI1_MOSI_PIN, PAL_MODE_INPUT_PULLUP);
  palSetPadMode (SPI1_MOSI_PORT, SPI1_MOSI_PIN, PAL_MODE_ALTERNATIVE_2);

#if HAL_USE_MMC_SPI
  /* Connect the SD card */

  mmcObjectInit (&MMCD1);
  mmcStart (&MMCD1, &mmccfg);

  if (mmcConnect (&MMCD1) == HAL_SUCCESS)
    chprintf (stream, "SD card detected: %dMB\r\n",
        mmcsdGetCardCapacity (&MMCD1) >> 11);
  else 
    chprintf (stream, "No SD card found.\r\n");
#endif

  /*
   * Be sure to start the second SPI channel again here. If mmcConnect()
   * returns error because there's no SD card plugged in, it will call
   * spiStop() which turns off the clock gating and interrupts for the
   * SPi1 module. We have to re-enable them otherwise we'll get an
   * exception when trying to access the SPI1 registers.
   */

  spiStart(&SPID2, &spi2_config);


#if !HAL_USE_MMC_SPI
  /*
   * Connect the SD card.
   * Note: we have to access the SD card first, before the display or
   * touch controller. All three devices share the same SPI channel, but
   * SD cards support multiple modes and we have to issue a special
   * command in order to put it in SPI mode before we begin doing other
   * SPI transactions with the other slaves. If we don't, it's possible
   * that the bit patterns generated by other SPI transactions might
   * be interpreted by the SD card as some sort of other command and
   * put it into an unknown state in which it won't respond to us anymore.
   */

  if (mmc_disk_initialize () == 0) {
    chprintf (stream, "SD card detected.\r\n");
  } else {
    /* Since we didn't init ugfx before because of the aforementioned
       issues with the SD card, we have to init now, even though we're
       going to die with an error. */
    gfxInit();
    chprintf (stream, "No SD card found.\r\n");
    oledSDFail();
    configStart();
    config = getConfig ();

    /* override any sound choice the user has made */
    config->sound_enabled = 1;

    /* put the LEDs in test mode because we failed */
    ledStart (LEDS_COUNT, led_fb);
    effectsStart();
    ledSetFunction(leds_test);
    
    playHardFail();
    /* nuke the main thread but keep our shell up for debugging */
#ifdef HALT_ON_SDFAIL
    orchardShellRestart();
    chThdSleep (TIME_INFINITE);
#endif
  }
  
#endif

  /*
   * Note: we have to wait until here to initialize the random seed
   * because we need to poll the touch controller and that requires
   * SPI channel 1 to be enabled. The userconfig module requires
   * rand(), so we have wait until after setting the seed before
   * calling configStart().
   */

  randInit ();

  /* Things are good, load config and start LEDs. 
   * 
   * We check to see if config is null to avoid an edge case here if
   * HALT_ON_SDFAIL is undefined and the SD card fails to start.  
   *
   */
  if (config == NULL) { 
    configStart ();
    config = getConfig ();
    ledStart (LEDS_COUNT, led_fb);
    if (config->led_pattern > 0) { 
      effectsStart();
    }
  }
  
  chprintf(stream, "HW UDID: 0x");
  chprintf(stream, "%08x", SIM->UIDMH);
  chprintf(stream, "%08x", SIM->UIDML);
  chprintf(stream, "%08x / netid: %08x\r\n",SIM->UIDL, config->netid);

  gfileMount ('F', "0:");

  orchardShellRestart();

  /* Initialize uGfx */
  gfxInit();
  uiStart();

  /* Draw a banner... */
  oledOrchardBanner();

#ifndef FAST_STARTUP
  if (config->sound_enabled == 1) {
    playStartupSong();
#ifdef MAKE_DTMF
    make_dtmf();
    dacPlay("dtmf-0.raw");
    dacWait();
    dacPlay("dtmf-1.raw");
    dacWait();
    dacPlay("dtmf-2.raw");
    dacWait();
    dacPlay("dtmf-3.raw");
    dacWait();
    dacPlay("dtmf-4.raw");
    dacWait();
#endif    
    chThdSleepMilliseconds(IMG_SPLASH_DISPLAY_TIME);
  } else {
    chThdSleepMilliseconds(IMG_SPLASH_NO_SOUND_DISPLAY_TIME);
  }

  /* Promote sponsors */
  putImageFile("dc25lgo.rgb", 0, 0);
  chThdSleepMilliseconds (2000);

  ledSetFunction(leds_shownetid);
  putImageFile("sponsor.rgb", 0, 0);
  dacWait ();
  dacStop ();
  i = 0;

  while (number_sequence[i] != 255) {
      tonePlay (NULL, number_sequence[i], 85);
      i++;
  }
  chThdSleepMilliseconds (2000);
#endif
  ledSetFunction(NULL);
  /* run apps */
  orchardAppInit();
  evtTableHook(orchard_events, orchard_app_terminated, orchard_app_restart);
  orchardAppRestart();

  /* set our inbound ping handler */
  radioHandlerSet(radioDriver, RADIO_PROTOCOL_PING, radio_ping_handler);
 
  chThdSetPriority (NORMALPRIO + 1);
 
  while (TRUE)
    chEvtDispatch(evtHandlers(orchard_events), chEvtWaitOne(ALL_EVENTS));
}

void halt(void) {
  // subsystems to bring down, in this order:
  // touch
  // LEDs
  // accelerometer
  // bluetooth, reset, setup, sleep
  // OLED
  // charger
  // GPIOX
  // Radio
  // CPU

  //  oledStop(&SPID2);
  chprintf(stream, "OLED stopped\n\r");
  chprintf(stream, "Slowing clock, sleeping radio & halting CPU...\n\r");
  pee_pbe();  // transition through three states to get to fei mode
  pbe_fbe();
  fbe_fei();
  
  radioStop(radioDriver);
  cpuVLLS0();
}

// look in ../os/ext/CMSIS/KINETIS/kl17z.h for CPU register set
void cpuStop(void) {
  volatile unsigned int dummyread;
  /* The PMPROT register may have already been written by init
     code. If so, then this next write is not done since PMPROT is
     write once after RESET this write-once bit allows the MCU to
     enter the normal STOP mode.  If AVLP is already a 1, VLPS mode is
     entered instead of normal STOP is SMC_PMPROT = 0 */

  /* Set the STOPM field to 0b000 for normal STOP mode
     For Kinetis L: if trying to enter Stop from VLPR user
     forced to VLPS low power mode */
  SMC->PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
  SMC->PMCTRL |= SMC_PMCTRL_STOPM(0);
  
  /*wait for write to complete to SMC before stopping core */
  dummyread =  SMC->PMCTRL;

  //#warning "check that this disassembles to a read"
  //  (void) SMC->PMCTRL; // check that this works
  dummyread++; // get rid of compiler warning

  /* Set the SLEEPDEEP bit to enable deep sleep mode (STOP) */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  /* WFI instruction will start entry into STOP mode */
  asm("WFI");
}

void cpuVLLS0(void) {
  volatile unsigned int dummyread;
  
  /* Write to PMPROT to allow all possible power modes */
  SMC->PMPROT = 0x2A; // enable all  possible modes
  /* Set the STOPM field to 0b100 for VLLS0 mode */
  SMC->PMCTRL &= 0xF8;
  SMC->PMCTRL |= 0x04;
  /* set VLLSM = 0b00 */
  SMC->STOPCTRL = 0x03;  // POR detect allowed, normal stop mode
  /*wait for write to complete to SMC before stopping core */
  dummyread = SMC->STOPCTRL;
  dummyread++; // get rid of compiler warning
  
  /* Set the SLEEPDEEP bit to enable deep sleep mode (STOP) */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  dummyread = SCB->SCR;
  /* WFI instruction will start entry into STOP mode */
  asm("WFI");
}

uint8_t pee_pbe(void) {
  int16_t i;
  
  // Check MCG is in PEE mode
  if (!((((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x3) && // check CLKS mux has selcted PLL output
	(!(MCG->S & MCG_S_IREFST)) &&                               // check FLL ref is external ref clk
	(MCG->S & MCG_S_PLLST)))                                    // check PLLS mux has selected PLL 
    {
      return 0x8;                                                       // return error code
    } 
  
  // As we are running from the PLL by default the PLL and external
  // clock settings are valid To move to PBE from PEE simply requires
  // the switching of the CLKS mux to select the ext clock As CLKS is
  // already 0 the CLKS value can simply be OR'ed into the register
  MCG->C1 |= MCG_C1_CLKS(2); // switch CLKS mux to select external reference clock as MCG_OUT
  
  // Wait for clock status bits to update 
  for (i = 0 ; i < 2000 ; i++) {
    if (((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x2) break; // jump out early if CLKST shows EXT CLK slected before loop finishes
  }
  if (((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x2) return 0x1A; // check EXT CLK is really selected and return with error if not

  // Now in PBE mode
  return 0;
} // pee_pbe


uint8_t pbe_fbe(void) {
  uint16_t i;
  
  // Check MCG is in PBE mode
  if (!((((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x2) && // check CLKS mux has selcted external reference
      (!(MCG->S & MCG_S_IREFST)) &&                               // check FLL ref is external ref clk
      (MCG->S & MCG_S_PLLST) &&                                   // check PLLS mux has selected PLL
      (!(MCG->C2 & MCG_C2_LP))))                                  // check MCG_C2[LP] bit is not set   
    {
      return 0x7;                                                       // return error code
    }

  // As we are running from the ext clock, by default the external clock settings are valid
  // To move to FBE from PBE simply requires the switching of the PLLS mux to disable the PLL 
  
  MCG->C6 &= ~MCG_C6_PLLS; // clear PLLS to disable PLL, still clocked from ext ref clk
  
  // wait for PLLST status bit to set
  for (i = 0 ; i < 2000 ; i++)  {
    if (!(MCG->S & MCG_S_PLLST)) break; // jump out early if PLLST clears before loop finishes
  }
  if (MCG->S & MCG_S_PLLST) return 0x15; // check bit is really clear and return with error if not clear  
  
  // Now in FBE mode  
  return 0;
 } // pbe_fbe
  
int32_t fll_freq(int32_t fll_ref) {
  int32_t fll_freq_hz;
  
  // Check that only allowed ranges have been selected
  if (((MCG->C4 & MCG_C4_DRST_DRS_MASK) >> MCG_C4_DRST_DRS_SHIFT) > 0x1)  {
    return 0x3B; // return error code if DRS range 2 or 3 selected
  }
  
  if (MCG->C4 & MCG_C4_DMX32) // if DMX32 set
    {
      switch ((MCG->C4 & MCG_C4_DRST_DRS_MASK) >> MCG_C4_DRST_DRS_SHIFT) // determine multiplier based on DRS
	{
	case 0:
	  fll_freq_hz = (fll_ref * 732);
	  if (fll_freq_hz < 20000000) {return 0x33;}
	  else if (fll_freq_hz > 25000000) {return 0x34;}
	  break;
	case 1:
	  fll_freq_hz = (fll_ref * 1464);
	  if (fll_freq_hz < 40000000) {return 0x35;}
	  else if (fll_freq_hz > 50000000) {return 0x36;}
	  break;
	case 2:
	  fll_freq_hz = (fll_ref * 2197);
	  if (fll_freq_hz < 60000000) {return 0x37;}
	  else if (fll_freq_hz > 75000000) {return 0x38;}
	  break;
	case 3:
	  fll_freq_hz = (fll_ref * 2929);
	  if (fll_freq_hz < 80000000) {return 0x39;}
	  else if (fll_freq_hz > 100000000) {return 0x3A;}
	  break;
	}
    }
  else // if DMX32 = 0
    {
      switch ((MCG->C4 & MCG_C4_DRST_DRS_MASK) >> MCG_C4_DRST_DRS_SHIFT) // determine multiplier based on DRS
	{
	case 0:
	  fll_freq_hz = (fll_ref * 640);
	  if (fll_freq_hz < 20000000) {return 0x33;}
	  else if (fll_freq_hz > 25000000) {return 0x34;}
	  break;
	case 1:
	  fll_freq_hz = (fll_ref * 1280);
	  if (fll_freq_hz < 40000000) {return 0x35;}
	  else if (fll_freq_hz > 50000000) {return 0x36;}
	  break;
	case 2:
	  fll_freq_hz = (fll_ref * 1920);
	  if (fll_freq_hz < 60000000) {return 0x37;}
	  else if (fll_freq_hz > 75000000) {return 0x38;}
	  break;
	case 3:
	    fll_freq_hz = (fll_ref * 2560);
	  if (fll_freq_hz < 80000000) {return 0x39;}
	  else if (fll_freq_hz > 100000000) {return 0x3A;}
	  break;
	}
    }    
  return fll_freq_hz;
} // fll_freq

uint8_t fbe_fei(void) {
  unsigned char temp_reg;
  short i;
  int mcg_out;
  int slow_irc_freq = 32768;
  
  // Check MCG is in FBE mode
  if (!((((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x2) && // check CLKS mux has selcted external reference
      (!(MCG->S & MCG_S_IREFST)) &&                               // check FLL ref is external ref clk
      (!(MCG->S & MCG_S_PLLST)) &&                                // check PLLS mux has selected FLL
      (!(MCG->C2 & MCG_C2_LP))))                                  // check MCG_C2[LP] bit is not set   
    {
      return 0x4;                                                       // return error code
    }

  // Check IRC frequency is within spec.
  if ((slow_irc_freq < 31250) || (slow_irc_freq > 39063))
    {
      return 0x31;
    }
  
  // Check resulting FLL frequency 
  mcg_out = fll_freq(slow_irc_freq); 
  if (mcg_out < 0x3C) {return mcg_out;} // If error code returned, return the code to calling function

  // Need to make sure the clockmonitor is disabled before moving to an "internal" clock mode
  MCG->C6 &= ~MCG_C6_CME0; //This assumes OSC0 is used as the external clock source
  
  // Move to FEI by setting CLKS to 0 and enabling the slow IRC as the FLL reference clock
  temp_reg = MCG->C1;
  temp_reg &= ~MCG_C1_CLKS_MASK; // clear CLKS to select FLL output
  temp_reg |= MCG_C1_IREFS; // select internal reference clock
  MCG->C1 = temp_reg; // update MCG_C1 
  
  // wait for Reference clock Status bit to set
  for (i = 0 ; i < 2000 ; i++)  {
    if (MCG->S & MCG_S_IREFST) break; // jump out early if IREFST sets before loop finishes
  }
  if (!(MCG->S & MCG_S_IREFST)) return 0x12; // check bit is really set and return with error if not set
  
  // Wait for clock status bits to show clock source is ext ref clk
  for (i = 0 ; i < 2000 ; i++)  {
    if (((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x0) break; // jump out early if CLKST shows EXT CLK slected before loop finishes
  }
  if (((MCG->S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x0) return 0x18; // check EXT CLK is really selected and return with error if not

  // Now in FEI mode
  return mcg_out;
} // fbe_fei

  
