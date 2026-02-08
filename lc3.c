#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/select.h>
#include <stdbool.h>

enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, 
    R_COND,
    R_COUNT
};
enum
{
    FL_POS = 1 << 0,
    FL_ZRO = 1 << 1,
    FL_NEG = 1 << 2,
};
enum
{
    OP_BR = 0,
    OP_ADD,  
    OP_LD,  
    OP_ST, 
    OP_JSR,
    OP_AND,
    OP_LDR,
    OP_STR,
    OP_RTI,
    OP_NOT,
    OP_LDI,
    OP_STI,
    OP_JMP,
    OP_RES,
    OP_LEA,
    OP_TRAP 
};
enum
{
    MR_KBSR = 0xFE00, 
    MR_KBDR = 0xFE02 
};
enum
{
    TRAP_GETC = 0x20,
    TRAP_OUT = 0x21, 
    TRAP_PUTS = 0x22,
    TRAP_IN = 0x23,  
    TRAP_PUTSP = 0x24,
    TRAP_HALT = 0x25  
};

#define MEMORY_MAX (1 << 16)

typedef struct {
    uint16_t mem[MEMORY_MAX];
    uint16_t reg[R_COUNT];
    int running;
} VM;

void vm_init(VM *vm) {
    memset(vm, 0, sizeof(*vm));

    vm->reg[R_COND] = FL_ZRO;
    vm->reg[R_PC] = 0x3000;
    vm->running = 1;
}

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}
void handle_interrupt(int signal)
{
    printf("\n");
    exit(-2);
}
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}
void update_flags(VM* vm, uint16_t r)
{
    if (vm->reg[r] == 0)
    {
        vm->reg[R_COND] = FL_ZRO;
    }
    else if (vm->reg[r] >> 15)
    {
        vm->reg[R_COND] = FL_NEG;
    }
    else
    {
        vm->reg[R_COND] = FL_POS;
    }
}
void read_image_file(VM* vm, FILE* file)
{
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = vm->mem + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}
int read_image(VM* vm, const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(vm, file);
    fclose(file);
    return 1;
}
void mem_write(VM* vm, uint16_t address, uint16_t val)
{
    vm->mem[address] = val;
}

uint16_t mem_read(VM* vm, uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            vm->mem[MR_KBSR] = (1 << 15);
            vm->mem[MR_KBDR] = getchar();
        }
        else
        {
            vm->mem[MR_KBSR] = 0;
        }
    }
    return vm->mem[address];
}

typedef void (*op_fn)(VM* vm, uint16_t instr);

static void op_bad(VM* vm, uint16_t instr) {
    fprintf(stderr, "Illegal/Unimplemented opcode at PC=0x%04X instr=0x%04x\n",
            (unsigned) (vm->reg[R_PC] - 1), (unsigned) instr);
    vm->running = 0;
} 

static void op_add(VM* vm, uint16_t instr)
{
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag) {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        vm->reg[dr] = vm->reg[sr1] + imm5;
    } else {
        uint16_t sr2 = instr & 0x7;
        vm->reg[dr] = vm->reg[sr1] + vm->reg[sr2];
    }
    update_flags(vm, dr);
}

static void op_br(VM* vm, uint16_t instr)
{
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;
    if (cond_flag & vm->reg[R_COND]) {
        vm->reg[R_PC] += pc_offset;
    }
}

static void op_trap(VM* vm, uint16_t instr)
{
    vm->reg[R_R7] = vm->reg[R_PC];

    switch (instr & 0xFF)
    {
        case TRAP_GETC:
            vm->reg[R_R0] = (uint16_t)getchar();
            update_flags(vm, R_R0);
            break;
        case TRAP_OUT:
            putc((char)vm->reg[R_R0], stdout);
            fflush(stdout);
            break;
        case TRAP_PUTS: {
            uint16_t* c = vm->mem + vm->reg[R_R0];
            while (*c) { putc((char)*c, stdout); ++c; }
            fflush(stdout);
        } break;
        case TRAP_IN: {
            printf("Enter a character: ");
            char c = getchar();
            putc(c, stdout);
            fflush(stdout);
            vm->reg[R_R0] = (uint16_t)c;
            update_flags(vm, R_R0);
        } break;
        case TRAP_PUTSP: {
            uint16_t* c = vm->mem + vm->reg[R_R0];
            while (*c) {
                char char1 = (*c) & 0xFF;
                putc(char1, stdout);
                char char2 = (*c) >> 8;
                if (char2) putc(char2, stdout);
                ++c;
            }
            fflush(stdout);
        } break;
        case TRAP_HALT:
            puts("HALT");
            fflush(stdout);
            vm->running = 0;
            break;
        default:
            abort();
    }
}

