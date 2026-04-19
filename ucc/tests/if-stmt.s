section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
buntan_main:
	push 3
	lt
	jz L_1
	push 42
	ret
L_1:
	push 0
	ret
