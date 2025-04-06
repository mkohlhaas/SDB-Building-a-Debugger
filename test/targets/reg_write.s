.global main

.section .data
hex_format: .asciz "%#x"
float_format: .asciz "%.2f"
long_float_format: .asciz "%.2Lf"

.section .text

.macro trap
movq   $62, %rax
movq   %r12, %rdi                   # pid
movq   $5, %rsi                     # SIGTRAP
syscall
.endm

main:
	push %rbp
	movq %rsp, %rbp

	movq $39, %rax                     # get pid syscall
	syscall
	movq %rax, %r12                    # save pid in r12

	trap

	leaq hex_format(%rip), %rdi        # print contents of rsi
	movq $0, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap

	movq %mm0, %rsi                      # print contents of mm0
	leaq hex_format(%rip), %rdi
	movq $0, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap

	leaq float_format(%rip), %rdi        # print contents of xmm0
	movq $1, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap

	subq  $16, %rsp                      # print contents of st0
	fstpt (%rsp)
	leaq  long_float_format(%rip), %rdi
	movq  $0, %rax
	call  printf@plt
	movq  $0, %rdi
	call  fflush@plt
	addq  $16, %rsp

	trap

	popq %rbp
	movq $0, %rax
	ret
