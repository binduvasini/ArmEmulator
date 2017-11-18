#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15
#define ITERS 10000

void execute_sum_array(void);
void execute_find_max(void);
void execute_fib_iter(void);
void execute_fib_rec(void);
void execute_find_str(void);
int sum_array_a(int *a, int len);
int find_max_a(int *a, int len);
int fib_iter_a(int n);
int fib_rec_a(int n);
int find_str_a(char *s, char *sub);

struct arm_state {
    unsigned int regs[NREGS];
    unsigned int cpsr;
    unsigned char stack[STACK_SIZE];
};

struct analysis_report
{
    int no_of_instructions;
    int data_processing_instruction_count;
    int memory_instruction_count;
    int branch_instruction_count;
    int regs_read[NREGS];
    int regs_write[NREGS];
    int branches_taken;
    int branches_not_taken;
};

void init_analysis_report(struct analysis_report *ar){
    int i, j;
    ar->no_of_instructions = 0;
    ar->data_processing_instruction_count = 0;
    ar->memory_instruction_count = 0;
    ar->branch_instruction_count = 0;
    ar->branches_taken = 0;
    ar->branches_not_taken = 0;

    for(i=0; i<NREGS; i++){
        ar->regs_read[i] = 0;
    }

    for(j=0; j<NREGS; j++){
        ar->regs_write[j] = 0;
    }

}

void init_arm_state(struct arm_state *as, unsigned int *func, unsigned int arg0, unsigned int arg1, unsigned int arg2, unsigned int arg3)
{
    int i;

    /* zero out all arm state */
    for (i = 0; i < NREGS; i++) {
        as->regs[i] = 0;
    }

    as->cpsr = 0;

    for (i = 0; i < STACK_SIZE; i++) {
        as->stack[i] = 0;
    }

    as->regs[PC] = (unsigned int) func;
    as->regs[SP] = (unsigned int) &as->stack[STACK_SIZE];
    as->regs[LR] = 0;  //LR holds the return address of a function

    as->regs[0] = arg0;
    as->regs[1] = arg1;
    as->regs[2] = arg2;
    as->regs[3] = arg3;
    
}

/* Data Processing instructions */

bool is_add_inst(unsigned int iw)
{
    unsigned int opcode;
    opcode = (iw >> 21) & 0b1111;
    return (opcode == 0b0100);
}

void armemu_add(struct analysis_report *report, struct arm_state *state, unsigned int rd, int operand1, int operand2, unsigned int cond)
{
    if((cond == state->cpsr && cond != 14) || (cond == 14 && cond != state->cpsr)){
        state->regs[rd] = operand1 + operand2;
        report->regs_write[rd] = 1;
    }
    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
    report->no_of_instructions += 1;
    report->data_processing_instruction_count += 1;
}

bool is_sub_inst(unsigned int iw)
{
    unsigned int opcode;
    opcode = (iw >> 21) & 0b1111;
    return (opcode == 0b0010);
}

void armemu_sub(struct analysis_report *report, struct arm_state *state, unsigned int rd, int operand1, int operand2)
{
    state->regs[rd] = operand1 - operand2;
    report->regs_write[rd] = 1;

    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
    report->no_of_instructions += 1;
    report->data_processing_instruction_count += 1;
}

bool is_cmp_inst(unsigned int iw)
{
    unsigned int opcode;
    opcode = (iw >> 21) & 0b1111;
    return (opcode == 0b1010);
}

void armemu_cmp(struct analysis_report *report, struct arm_state *state, unsigned int rd, int operand1, int operand2){
    if(operand1==operand2){
        state->cpsr=0;
    }
    else if(operand1<operand2){
        state->cpsr=11;
    }
    else if(operand1!=operand2){
        state->cpsr=1;
    }
    
    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
    report->no_of_instructions += 1;
    report->data_processing_instruction_count += 1;
}

bool is_mov_inst(unsigned int iw)
{
    unsigned int opcode;
    opcode = (iw >> 21) & 0b1111;
    
    return (opcode == 0b1101);
}

