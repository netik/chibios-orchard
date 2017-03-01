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

#ifndef _DAC_LLD_H_
#define _DAC_LLD_H_

typedef struct dac_driver {
	uint8_t *	dac_base;
} DACDriver;

#define DAC_THREAD_PRIO		3

#define DAC_SAMPLERATE		9216
#define DAC_SAMPLES		450
#define DAC_BYTES		(DAC_SAMPLES * 2)

#define KINETIS_DAC_IRQ_VECTOR Vector98

#define DAC_READ_1(drv, addr)					\
        *(volatile uint8_t *)((drv)->dac_base + addr)

#define DAC_WRITE_1(drv, addr, data)				\
        do {							\
            volatile uint8_t *pReg =				\
                (uint8_t *)((drv)->dac_base + addr);		\
            *(pReg) = (uint8_t)(data);				\
        } while ((0))

#define DAC_READ_2(drv, addr)					\
        *(volatile uint16_t *)((drv)->dac_base + addr)

#define DAC_WRITE_2(drv, addr, data)				\
        do {							\
            volatile uint16_t *pReg =				\
                (uint16_t *)((drv)->dac_base + addr);		\
            *(pReg) = (uint16_t)(data);				\
        } while ((0))

extern DACDriver DAC1;
extern uint16_t * dacBuf;

extern void dacStart (DACDriver *);
extern void dacPlay (char *);

extern void dacSamplesPlay (uint16_t * p, int cnt);
extern int dacWait (void);

#endif /* _DAC_LLD_H_ */
