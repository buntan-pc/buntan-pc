section .data
f:
	db 0,0

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
buntan_main:
	add fp,-2
	st fp+0
	add fp,-4
	push 4
	st fp+2
	push 1
	st fp+0
	push 3
	ld gp+0
	call
	add fp,4
	add fp,2
	ret
