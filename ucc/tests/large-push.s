section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
buntan_main:
	add fp,-2
	push 57005
	st fp+0
	ld fp+0
	push 15
	sar
	add fp,2
	ret
