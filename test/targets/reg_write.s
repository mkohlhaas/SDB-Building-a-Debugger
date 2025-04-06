.global main

.section .data
hex_format: .asciz "%#x"

.section .text

.macro trap
movq   $62, %rax
movq   %r12, %rdi  # pid
movq   $5, %rsi    # SIGTRAP
syscall
.endm

main:
	push %rbp
	movq %rsp, %rbp

	movq $39, %rax  # get pid syscall
	syscall
	movq %rax, %r12 # save pid in r12

	trap

	leaq hex_format(%rip), %rdi # print contents of rsi
	movq $0, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap

	popq %rbp
	movq $0, %rax
	ret
