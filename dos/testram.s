section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
func1:
	nop
	ret
func2:
	nop
	ret
func3:
	nop
	ret
buntan_main:
	call func1
	call func2
	call func3
	nop
	call func1
	nop
	call func2
	nop
	call func3
	push 3
	ret
