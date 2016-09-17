
/*
 * Freescale/NXP KW01 radio register definitions.
 * The radio registers are accessed via the SPI0 port
 */

/* Most boards use a 32MHz crystal as a timebase */

#define KW01_XTAL_FREQ		32000000
#define KW01_FSTEP		((float)KW01_XTAL_FREQ / (float)524288.0)

#define KW01_FIFO		0x00	/* FIFO read/write access */
#define KW01_OPMODE		0x01	/* Transceiver operating mode */
#define KW01_DATMOD		0x02	/* Data and modulation modes */
#define KW01_BITRATEMSB		0x03	/* Bitrate MSB */
#define KW01_BITRATELSB		0x04	/* Bitrate LSB */
#define KW01_FDEVMSB		0x05	/* Deviation MSB */
#define KW01_FDEVLSB		0x06	/* Deviation LSB */
#define KW01_FRFMSB		0x07	/* Carrier frequency MSB */
#define KW01_FRFISB		0x08	/* Carrier frequency ISB */
#define KW01_FRFLSB		0x09	/* Carrier frequency LSB */
#define KW01_OSC		0x0A	/* RC oscillator setting */
#define KW01_AFCCTRL		0x0B	/* AFC control, low modulation */
#define KW01_LOWBAT		0x0C	/* Low battery indicator settings */
#define KW01_LISTEN1		0x0D	/* Listen mode settings */
#define KW01_LISTEN2		0x0E	/* Listen mode idle duration */
#define KW01_LISTEN3		0x0F	/* Listen mode RX duration */
#define KW01_VERSION		0x10	/* Radio silicon version */
#define KW01_PALEVEL		0x11	/* PA selection + power control */
#define KW01_PARAMP		0x12	/* PA ramp time */
#define KW01_OCP		0x13	/* Over current protection */
#define KW01_RSVD0		0x14
#define KW01_RSVD1		0x15
#define KW01_RSVD2		0x16
#define KW01_RSVD3		0x17
#define KW01_LNA		0x18	/* LNA settings */
#define KW01_RXBW		0x19	/* Channel filter BW control */
#define KW01_RXBWAFC		0x1A	/* Chn. filt. BW control during AFC */
#define KW01_OOK_PEAK		0x1B	/* OOK demod/select in peak mode */
#define KW01_OOK_AVG		0x1C	/* OOK average threshold control */
#define KW01_OOK_FIX		0x1D	/* OOK fixed threshold control */
#define KW01_AFC_FEI		0x1E	/* AFC and FEI control and status */
#define KW01_AFC_MSB		0x1F	/* AFC frequency correction MSB */
#define KW01_AFC_LSB		0x20	/* AFC frequency correction LSB */
#define KW01_FEI_MSB		0x21	/* Calculated frequency error MSB */
#define KW01_FEI_LSB		0x22	/* Calculated frequency error LSB */
#define KW01_RSSICONF		0x23	/* RSSI settings */
#define KW01_RSSIVAL		0x24	/* RSSI value in dBm */
#define KW01_DIOMAP1		0x25	/* Digital I/O pin mapping 1 */
#define KW01_DIOMAP2		0x26	/* Digital I/O pin mapping 2 */
#define KW01_IRQ1		0x27	/* IRQ status 1 */
#define KW01_IRQ2		0x28	/* IRQ status 2 */
#define KW01_RSSITHRESH		0x29	/* RSSI threshold control */
#define KW01_RXTMO1		0x2A	/* RX timeout btwn. RX and RSSI det. */
#define KW01_RXTMO2		0x2B	/* RX timeout btwn. RSSI det & plrdy */
#define KW01_PREAMBLEMSB	0x2C	/* Preamble length MSB */
#define KW01_PREAMBLELSB	0x2D	/* Preamble length LSB */
#define KW01_SYNCCONF		0x2E	/* Sync word recognition control */
#define KW01_SYNCVAL0		0x2F	/* Sync val byte 0 */
#define KW01_SYNCVAL1		0x30	/* Sync val byte 1 */
#define KW01_SYNCVAL2		0x31	/* Sync val byte 2 */
#define KW01_SYNCVAL3		0x32	/* Sync val byte 3 */
#define KW01_SYNCVAL4		0x33	/* Sync val byte 4 */
#define KW01_SYNCVAL5		0x34	/* Sync val byte 5 */
#define KW01_SYNCVAL6		0x35	/* Sync val byte 6 */
#define KW01_SYNCVAL7		0x36	/* Sync val byte 7 */
#define KW01_PKTCONF1		0x37	/* Packet mode settings 1 */
#define KW01_PAYLEN		0x38	/* Payload length */
#define KW01_NODEADDR		0x39	/* Node address */
#define KW01_BCASTADDR		0x3A	/* Broadcast address */
#define KW01_AUTOMODES		0x3B	/* Auto modes settings */
#define KW01_FIFOTHRESH		0x3C	/* FIFO threshold, TX start cond. */
#define KW01_PKTCONF2		0x3D	/* Packet mode settings 2 */
#define KW01_AESKEY0		0x3E	/* AES KEY 0 */
#define KW01_AESKEY1		0x3F	/* AES KEY 1 */
#define KW01_AESKEY2		0x40	/* AES KEY 2 */
#define KW01_AESKEY3		0x41	/* AES KEY 3 */
#define KW01_AESKEY4		0x42	/* AES KEY 4 */
#define KW01_AESKEY5		0x43	/* AES KEY 5 */
#define KW01_AESKEY6		0x44	/* AES KEY 6 */
#define KW01_AESKEY7		0x45	/* AES KEY 7 */
#define KW01_AESKEY8		0x46	/* AES KEY 8 */
#define KW01_AESKEY9		0x47	/* AES KEY 9 */
#define KW01_AESKEY10		0x48	/* AES KEY 10 */
#define KW01_AESKEY11		0x49	/* AES KEY 11 */
#define KW01_AESKEY12		0x4A	/* AES KEY 12 */
#define KW01_AESKEY13		0x4B	/* AES KEY 13 */
#define KW01_AESKEY14		0x4C	/* AES KEY 14 */
#define KW01_AESKEY15		0x4D	/* AES KEY 15 */
#define KW01_TEMP1		0x4E	/* Temp sensor control */
#define KW01_TEMP2		0x4F	/* Temp readout */

