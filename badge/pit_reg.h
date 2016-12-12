/*-
 * Copyright (c) 2016
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

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
