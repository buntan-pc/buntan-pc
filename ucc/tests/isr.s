section .data

section .text
start:
	call buntan_main
	st 6
fin:
	jmp fin
_ISRfoo:
	iret
buntan_main:
	push _ISRfoo
	isr

	push 0
	ret