/* Test registers */

#define KW01_TEST_INT0		0x50	/* Internal test */
#define KW01_TEST_INT1		0x51	/* Internal test */
#define KW01_TEST_INT2		0x52	/* Internal test */
#define KW01_TEST_INT3		0x53	/* Internal test */
#define KW01_TEST_INT4		0x54	/* Internal test */
#define KW01_TEST_INT5		0x55	/* Internal test */
#define KW01_TEST_INT6		0x56	/* Internal test */
#define KW01_TEST_INT7		0x57	/* Internal test */
#define KW01_TESTLNA		0x58	/* Sensitivity boost */
#define KW01_TESTTXCO		0x59	/* XTAL or TCXO input select */
#define KW01_PSDTST3		0x5F	/* Test, PLL bandwidth */
#define KW01_TESTDAGC		0x6F	/* Fading margin improvement */
#define KW01_TESTAFC		0x71	/* AFC offset for low modulation idx */


/* Opmode register */

#define KW01_OPMODE_SEQOFF	0x80	/* Automatic sequencer disable */
#define KW01_OPMODE_LISTENON	0x40	/* Enable listen mode 0 off/1 on */
#define KW01_OPMODE_LISTENABT	0x20	/* Abort listen mode */
#define KW01_OPMODE_MODE	0x1C	/* Transceiver mode */

#define KW01_MODE_SLEEP		0x00	/* Sleep mode */
#define KW01_MODE_STANDBY	0x04	/* Standby mode */
#define KW01_MODE_FS		0x08	/* Frequency synthesis mode (FS) */
#define KW01_MODE_TX		0x0C	/* Transmit mode */
#define KW01_MODE_RX		0x10	/* Receive mode */

/* Data/modulation control register */

#define KW01_DATMOD_DATAMODE	0x60	/* Data mode */
#define KW01_DATMOD_MODULATION	0x18	/* Modulation type */
#define KW01_DATMOD_MODSHAPE	0x03	/* Modulation shaping */

