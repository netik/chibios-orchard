/*-
 * Copyright (c) 2016-2017
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

#ifndef _PIT_H_
#define _PIT_H_

typedef void (*PIT_FUNC)(void);

typedef struct pit_driver {
	uint8_t *	pit_base;
	PIT_FUNC	pit0_func;
	PIT_FUNC	pit1_func;
} PITDriver;

#ifdef UPDATER
#define KINETIS_PIT_IRQ_VECTOR	38
#define pitEnable(x, y)
#define pitDisable(x, y)
#else
#define KINETIS_PIT_IRQ_VECTOR Vector98
#endif

#define pitStart(x,y) pit0Start(x,y)

#define CSR_READ_4(drv, addr)					\
        *(volatile uint32_t *)((drv)->pit_base + addr)

#define CSR_WRITE_4(drv, addr, data)				\
        do {							\
            volatile uint32_t *pReg =				\
                (uint32_t *)((drv)->pit_base + addr);		\
            *(pReg) = (uint32_t)(data);				\
        } while ((0))

extern PITDriver PIT1;

extern void pit0Start (PITDriver *, PIT_FUNC);
#ifndef UPDATER
extern void pit1Start (PITDriver *, PIT_FUNC);

extern void pitEnable (PITDriver *, uint8_t);
extern void pitDisable (PITDriver *, uint8_t);
#endif

#endif /* _PIT_H_ */
