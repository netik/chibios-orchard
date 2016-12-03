#ifndef _PIT_REG_H_
#define _PIT_REG_H_

#define PIT_BASE	0x40037000

#define PIT_MCR		0x000	/* Module control register */
#define PIT_LTMR64H	0x0E0	/* Lifetime counter register upper */
#define PIT_LTMR64L	0x0E4	/* Lifetime counter register lower */
#define PIT_LDVAL0	0x100	/* Timer load register PIT0 */
#define PIT_CVAL0	0x104	/* Current timer value register PIT0 */
#define PIT_TCTRL0	0x108	/* Timer control register PIT0 */
#define PIT_TFLG0	0x10C	/* Timer flag register PIT0 */
#define PIT_LDVAL1	0x110	/* Timer load register PIT1 */
#define PIT_CVAL1	0x114	/* Current timer value register PIT1 */
#define PIT_TCTRL1	0x118	/* Timer control register PIT1 */
#define PIT_TFLG1	0x11C	/* Timer flag register PIT1 */

/* Module control register */

#define PIT_MCR_FRZ	0x00000001	/* Freeze timers in debug mode */
#define PIT_MCR_MDIS	0x00000002	/* Module disable */

/* Timer control register */

#define PIT_TCTRL_TEN	0x00000001	/* Timer enable */
#define PIT_TCTRL_TIE	0x00000002	/* Timer interrupt enable */
#define PIT_TCTRL_CHN	0x00000004	/* Chain mode */

/* Timer interrupt flag register */

#define PIT_TFLG_TIF	0x00000001	/* Timer interrupt has fired */

#endif /* _PIT_REG_H_ */