#define KW01_DATAMODE_PACKET	0x00	/* Packet mode */
#define KW01_DATAMODE_CONTSYNC	0x40	/* Continuous mode + synchronizer */
#define KW01_DATAMODE_CONT	0x60	/* Continuous mode, no synchronizer */

#define KW01_MODULATION_FSK	0x00	/* FSK modulation */
#define KW01_MODULATION_OOK	0x08	/* OOK modulation */

#define KW01_MODSHAPE_NONE	0x00	/* No shaping */
#define KW01_MODSHAPE_GFBT10	0x01	/* Guassian filter, bt == 1.0 */
#define KW01_MODSHAPE_GFBT05	0x02	/* Guassian filter, bt == 0.5 */
#define KW01_MODSHAPE_GFBT03	0x03	/* Guassian filter, bt == 0.3 */

/* Bitrate registers */

#define KW01_BITRATE(x)		(KW01_XTAL_FREQ / (x))	/* 4800 - 200000 */

/* Deviation registers */

#define KW01_FDEV(x)		((x) / KW01_FSTEP)	/* 2400 -- 200000 */

/* Carrier frequency registers */

#define KW01_FREQ(x)		((x) / KW01_FSTEP)

/* RC oscillator control register */

#define KW01_OSC_STARTCAL	0x80	/* Start oscillator calibration */
#define KW01_OSC_CALDONE	0x40	/* Calibration complete */

/* AFC control register */

#define KW01_AFCCTRL_LOWBETA	0x20	/* 0 standard AFC / 1 improved AFC */

/* Battery monitor register */

#define KW01_BATMON_MONITOR	0x10	/* Real-time low battery indicator */
#define KW01_BATMON_ENABLE	0x08	/* Enable battery monitor */
#define KW01_BATMON_BATTRIM	0x07	/* Trim low battery threshold */

#define KW01_BATTRIM_1_695	0x00	/* 1.695V */
#define KW01_BATTRIM_1_764	0x01	/* 1.764V */
#define KW01_BATTRIM_1_835	0x02	/* 1.835V */
#define KW01_BATTRIM_1_905	0x03	/* 1.905V */
#define KW01_BATTRIM_1_976	0x04	/* 1.976V */
#define KW01_BATTRIM_2_045	0x05	/* 2.045V */
#define KW01_BATTRIM_2_116	0x06	/* 2.116V */
#define KW01_BATTRIM_2_185	0x07	/* 2.185V */

/* Listen1 register */

#define KW01_LISTEN1_IDLERES	0xC0	/* Idle time resolution */
#define KW01_LISTEN1_RXRES	0x30	/* RX time resolution */
#define KW01_LISTEN1_CRITERIA	0x08	/* Acceptance criteria */
#define KW01_LISTEN1_ACTION	0x06	/* Acceptance action */

#define KW01_IDLERES_64US	0x40	/* 64us */
#define KW01_IDLERES_4_1MS	0x80	/* 4.1ms */
#define KW01_IDLERES_262MS	0xC0	/* 262ms */

#define KW01_RXRES_64US		0x10	/* 64us */
#define KW01_RXRES_4_1MS	0x20	/* 4.1ms */
#define KW01_RXRES_262MS	0x30	/* 262ms */

#define KW01_CRITERIA_RSSI	0x00	/* Signal above RSSI threshold */
#define KW01_CRITERIA_RSSISYNC	0x08	/* Signal RSSI + syncaddr match */

#define KW01_ACTION_RXDIS	0x00	/* Stay in RX, must disable listen */
#define KW01_ACTION_PYLTMO	0x02	/* Wait for PayloadReady or Timeout */
#define KW01_ACTION_IDLE	0x04	/* Resume in idle state */

/* Version register */

#define KW01_VERSION_CHIP	0xF8	/* Chip revision number */
#define KW01_VERSION_MASK	0x07	/* Metal mask revision */

#define KW01_CHIPREV(x)		((x) >> 3)
#define KW01_MASKREV(x)		(x & KW01_VERSION_MASK)

/* PA level register */

#define KW01_PALEVEL_PA0	0x80	/* PA0 on RFIO and LNA */
#define KW01_PALEVEL_PA1	0x40	/* PA1 on PA_BOOST pin */
#define KW01_PALEVEL_PA2	0x20	/* PA1 on PA_BOOST pin */

