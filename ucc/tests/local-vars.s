section .data

section .text
start:
	call main
	st 6
fin:
	jmp fin
main:
	add fp,-6
	st fp+0
	push 42
	st fp+2
	push 2
	st fp+4
	ld fp+2
	push 1
	sub
	add fp,6
	ret
