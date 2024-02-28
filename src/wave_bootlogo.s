
	.align 2
	.balign 4
	.text
	.cpu cortex-a9
	.arch armv7-a
	.syntax unified
	.arm
	.fpu neon

	.section .rodata
	.align	2

	.global wave_bootlogo

wave_bootlogo:
	.incbin "../image/wave_bootlogo.bin", 0x0