void armemu_mov(struct analysis_report *report, struct arm_state *state, unsigned int rd, int operand2, unsigned int cond){
    if((cond == state->cpsr && cond != 14) || (cond == 14 && cond != state->cpsr)){  //move only if the condition is AL or is equal to the value in cpsr
        state->regs[rd] = operand2;
        report->regs_write[rd] = 1;
    }
    
    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;        
    }
    report->no_of_instructions += 1;
    report->data_processing_instruction_count += 1;
}

bool is_data_processing_inst(unsigned int iw){
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 0);
}

armemu_data_processing(struct analysis_report *report, unsigned int iw, struct arm_state *state){
    unsigned int rd, i, rm, rn, cond;
    int operand1, operand2;

    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    operand1 = state->regs[rn];
    i = (iw >> 25) & 0b1;
    if(i == 0){  //determine if the second operand is a register or an immediate value
        rm = iw & 0xF;
        operand2 = state->regs[rm];
        report->regs_read[rm] = 1;
    }
    else{
        operand2 = iw & 0xFF;
    }

    cond = (iw >> 28) & 0xF;

    if(is_add_inst(iw)){
        armemu_add(report, state, rd, operand1, operand2, cond);
    }
    else if(is_sub_inst(iw)){
        armemu_sub(report, state, rd, operand1, operand2);
    }
    else if(is_cmp_inst(iw)){
        report->regs_read[rn] = 1;
        armemu_cmp(report, state, rd, operand1, operand2);
    }
    else if(is_mov_inst(iw)){
        armemu_mov(report, state, rd, operand2, cond);
    }
    report->regs_read[PC] = 1;
    report->regs_write[PC] = 1;

}

/* Branch instructions */

is_branch_inst(unsigned int iw){
    unsigned int br;
    br = (iw >> 25) & 0b111;
    return (br == 5);
}

armemu_branch(struct analysis_report *report, unsigned int iw, struct arm_state *state)
{
    unsigned int offset, targetaddr, signBit, l, cond;
    offset = iw & 0x00FFFFFF;
    signBit = (offset >> 23) & 0b1;
    l = (iw >> 24) & 0b1;
    cond = (iw >> 28) & 0xF;
    
    if((cond==state->cpsr && cond!=14) || (cond==14 && cond!=state->cpsr)){
        if(signBit == 1){
            targetaddr = offset | 0xFF000000;  
        }
        else{
            targetaddr = offset | 0;
        }
        targetaddr = targetaddr << 2;
        if(l == 1){  //bl instruction
            state->regs[LR] = state->regs[PC] + 4;
            report->regs_read[PC] = 1;
            report->regs_write[LR] = 1;
        }
        state->regs[PC] = state->regs[PC] + 8 + targetaddr;
        report->branches_taken += 1;
    }
    else{
        state->regs[PC] = state->regs[PC] + 4;
        report->branches_not_taken += 1;
    }
    report->no_of_instructions += 1;
    report->branch_instruction_count += 1;
    report->regs_read[PC] = 1;
    report->regs_write[PC] = 1;
}

/* Branch and exchange instruction */

bool is_bx_inst(unsigned int iw)
{
    unsigned int bx_code;
    bx_code = (iw >> 4) & 0x00FFFFFF;
    return (bx_code == 0b000100101111111111110001);
}

void armemu_bx(struct analysis_report *report, struct arm_state *state)
{
    unsigned int iw;
    unsigned int rn;

    iw = *((unsigned int *) state->regs[PC]);
    rn = iw & 0b1111;
    state->regs[PC] = state->regs[rn];
    report->no_of_instructions += 1;
    report->branch_instruction_count += 1;
    report->branches_taken += 1;
    report->regs_read[rn] = 1;
    report->regs_write[PC] = 1;
}

/* Memory instructions */

bool is_memory_inst(unsigned int iw){
    unsigned int mem;
    mem = (iw >> 26) & 0b11;
    return (mem == 0b01);
}

