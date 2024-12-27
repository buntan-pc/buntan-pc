section .data

section .text
start:
	call main
	st 6
fin:
	jmp fin
main:
	.push 2
	.push 3
	.push 4
	.mul
	.add
	push $top
	ret