/* PA ramp up register */

#define KW01_PARAMP_RISEFALL	0x0F	/* Rise/fall time in FSK mode */

#define KW01_RISEFALL_3_4MS	0x00	/* 3.4ms */
#define KW01_RISEFALL_2MS	0x01	/* 2ms */
#define KW01_RISEFALL_1MS	0x02	/* 1ms */
#define KW01_RISEFALL_500US	0x03	/* 500us */
#define KW01_RISEFALL_250US	0x04	/* 250us */
#define KW01_RISEFALL_125US	0x05	/* 100us */
#define KW01_RISEFALL_100US	0x06	/* 125us */
#define KW01_RISEFALL_62US	0x07	/* 62us */
#define KW01_RISEFALL_50US	0x08	/* 50us */
#define KW01_RISEFALL_40US	0x09	/* 40us */
#define KW01_RISEFALL_31US	0x0A	/* 31us */
#define KW01_RISEFALL_25US	0x0B	/* 25us */
#define KW01_RISEFALL_20US	0x0C	/* 20us */
#define KW01_RISEFALL_15US	0x0D	/* 15us */
#define KW01_RISEFALL_12US	0x0E	/* 12us */
#define KW01_RISEFALL_10US	0x0F	/* 10us */

/* Over-current protection register */

#define KW01_OCP_OK		0x10	/* OCP enabled */
#define KW01_OCP_TRIM		0x0F	/* OCP trim value, in mA */

/* Note: min is 45mA, default is 95mA */

#define KW01_OCPTRIM(x)		(((x) - 45) / 5)

/* LNA register */

#define KW01_LNA_ZIN		0x80	/* Input impedance */
#define KW01_LNA_CURRGAIN	0x38	/* Current gain value */
#define KW01_LNA_GAIN		0x07	/* Manual gain setting */

#define KW01_ZIN_50OHM		0x00
#define KW01_ZIN_200OHM		0x80

#define KW01_CURRGAIN(x)	(((x) & KW01_LNA_CURRGAIN) >> 3)

#define KW01_GAIN_AGC		0x00	/* Gain set by AGC */
#define KW01_GAIN_G1		0x01	/* Max gain */
#define KW01_GAIN_G2		0x02	/* Max gain - 6dB */
#define KW01_GAIN_G3		0x03	/* Max gain - 12Db */
#define KW01_GAIN_G4		0x04	/* Max gain - 24dB */
#define KW01_GAIN_G5		0x05	/* Max gain - 36dB */
#define KW01_GAIN_G6		0x06	/* Max gain - 48dB */

/* Channel filter bandwidth control register */

#define KW01_RXBW_DCCFREQ	0xE0	/* DC cutoff frequency */
#define KW01_RXBW_MANT		0x18	/* BW control mantissa */
#define KW01_RXBW_EXP		0x07	/* BW control exponent */

#define KW01_DCCFREQ_16		0x00	/* 16% */
#define KW01_DCCFREQ_8		0x20	/* 8% */
#define KW01_DCCFREQ_4		0x40	/* 4% */
#define KW01_DCCFREQ_2		0x60	/* 2% */
#define KW01_DCCFREQ_1		0x80	/* 1% */
#define KW01_DCCFREQ_0_5	0xA0	/* 0.5% */
#define KW01_DCCFREQ_0_25	0xC0	/* 0.25% */
#define KW01_DCCFREQ_0_125	0xE0	/* 0.125% */

#define KW01_MANT_16		0x00
#define KW01_MANT_20		0x08
#define KW01_MANT_24		0x10

