.global fib_iter_a
.func fib_iter_a

fib_iter_a:
        sub sp, sp, #8
        str r4, [sp]
        cmp r0, #0
        moveq r1, #0
        beq done
        cmp r0, #1
        moveq r1, #1
        beq done
        mov r1, #0
        mov r2, #1
        mov r3, #0
        mov r4, #0
loop:
        cmp r0, r3
        beq done
        add r4, r1, r2
        mov r1, r2
        mov r2, r4
        add r3, r3, #1
        b loop
done:
        mov r0, r1
        ldr r4, [sp]
        add sp, sp, #8
        bx lr
