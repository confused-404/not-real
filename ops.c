#include "utils.c"

void op_add(uint16_t reg[], uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr1 = (instr >> 6) & 0x7;

    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag) {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[dr] = reg[r1] + imm5;
    } else {
        uint16_t sr2 = instr & 0x7;
        reg[dr] = reg[r1] + reg[r2];
    }

    update_flags(reg, dr);
}

void op_and(uint16_t reg[], uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr1 = (instr >> 6) & 0x7;

    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag) {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[dr] = reg[sr1] & imm5;
    } else {
        reg[dr] = reg[sr1] & (instr & 0x7); 
    }

    update_flags(reg, dr);
}

void op_not(uint16_t reg[], uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr = (instr >> 6) & 0x7;

    reg[dr] = ~reg[sr];

    update_flags(reg, dr);
}

void op_br(uint16_t reg[], uint16_t instr) {
    uint16_t n = (instr >> 11) & 0x1;
    uint16_t z = (instr >> 10) & 0x1;
    uint16_t p = (instr >> 9) & 0x1;

    if (n && reg[R_COND] == FL_NEG ||
        z && reg[R_COND] == FL_ZRO ||
        p && reg[R_COND] == FL_POS) {
        reg[R_PC] += sign_extend(instr & 0x1FF, 9);
    }
}
