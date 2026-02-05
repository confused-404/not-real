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
    uint16_t cond_flag = (instr >> 9) & 0x7;

    if (cond_flag & reg[R_COND]) {
        reg[R_PC] += sign_extend(instr & 0x1FF, 9);
    }
}

void op_jmp(uint16_t reg[], uint16_t instr) {
    uint16_t baser = (instr >> 6) & 0x7;
    reg[R_PC] = reg[baser];
}

void op_jsr(uint16_t reg[], uint16_t instr) {
   reg[R_R7] = reg[R_PC];

   if ((instr >> 11) & 0x1) {
       reg[R_PC] += sign_extend((instr & 0x7FF), 11);
   } else {
       reg[R_PC] = reg[(instr >> 6) & 0x7];
   }
}

void op_ld(uint16_t reg[], uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;

    reg[dr] = sign_extend((instr & 0x1FF), 9);

    update_flags(reg, dr);
}

void op_ldi(uint16_t reg[], uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    
    reg[dr] = reg[reg[R_PC + sign_extend(instr & 0x1FF, 9)]];

    update_flags(reg, dr);
}
















