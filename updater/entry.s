

	.set		STACK, 0x1FFFF800

	.cpu		cortex-m0
	.fpu		softvfp

	.thumb
	.text

	.globl		main

	/* Load an initial stack, then jump to the updater code */
main:
	ldr	r0, = STACK
	mov	sp, r0
	b	updater
