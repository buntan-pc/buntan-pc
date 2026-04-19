section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
buntan_main:
	.push 2
	.push 3
	.push 4
	.mul
	.add
	push $top
	ret