void armemu_ldr(struct analysis_report *report, struct arm_state *state, unsigned int rd, unsigned int rn, unsigned int offset){
    unsigned int *addr, addrwithoff;
    int value;

    addr = (unsigned int *)state->regs[rn];  //cast it to (unsigned int *) to get the address of rn register
    addrwithoff = (unsigned int)addr+offset;
    state->regs[rd] = *((unsigned int *)addrwithoff);

    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
    report->no_of_instructions += 1;
    report->memory_instruction_count += 1;
    report->regs_read[rn] = 1;
    report->regs_write[rd] = 1;
    report->regs_read[PC] = 1;
    report->regs_write[PC] = 1;
}

void armemu_str(struct analysis_report *report, struct arm_state *state, unsigned int rd, unsigned int rn, unsigned int offset){
    unsigned int *addr, addrwithoff;

    addr = (unsigned int *)state->regs[rn];
    addrwithoff = (unsigned int)addr+offset;
    *((unsigned int *)addrwithoff) = state->regs[rd];

    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
    report->no_of_instructions += 1;
    report->memory_instruction_count += 1;
    report->regs_write[rn] = 1;
    report->regs_read[rd] = 1;
    report->regs_read[PC] = 1;
    report->regs_write[PC] = 1;
}

void armemu_ldrb(struct analysis_report *report, struct arm_state *state, unsigned int rd, unsigned int rn, unsigned int offset){
    char value;
    value = *((char *)state->regs[rn]+offset);  //dereference the address to get the value. If the immediate is 0, it's just the current element. Else it's the next element
    
    state->regs[rd] = (unsigned int)value;

    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
    report->no_of_instructions+=1;
    report->memory_instruction_count += 1;
    report->regs_read[rn] = 1;
    report->regs_write[rd] = 1;
    report->regs_read[PC] = 1;
    report->regs_write[PC] = 1;
}

void armemu_memory(struct analysis_report *report, unsigned int iw, struct arm_state *state){
    unsigned int l, rd, rn, rm, i, offset, b;
    l = (iw >> 20) & 0b1;
    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    i = (iw >> 25) & 0b1;
    b = (iw >> 22) & 0b1;
    
    if(i == 0){
        offset = iw & 0xFFF;
    }
    else{
        rm = iw & 0xF;
        offset = state->regs[rm];
    }

    if(b == 0){
        if(l == 0){
            armemu_str(report, state, rd, rn, offset);
        }
        else{
            armemu_ldr(report, state, rd, rn, offset);
        }
    }
    else{
        armemu_ldrb(report, state, rd, rn, offset);
    }
}

void armemu_execute(struct analysis_report *report, struct arm_state *state)
{
    unsigned int iw;
    
    iw = *((unsigned int *) state->regs[PC]);
    if(is_bx_inst(iw)){
        armemu_bx(report, state);
    }
    else if(is_branch_inst(iw)){
        armemu_branch(report, iw, state);
    }
    else if(is_data_processing_inst(iw)){
        armemu_data_processing(report, iw, state);
    }
    else if(is_memory_inst(iw)){
        armemu_memory(report, iw, state);
    }
}

int armemu(struct analysis_report *report, struct arm_state *state)
{
    while (state->regs[PC] != 0) {
        armemu_execute(report, state);
    }
    return state->regs[0];
}

void print_report(struct analysis_report *report, char *input, int output){
    int i, j;
    printf("%s : %d\n", input, output);
    printf("Number of instructions executed : %d\n", report->no_of_instructions);
    printf("Data Processing instructions : %d\n", report->data_processing_instruction_count);
    printf("Memory instructions : %d\n", report->memory_instruction_count);
    printf("Branch instructions : %d\n", report->branch_instruction_count);
    
    printf("Registers read : ");
    for(i=0; i<NREGS; i++){
        if(report->regs_read[i] == 1){
            printf("r%d, ", i);
        }
    }

    printf("\nRegisters written : " );
    for(j=0; j<NREGS; j++){
        if(report->regs_write[j] == 1){
            printf("r%d, ", j);
        }
    }
    printf("\nNumber of branches taken : %d\n", report->branches_taken);
    printf("Number of branches not taken : %d\n", report->branches_not_taken);
    
}