static void op_and(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag)
    {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        vm->reg[r0] = vm->reg[r1] & imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        vm->reg[r0] = vm->reg[r1] & vm->reg[r2];
    }
    update_flags(vm, r0);
}

static void op_not(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;

    vm->reg[r0] = ~vm->reg[r1];
    update_flags(vm, r0);
}

static void op_jmp(VM* vm, uint16_t instr)
{
    uint16_t r1 = (instr >> 6) & 0x7;
    vm->reg[R_PC] = vm->reg[r1];
}

static void op_jsr(VM* vm, uint16_t instr)
{
    uint16_t long_flag = (instr >> 11) & 1;
    vm->reg[R_R7] = vm->reg[R_PC];
    if (long_flag)
    {
        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        vm->reg[R_PC] += long_pc_offset;  /* JSR */
    }
    else
    {
        uint16_t r1 = (instr >> 6) & 0x7;
        vm->reg[R_PC] = vm->reg[r1]; /* JSRR */
    }
}

static void op_ld(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    vm->reg[r0] = mem_read(vm, vm->reg[R_PC] + pc_offset);
    update_flags(vm, r0);
}

static void op_ldi(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    vm->reg[r0] = mem_read(vm, mem_read(vm, vm->reg[R_PC] + pc_offset));
    update_flags(vm, r0);
}

static void op_ldr(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    vm->reg[r0] = mem_read(vm, vm->reg[r1] + offset);
    update_flags(vm, r0);
}

static void op_lea(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    vm->reg[r0] = vm->reg[R_PC] + pc_offset;
    update_flags(vm, r0);
}

static void op_st(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(vm, vm->reg[R_PC] + pc_offset, vm->reg[r0]);
}

static void op_sti(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(vm, mem_read(vm, vm->reg[R_PC] + pc_offset), vm->reg[r0]);
}

static void op_str(VM* vm, uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(vm, vm->reg[r1] + offset, vm->reg[r0]);
}

static op_fn op_table[16] = {
    [0 ... 15] = op_bad,   
    [OP_ADD]  = op_add,
    [OP_AND]  = op_and,
    [OP_NOT]  = op_not,
    [OP_BR]   = op_br,
    [OP_JMP]  = op_jmp,
    [OP_JSR]  = op_jsr,
    [OP_LD]   = op_ld,
    [OP_LDI]  = op_ldi,
    [OP_LDR]  = op_ldr,
    [OP_LEA]  = op_lea,
    [OP_ST]   = op_st,
    [OP_STI]  = op_sti,
    [OP_STR]  = op_str,
    [OP_TRAP] = op_trap,
};

int main(int argc, const char* argv[])
{
    VM vm;
    vm_init(&vm);

    if (argc < 2)
    {
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    
    if (!read_image(&vm, argv[1]))
    {
        printf("failed to load image: %s\n", argv[1]);
        exit(1);
    }

    bool trace = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) trace = true;
    }

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();
    atexit(restore_input_buffering);

    while (vm.running)
    {
        uint16_t pc_before = vm.reg[R_PC];
        uint16_t instr = mem_read(&vm, vm.reg[R_PC]++);
        uint16_t op = instr >> 12;

        if (trace)
            printf("PC: 0x%04X Instr: 0x%04X Op: 0x%X\n", 
                    (unsigned) pc_before, (unsigned) instr, (unsigned) op);

        op_table[op](&vm, instr);
    }
}
