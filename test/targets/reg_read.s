.global main

.section .data
my_double: .double 64.125

.section .text

.macro trap
movq   $62, %rax
movq   %r12, %rdi
movq   $5, %rsi
syscall
.endm

main:
	push %rbp
	movq %rsp, %rbp

	movq $39, %rax               # get pid
	syscall
	movq %rax, %r12

	movq $0xcafecafe, %r13       # store to r13
	trap

	movb $42, %r13b              # store to r13b
	trap

	movq $0xba5eba11, %r13       # store to mm0
	movq %r13, %mm0
	trap

	movsd my_double(%rip), %xmm0 # store to xmm0
	trap

	emms # store to st0
	fldl my_double(%rip)
	trap

	popq %rbp
	movq $0, %rax
	ret