void print_perf_report_sum_array(int *array, int arrayLen){
    struct arm_state state;
    struct tms t1, t2;
    struct analysis_report report;
    double total_secs = 0.0, total_secs_e = 0.0;
    int i, ticks_per_second, sum, sume;
    ticks_per_second = sysconf(_SC_CLK_TCK);
    
    times(&t1);
    for (i = 0; i < ITERS; i++) {
        sum = sum_array_a(array, arrayLen);
    }
    times(&t2);
    total_secs = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Native execution ------------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs);

    times(&t1);
    for (i = 0; i < ITERS; i++) {
        init_arm_state(&state, (unsigned int *) sum_array_a, (unsigned int)array, arrayLen, 0, 0);
        sume = armemu(&report, &state);
    }
    times(&t2);
    total_secs_e = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Emulated execution ----------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs_e);
    
}

void execute_sum_array(){

    struct arm_state state;
    struct analysis_report report;

    int arrayPositive[] = {0, 1, 2, 3, 4, 5}, arrayPositiveLen = 6;
    int arrayNegative[] = {-1, -2, -3, -4, -5}, arrayNegativeLen = 5;
    int arrayZeros[] = {0, 0, 0, 0}, arrayZerosLen = 4;
    int arrayThousandLen = 1000, arrayThousand[arrayThousandLen];
    int i, sum;
    
    for(i=0; i<arrayThousandLen; i++){
        arrayThousand[i] = i;
    }

    printf("******************************************************************\n");
    printf("                            SUM ARRAY                             \n");
    printf("******************************************************************\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) sum_array_a, (unsigned int)&arrayPositive, arrayPositiveLen, 0, 0);
    sum = armemu(&report, &state);
    print_report(&report, "------------------ Array of positive numbers ---------------------\nArray : {0, 1, 2, 3, 4, 5} :: Sum is", sum);
    print_perf_report_sum_array(arrayPositive, arrayPositiveLen);
    printf("__________________________________________________________________\n\n");
    
    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) sum_array_a, (unsigned int)&arrayNegative, arrayNegativeLen, 0, 0);
    sum = armemu(&report, &state);
    print_report(&report, "------------------- Array of negative numbers --------------------\nArray : {-1, -2, -3, -4, -5} :: Sum is", sum);
    print_perf_report_sum_array(arrayNegative, arrayNegativeLen);
    printf("__________________________________________________________________\n\n");
    
    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) sum_array_a, (unsigned int)&arrayZeros, arrayZerosLen, 0, 0);
    sum = armemu(&report, &state);
    print_report(&report, "------------------------- Array of zeros -------------------------\nArray : {0, 0, 0, 0} :: Sum is", sum);
    print_perf_report_sum_array(arrayZeros, arrayZerosLen);
    printf("__________________________________________________________________\n\n");
    
    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) sum_array_a, (unsigned int)&arrayThousand, arrayThousandLen, 0, 0);
    sum = armemu(&report, &state);
    print_report(&report, "------------------- Array of thousand elements -------------------\nArray of elements from 0 to 999 : Sum is", sum);
    print_perf_report_sum_array(arrayThousand, arrayThousandLen);
    printf("__________________________________________________________________\n\n");
   
}

void print_perf_report_find_max(int *array, int arrayLen){
    struct arm_state state;
    struct tms t1, t2;
    struct analysis_report report;
    double total_secs = 0.0, total_secs_e = 0.0;
    int i, ticks_per_second, maximum, maxe;
    ticks_per_second = sysconf(_SC_CLK_TCK);
    
    times(&t1);
    for (i = 0; i < ITERS; i++) {
        maximum = find_max_a(array, arrayLen);
    }
    times(&t2);
    total_secs = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Native execution ------------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs);

    times(&t1);
    for (i = 0; i < ITERS; i++) {
        init_arm_state(&state, (unsigned int *) find_max_a, (unsigned int)array, arrayLen, 0, 0);
        maxe = armemu(&report, &state);
    }
    times(&t2);
    total_secs_e = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Emulated execution ----------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs_e);
    
}

