/*-
 * Copyright (c) 2017
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

#ifndef _DAC_REG_H_
#define _DAC_REG_H_

#define DAC_BASE        0x4003F000

#define DAC0_DAT0L	0x00	/* Data 0 low byte */
#define DAC0_DAT0H	0x01	/* Data 0 higb byte */
#define DAC0_DAT1L	0x02	/* Data 1 low byte */	
#define DAC0_DAT1H	0x03	/* Data 1 high byte */

#define DAC0_SR		0x20	/* Status register */
#define DAC0_C0		0x21	/* Control register 0 */
#define DAC0_C1		0x22	/* Control register 1 */
#define DAC0_C2		0x23	/* Control register 2 */

#define DAC0_C0_EN	0x80	/* DAC enable */
#define DAC0_C0_RFS	0x40	/* Reference select */
#define DAC0_C0_TRS	0x20	/* Trigger select */
#define DAC0_C0_STR	0x10	/* Software trigger */
#define DAC0_C0_LPEN	0x08	/* Low power control */
#define DAC0_C0_BTIE	0x02	/* Buffer read pointer top flag intr enable */
#define DAC0_C0_BBIE	0x01	/* Buffer read pointer bottom intr enable */

#define DAC0_C1_DMAEN	0x80	/* DMA enable */
#define DAC0_C1_BWMS	0x04	/* Buffer work mode select */
#define DAC0_C1_BEN	0x01	/* Buffer enable */

#endif /* _DAC_REG_H_ */
