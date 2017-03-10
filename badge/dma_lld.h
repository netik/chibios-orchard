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

#ifndef _DMA_LLD_H_
#define _DMA_LLD_H_

#define KINETIS_DMA0_IRQ_VECTOR		Vector40
#define KINETIS_DMA1_IRQ_VECTOR		Vector44
#define KINETIS_DMA2_IRQ_VECTOR		Vector48
#define KINETIS_DMA3_IRQ_VECTOR		Vector4C

#define DMA_CHAN_UART0_RX		2
#define DMA_CHAN_UART0_TX		3
#define DMA_CHAN_UART1_RX		4
#define DMA_CHAN_UART1_TX		5
#define DMA_CHAN_UART2_RX		6
#define DMA_CHAN_UART2_TX		7

#define DMA_CHAN_SPI0_RX		16
#define DMA_CHAN_SPI0_TX		17
#define DMA_CHAN_SPI1_RX		18
#define DMA_CHAN_SPI1_TX		19

#define DMA_CHAN_I2C0			22
#define DMA_CHAN_I2C1			23

#define DMA_CHAN_TPM0_CHAN0		24
#define DMA_CHAN_TPM0_CHAN1		25
#define DMA_CHAN_TPM0_CHAN2		26
#define DMA_CHAN_TPM0_CHAN3		27
#define DMA_CHAN_TPM0_CHAN4		28

#define DMA_CHAN_TPM1_CHAN0		32
#define DMA_CHAN_TPM1_CHAN1		33
#define DMA_CHAN_TPM2_CHAN0		34
#define DMA_CHAN_TPM2_CHAN1		35

#define DMA_CHAN_ADC0			40

#define DMA_CHAN_CMP0			42

#define DMA_CHAN_DAC0			45

#define DMA_CHAN_PTCA			49
#define DMA_CHAN_PTCC			51
#define DMA_CHAN_PTCD			52

#define DMA_CHAN_TPM0_OVFL		54
#define DMA_CHAN_TPM1_OVFL		55
#define DMA_CHAN_TPM2_OVFL		56

#define DMA_CHAN_TSI			57

#define DMA_CHAN_DMAMUX0		60
#define DMA_CHAN_DMAMUX1		61
#define DMA_CHAN_DMAMUX2		62
#define DMA_CHAN_DMAMUX3		63

extern thread_reference_t dma0Thread;
extern thread_reference_t dma1Thread;
extern thread_reference_t dma2Thread;

extern void dmaStart (void);
extern void dmaSend8 (const void * src, uint32_t len);
extern void dmaRecv8 (const void * dst, uint32_t len);
extern void dmaSend16 (const void * src, uint32_t len);

#endif /* _DMA_LLD_H_ */