#define KWW01_RXBW_2600		(KW01_MANT_24 | 7)
#define KWW01_RXBW_3100		(KW01_MANT_20 | 7)
#define KWW01_RXBW_3900		(KW01_MANT_16 | 7)
#define KWW01_RXBW_5200		(KW01_MANT_24 | 6)
#define KWW01_RXBW_6300		(KW01_MANT_20 | 6)
#define KWW01_RXBW_7800		(KW01_MANT_16 | 6)
#define KWW01_RXBW_10400	(KW01_MANT_24 | 5)
#define KWW01_RXBW_12500	(KW01_MANT_20 | 5)
#define KWW01_RXBW_15600	(KW01_MANT_16 | 5)
#define KWW01_RXBW_20800	(KW01_MANT_24 | 4)
#define KWW01_RXBW_25000	(KW01_MANT_20 | 4)
#define KWW01_RXBW_31300	(KW01_MANT_16 | 4)
#define KWW01_RXBW_41700	(KW01_MANT_24 | 3)
#define KWW01_RXBW_50000	(KW01_MANT_20 | 3)
#define KWW01_RXBW_62500	(KW01_MANT_16 | 3)
#define KWW01_RXBW_83300	(KW01_MANT_24 | 2)
#define KWW01_RXBW_100000	(KW01_MANT_20 | 2)
#define KWW01_RXBW_125000	(KW01_MANT_16 | 2)
#define KWW01_RXBW_166700	(KW01_MANT_24 | 1)
#define KWW01_RXBW_200000	(KW01_MANT_20 | 1)
#define KWW01_RXBW_250000	(KW01_MANT_16 | 1)
#define KWW01_RXBW_333300	(KW01_MANT_24 | 0)
#define KWW01_RXBW_400000	(KW01_MANT_20 | 0)
#define KWW01_RXBW_500000	(KW01_MANT_16 | 0)

/* Channel filter bandwidth control during AFC register */

#define KW01_RXBWAFC_DCCFREQ	0xE0	/* DC cutoff frequency */
#define KW01_RXBWAFC_MANT	0x18	/* BW control mantissa */
#define KW01_RXBWAFC_EXP	0x07	/* BW control exponent */

/* OOK demod/delect register */

#define KW01_OOK_PEAK_TTYPE	0xC0	/* Threshold type */
#define KW01_OOK_PEAK_TSTEP	0x38	/* Threshold step */
#define KW01_OOK_PEAK_TDEC	0x07	/* Threshold decrement period */

/* OOK average threshold control register */

#define KW01_OOK_AVG_TFILT	0xC0	/* Threshold coefficients */

/* OOK fixed threshold control register */

#define KW01_OOK_FIX_THRESH	0xFF	/* Fixed threshold value */

/* AFC and FEI control/status register */

#define KW01_AFC_FEI_FEISTS	0x40	/* FEI working/finished */
#define KW01_AFC_FEI_FEISTART	0x20	/* Trigger FEI measurement */
#define KW01_AFC_FEI_AFCSTS	0x10	/* AFC working/finished */
#define KW01_AFC_FEI_AFCACLR	0x08	/* AFC Autoclear on */
#define KW01_AFC_FEI_AFCAUTO	0x04	/* AFC Auto on */
#define KW01_AFC_FEI_AFCCLEAR	0x02	/* Clear AFC value */
#define KW01_AFC_FEI_AFCSTART	0x01	/* Trigger AFC measurement */

/* RSSI configuration register */

#define KW01_RSSICONF_DONE	0x02	/* RSSI working/finished */
#define KW01_RSSICONF_START	0x01	/* Trigger RSSI measurement */

/* Digital I/O pin mapping register 1 */

#define KW01_DIOMAP1_DIO0MAP	0xC0	/* DIO0 mapping */
#define KW01_DIOMAP1_DIO1MAP	0x30	/* DIO1 mapping */
#define KW01_DIOMAP1_DIO2MAP	0x0C	/* DIO2 mapping */
#define KW01_DIOMAP1_DIO3MAP	0x03	/* DIO3 mapping */

/* Digital I/O pin mapping register 1 */

#define KW01_DIOMAP2_DIO4MAP	0xC0	/* DIO4 mapping */
#define KW01_DIOMAP2_DIO5MAP	0x30	/* DIO5 mapping */
#define KW01_DIOMAP2_CLKOUT	0x07	/* ClkOut frequency select */

#define KW01_CLKOUT_FXOSC	0x00	/* FXOSC */
#define KW01_CLKOUT_FXOSCBY2	0x01	/* FXOSC / 2 */
#define KW01_CLKOUT_FXOSCBY4	0x02	/* FXOSC / 4 */
#define KW01_CLKOUT_FXOSCBY8	0x03	/* FXOSC / 8 */
#define KW01_CLKOUT_FXOSCBY16	0x04	/* FXOSC / 16 */
#define KW01_CLKOUT_FXOSCBY32	0x05	/* FXOSC / 32 */
#define KW01_CLKOUT_FXOSCRC	0x06	/* RC */
#define KW01_CLKOUT_FXOSCOFF	0x07	/* off */

