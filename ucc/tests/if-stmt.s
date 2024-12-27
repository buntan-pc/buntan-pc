section .data

section .text
start:
	call main
	st 6
fin:
	jmp fin
main:
	push 3
	lt
	jz L_1
	push 42
	ret
L_1:
	push 0
	ret
