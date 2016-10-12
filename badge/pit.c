#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "pit_reg.h"
#include "pit.h"

PITDriver PIT1;

OSAL_IRQ_HANDLER(KINETIS_PIT_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();

	/* Acknowledge the interrupt */

	CSR_WRITE_4(&PIT1, PIT_TFLG0, PIT_TFLG_TIF);

	/* Call the callout function if one is present */

	if (PIT1.pit_func != NULL)
		(*PIT1.pit_func)();

	OSAL_IRQ_EPILOGUE();

	return;
}

void
pitStart (PITDriver * pit, PIT_FUNC func)
{
	pit->pit_base = (uint8_t *)PIT_BASE;
	pit->pit_func = func;

	/* Enable the clock gating for the PIT. */

	SIM->SCGC6 |= SIM_SCGC6_PIT;

	/* Enable the IRQ in the NVIC */

	nvicEnableVector (PIT_IRQn, 5);

	CSR_WRITE_4(pit, PIT_MCR, PIT_MCR_FRZ);

	/*
	 * The PIT is triggered from the bus clock, which is
	 * usually 24MHz. We want it to trigger every 10ms for
	 * the FatFs timer callback.
	 */

	CSR_WRITE_4(pit, PIT_LDVAL0, KINETIS_BUSCLK_FREQUENCY / 100);

	/* Enable timer and interrupt. */

	CSR_WRITE_4(pit, PIT_TFLG0, PIT_TFLG_TIF);
	CSR_WRITE_4(pit, PIT_TCTRL0, PIT_TCTRL_TIE|PIT_TCTRL_TEN);

	return;
}