/* IRQ flags 1 register */

#define KW01_IRQ1_MODEREADY	0x80	/* Mode request complete */
#define KW01_IRQ1_RXREADY	0x40	/* RX mode, after RSSI/AGC/AFC */
#define KW01_IRQ1_TXREADY	0x20	/* TX mode, after PA ramp-up */
#define KW01_IRQ1_PLLLOCK	0x10	/* PLL locked */
#define KW01_IRQ1_RSSI		0x08	/* RSSI threshold exceeded, rwc */
#define KW01_IRQ1_TIMEOUT	0x04	/* RX start or RSSI thresh timeout */
#define KW01_IRQ1_AUTOMODE	0x02	/* Entered auto mode */
#define KW01_IRQ1_SYNCMATCH	0x01	/* Sync address match r/rwc */

/* IRQ flags 2 register */

#define KW01_IRQ2_FIFOFULL	0x80	/* FIFO is full (66 bytes) */
#define KW01_IRQ2_FIFONOTEMPTY	0x40	/* FIFO has at least 1 byte */
#define KW01_IRQ2_FIFOLEVEL	0x20	/* FIFO exceeds FIFO threshold */
#define KW01_IRQ2_FIFOOFLOW	0x10	/* FIFO overrun */
#define KW01_IRQ2_PACKETSENT	0x08	/* Complete packet sent */
#define KW01_IRQ2_PAYLOADREADY	0x04	/* RX payload ready */
#define KW01_IRQ2_CRCOK		0x02	/* RX packet has good CRC */
#define KW01_IRQ2_LOWBAT	0x01	/* Low battery threshold reached */

/* Synchronization control register */

#define KW01_SYNCCONF_SYNCON	0x80	/* Sync word generation on */
#define KW01_SYNCCONF_FIFOFILL	0x40	/* SyncAddress / FifoFill */
#define KW01_SYNCCONF_SYNCSIZE	0x38	/* Sync size */
#define KW01_SYNCCONF_ERRTOL	0x07	/* Tollerated error bits */

#define KW01_SYNCSIZE_1		0x00
#define KW01_SYNCSIZE_2		0x08
#define KW01_SYNCSIZE_3		0x10
#define KW01_SYNCSIZE_4		0x18	/* default */
#define KW01_SYNCSIZE_5		0x20
#define KW01_SYNCSIZE_6		0x28
#define KW01_SYNCSIZE_7		0x30
#define KW01_SYNCSIZE_8		0x38

/* Packet configuration 1 register */

#define KW01_PKTCONF1_FORMAT	0x80	/* Packet format */
#define KW01_PKTCONF1_DCFREE	0x60	/* DC free encoding/decoding format */
#define KW01_PKTCONF1_CRCON	0x10	/* Enable CRC calculation/check */
#define KW01_PKTCONF1_CRCACLR	0x08	/* Bad CRC failure behavior */
#define KW01_PKTCONF1_AFILT	0x06	/* Address filtering */

#define KW01_FORMAT_FIXED	0x00	/* Fixed length packets */
#define KW01_FORMAT_VARIABLE	0x80	/* Variable length packets */

#define KW01_DCFREE_NONE	0x00	/* No encode/decode */
#define KW01_DCFREE_MANCHESTER	0x20	/* Manchester */
#define KW01_DCFREE_WHITENING	0x40	/* Whitening */

#define KW01_AFILT_NONE		0x00	/* Promiscuous mode */
#define KW01_AFILT_UCAST	0x02	/* Must match unicast */
#define KW01_AFILT_UCASTBCAST	0x04	/* Must match ucast or bcast */

/* Packet configuration 2 register */

#define KW01_PKTCONF2_IPKTDELAY	0xF0	/* Inter-packet delay */
#define KW01_PKTCONF2_RESTARTRX	0x04	/* Force RX restart */
#define KW01_PKTCONF2_AUTORRX	0x02	/* Automatically restart RX */
#define KW01_PKTCONF2_AESON	0x01	/* Enable AES crypto */
