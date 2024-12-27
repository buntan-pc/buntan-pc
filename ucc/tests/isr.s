section .data

section .text
start:
	call main
	st 6
fin:
	jmp fin
_ISRfoo:
	iret
main:
	push _ISRfoo
	isr

	push 0
	ret
