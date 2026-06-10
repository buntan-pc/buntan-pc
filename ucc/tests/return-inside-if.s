section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
f:
	push 0
	eq
	jz L_1
	push 1
	ret
	jmp L_2
L_1:
	push 0
	ret
L_2:
buntan_main:
	push 0
	call f
	ret