void execute_find_max(){

    struct arm_state state;
    struct analysis_report report;
    
    int arrayPositive[] = {1, 2, 3, 10, 4, 5}, arrayPositiveLen = 6;
    int arrayNegative[] = {-1, -2, -3, -4, -5}, arrayNegativeLen = 5;
    int arrayZeros[] = {0, 0, 0, 0}, arrayZerosLen = 4;
    int arrayThousandLen = 1000, arrayThousand[arrayThousandLen];
    int i, maximum;

    for(i=0; i<arrayThousandLen; i++){
        arrayThousand[i] = i;
    }

    printf("******************************************************************\n");
    printf("                             FIND MAX                             \n");
    printf("******************************************************************\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) find_max_a, (unsigned int)&arrayPositive, arrayPositiveLen, 0, 0);
    maximum = armemu(&report, &state);
    print_report(&report, "------------------ Array of positive numbers ---------------------\nArray : {1, 2, 3, 10, 4, 5} :: Max is", maximum);
    print_perf_report_find_max(arrayPositive, arrayPositiveLen);
    printf("__________________________________________________________________\n\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) find_max_a, (unsigned int)&arrayNegative, arrayNegativeLen, 0, 0);
    maximum = armemu(&report, &state);
    print_report(&report, "------------------- Array of negative numbers --------------------\nArray : {-1, -2, -3, -4, -5} :: Max is", maximum);
    print_perf_report_find_max(arrayNegative, arrayNegativeLen);
    printf("__________________________________________________________________\n\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) find_max_a, (unsigned int)&arrayZeros, arrayZerosLen, 0, 0);
    maximum = armemu(&report, &state);
    print_report(&report, "------------------------- Array of zeros -------------------------\nArray : {0, 0, 0, 0} :: Max is", maximum);
    print_perf_report_find_max(arrayZeros, arrayZerosLen);
    printf("__________________________________________________________________\n\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) find_max_a, (unsigned int)&arrayThousand, arrayThousandLen, 0, 0);
    maximum = armemu(&report, &state);
    print_report(&report, "------------------- Array of thousand elements -------------------\nArray of elements from 0 to 999 : Max is", maximum);
    print_perf_report_find_max(arrayThousand, arrayThousandLen);
    printf("__________________________________________________________________\n\n");

}

void print_perf_report_fib_iter(int n){
    struct arm_state state;
    struct tms t1, t2;
    struct analysis_report report;
    double total_secs = 0.0, total_secs_e = 0.0;
    int i, ticks_per_second, fib, fibe;
    ticks_per_second = sysconf(_SC_CLK_TCK);
    
    times(&t1);
    for (i = 0; i < ITERS; i++) {
        fib = fib_iter_a(n);
    }
    times(&t2);
    total_secs = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Native execution ------------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs);

    times(&t1);
    for (i = 0; i < ITERS; i++) {
        init_arm_state(&state, (unsigned int *) fib_iter_a, n, 0, 0, 0);
        fibe = armemu(&report, &state);
    }
    times(&t2);
    total_secs_e = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Emulated execution ----------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs_e);
    
}

void execute_fib_iter(){

    struct arm_state state;
    struct analysis_report report;
    int fib;

    printf("******************************************************************\n");
    printf("                       FIBONACCI ITERATION                        \n");
    printf("******************************************************************\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) fib_iter_a, 10, 0, 0, 0);
    fib = armemu(&report, &state);
    print_report(&report, "\nFib of 10 is", fib);
    print_perf_report_fib_iter(10);
    printf("__________________________________________________________________\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) fib_iter_a, 19, 0, 0, 0);
    fib = armemu(&report, &state);
    print_report(&report, "\nFib of 19 is", fib);
    print_perf_report_fib_iter(19);
    printf("__________________________________________________________________\n\n");

}

void print_perf_report_fib_rec(int n){
    struct arm_state state;
    struct tms t1, t2;
    struct analysis_report report;
    double total_secs = 0.0, total_secs_e = 0.0;
    int i, ticks_per_second, fib, fibe;
    ticks_per_second = sysconf(_SC_CLK_TCK);
    
    times(&t1);
    for (i = 0; i < 1000; i++) {
        fib = fib_rec_a(n);
    }
    times(&t2);
    total_secs = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Native execution ------------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs);

    times(&t1);
    for (i = 0; i < 1000; i++) {
        init_arm_state(&state, (unsigned int *) fib_rec_a, n, 0, 0, 0);
        fibe = armemu(&report, &state);
    }
    times(&t2);
    total_secs_e = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Emulated execution ----------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs_e);
    
}

void execute_fib_rec(){

    struct arm_state state;
    struct analysis_report report;
    int fib;

    printf("******************************************************************\n");
    printf("                    FIBONACCI RECURSION                           \n");
    printf("******************************************************************\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) fib_rec_a, 10, 0, 0, 0);
    fib = armemu(&report, &state);
    print_report(&report, "\nFib of 10 is", fib);
    print_perf_report_fib_rec(10);
    printf("__________________________________________________________________\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) fib_rec_a, 19, 0, 0, 0);
    fib = armemu(&report, &state);
    print_report(&report, "\nFib of 19 is", fib);
    print_perf_report_fib_rec(19);
    printf("__________________________________________________________________\n\n");

}

void print_perf_report_find_str(char *s, char *sub){
    struct arm_state state;
    struct tms t1, t2;
    struct analysis_report report;
    double total_secs = 0.0, total_secs_e = 0.0;
    int i, ticks_per_second, index, indexe;
    ticks_per_second = sysconf(_SC_CLK_TCK);
    
    times(&t1);
    for (i = 0; i < ITERS; i++) {
        index = find_str_a(s, sub);
    }
    times(&t2);
    total_secs = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Native execution ------------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs);

    times(&t1);
    for (i = 0; i < ITERS; i++) {
        init_arm_state(&state, (unsigned int *) find_str_a, (unsigned int)s, (unsigned int)sub, 0, 0);
        indexe = armemu(&report, &state);
    }
    times(&t2);
    total_secs_e = ((double) (t2.tms_utime - t1.tms_utime)) / ((double) ticks_per_second);
    printf("---------- Performance measurement - Emulated execution ----------\n");
    printf("Total time taken (in seconds) : %lf\n", total_secs_e);
    
}

void execute_find_str(){
    
    struct arm_state state;
    struct analysis_report report;
    unsigned int index;

    char s[] = "helloworldstring";
    char s1[] = "hello world string";
    char sub[] = "world";
    char sub1[] = "asdf";
    char sub2[] = "string";

    printf("******************************************************************\n");
    printf("                             FIND STRING                          \n");
    printf("******************************************************************\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) find_str_a, (unsigned int)&s, (unsigned int)&sub, 0, 0);
    index = armemu(&report, &state);
    print_report(&report, "\nfind_str_a('helloworldstring', 'world') ", index);
    print_perf_report_find_str(s, sub);
    printf("___________________________________________________________________\n");

    init_analysis_report(&report);
    init_arm_state(&state, (unsigned int *) find_str_a, (unsigned int)&s, (unsigned int)&sub1, 0, 0);
    index = armemu(&report, &state);
    print_report(&report, "\nfind_str_a('helloworldstring', 'asdf') ", index);
    print_perf_report_find_str(s, sub1);
    printf("___________________________________________________________________\n");

    init_analysis_report(&report);    
    init_arm_state(&state, (unsigned int *) find_str_a, (unsigned int)&s1, (unsigned int)&sub2, 0, 0);
    index = armemu(&report, &state);
    print_report(&report, "\nfind_str_a('helloworldstring', 'string') ", index);
    print_perf_report_find_str(s1, sub2);
    printf("___________________________________________________________________\n\n");
    
}

int main(int argc, char *argv[])
{

    if(argc < 2){
        printf("Please enter function name to execute\n");
        exit(-1);
    }

    if(strncmp(argv[1],"all") == 0){
        execute_sum_array();
        execute_find_max();
        execute_fib_iter();
        execute_fib_rec();
        execute_find_str();
    }
    else if(strncmp(argv[1],"sumarray") == 0)
        execute_sum_array();
    else if(strncmp(argv[1],"findmax") == 0)
        execute_find_max();
    else if(strncmp(argv[1],"fibiter") == 0)
        execute_fib_iter();
    else if(strncmp(argv[1],"fibrec") == 0)
        execute_fib_rec();
    else if(strncmp(argv[1],"findstr") == 0)
        execute_find_str();

    return 0;
}
