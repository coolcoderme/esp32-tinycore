/*
 * Optional Phase-1 PSRAM smoke stub.
 *
 * Linked at 0x48000000 (start of PSRAM). The linux-loader copies
 * /boot/vmlinuz here and jumps with a0=hartid, a1=dtb.
 *
 * This file is documentation + a starting point; mkimg.py emits a
 * binary RISC-V Image header that loops on `jal zero, 0` until you
 * replace /boot/vmlinuz with a real kernel Image.
 *
 * Build (riscv32 toolchain):
 *   riscv32-unknown-elf-gcc -march=rv32imafc -mabi=ilp32f -nostdlib \
 *       -Ttext=0x48000000 -o psram_stub.elf psram_stub.S
 */
	.section .text
	.globl _start
_start:
	/* UART0 tx fifo on ESP32-P4 (0x500CA000 + FIFO). */
	li	t0, 0x500CA000
	la	t1, msg
1:
	lbu	t2, 0(t1)
	beqz	t2, 2f
	sw	t2, 0(t0)
	addi	t1, t1, 1
	j	1b
2:
	j	2b

	.section .rodata
msg:
	.asciz "\r\nESP32-P4 PSRAM stub: SD -> PSRAM jump OK\r\n"
