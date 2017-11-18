.globl fib_rec_a
.func fib_rec_a

fib_rec_a:
	sub sp, sp, #16
	str lr, [sp]
	cmp r0, #2
	blt done
	str r0, [sp, #4]
	sub r0, r0, #1
	bl fib_rec_a
	str r0, [sp, #8]
	ldr r0, [sp, #4]
	sub r0, r0, #2
	bl fib_rec_a
	ldr r1, [sp, #8]
	add r0, r0, r1
done:
	ldr lr, [sp]
	add sp, sp, #16
	bx lr
