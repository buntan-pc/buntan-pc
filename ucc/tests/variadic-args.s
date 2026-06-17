section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
init_syscall:
	add fp,-2
	st fp+0
	push 0
	add fp,2
	ret
add:
	add fp,-6
	st fp+0
	push fp+6
	st fp+2
	ld fp+2
	dup 0
	push 2
	add
	st fp+2
	ldd
	st fp+4
	ld fp+0
	ld fp+4
	add
	add fp,6
	ret
buntan_main:
	add fp,-2
	st fp+0
	ld fp+0
	call init_syscall
	pop
	add fp,-2
	ld fp+2
	push 4
	add
	ldd
	st fp+0
	ld fp+2
	ldd
	call add
	add fp,2
	add fp,2
	ret
