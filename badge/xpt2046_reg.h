#ifndef _XPT2046_REG_H_
#define _XPT2046_REG_H_


#define XPT_CTL_PD0		0x01	/* Power down select 0 */
#define XPT_CTL_PD1		0x02	/* Power down select 1 */
#define XPT_CTL_SDREF		0x04	/* Single ended/differential refsel */
#define XPT_CTL_MODE		0x08	/* Conversion mode */
#define XPT_CTL_CHAN		0x70	/* Channel select */
#define XPT_CTL_START		0x80	/* Start bit */

#define XPT_PD_POWERDOWN	0x00	/* Power down between conversions */
#define XPT_PD_REFOFF_ADCON	0x01
#define XPT_PD_REFON_ADCOFF	0x02
#define XPT_PD_POWERUP		0x03	/* Always powered */

#define XPT_MODE_8BITS		0x08
#define XPT_MODE_12BITS		0x00

#define XPT_CHAN_TEMP0		0x00
#define XPT_CHAN_Y		0x10	/* Y position */
#define XPT_CHAN_VBAT		0x20	/* Battery voltage */
#define XPT_CHAN_Z1		0x30	/* Z1 position */
#define XPT_CHAN_Z2		0x40	/* Z2 position */
#define XPT_CHAN_X		0x50	/* X position */
#define XPT_CHAN_AUX		0x60	/* Aux input */
#define XPT_CHAN_TEMP		0x70	/* Temperature */

#define XPT_READ_Z1		(XPT_CTL_START | XPT_CHAN_Z1 /*| XPT_CTL_PD0 | XPT_CTL_PD1*/)
#define XPT_READ_Z2		(XPT_CTL_START | XPT_CHAN_Z2 /*| XPT_CTL_PD0|XPT_CTL_PD1*/)
#define XPT_READ_X		(XPT_CTL_START | XPT_CHAN_X /*| XPT_CTL_PD0|XPT_CTL_PD1*/)
#define XPT_READ_Y		(XPT_CTL_START | XPT_CHAN_Y /*| XPT_CTL_PD0|XPT_CTL_PD1*/)

#endif /* _XPT2046_REG_H_ */
