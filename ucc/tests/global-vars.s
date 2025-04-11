section .data
gv:
	db 3,0

section .text
start:
	call main
	st 6
fin:
	jmp fin
main:
	add fp,-2
	st fp+0
	ld gp+gv
	push gp+gv
	push 4095
	and
	add
	add fp,2
	ret
