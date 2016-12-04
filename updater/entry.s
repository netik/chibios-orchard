

	.cpu		cortex-m0
	.fpu		softvfp

	.thumb

	.file		"entry.s"
	.text

	.section	.text.main,"ax",%progbits
	.align		1
	.code		16
	.thumb_func
	.type		main, %function

	.globl		main

	/* Load an initial stack, then jump to the updater code */
main:
	ldr	r0, = #0x1FFFF800
	mov	sp, r0
	b	updater
