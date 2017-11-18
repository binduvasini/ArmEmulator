.global find_str_a
.func find_str_a

find_str_a:
        sub sp, sp, #16
        str r4, [sp]
        str r5, [sp, #4]
        str r6, [sp, #8]
        str r9, [sp, #12]
        mov r12, #0
        sub r12, r12, #1
        mov r2, #0      /*i*/
        mov r3, #0      /*j*/
        mov r5, #0      /*length of sub*/
sublength:
        ldrb r4, [r1, r5]
        cmp r4, #0
        beq whileloop
        add r5, r5, #1
        b sublength
whileloop:
        ldrb r6, [r0, r2]
        cmp r6, #0
        beq done
        ldrb r9, [r1, r3]
        add r2, r2, #1
        cmp r6, r9
        beq ifcondition
        movne r3, #0
        b whileloop
ifcondition:
        cmp r5, r3
        addne r3, r3, #1
        sub r12, r2, r3
done:
        ldr r4, [sp]
        ldr r5, [sp, #4]
        ldr r6, [sp, #8]
        ldr r9, [sp, #12]
        add sp, sp, #16
        mov r0, r12
        bx lr
