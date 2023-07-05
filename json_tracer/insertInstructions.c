#include "insertInstructions.h"

/*
 * Inserts instruction to store an operand at a given offset
 */
static void insertStore(instructionContext cont, opnd_t opnd, int offset);

/*
 * Saves values for an operand
 */
static void saveOpnd(instructionContext cont, opnd_t opnd, int *numVals);

void instructionContextInit() { drreg_options_t ops = {sizeof(ops), 3, false};
    drreg_init(&ops);
}

void instructionContextDeinit() {
    drreg_exit();
}

instructionContext createInstructionContext(void *drcontext, instrlist_t *instrs, instr_t *nextInstr) {
    instructionContext cont;
    cont.drcontext = drcontext;
    cont.instrs = instrs;
    cont.nextInstr = nextInstr;

    drreg_reserve_register(drcontext, instrs, nextInstr, NULL, &cont.regDstAddr);
    drreg_reserve_register(drcontext, instrs, nextInstr, NULL, &cont.regVal);

    return cont;
}

void destroyInstructionContext(instructionContext cont) {
    drreg_unreserve_register(cont.drcontext, cont.instrs, cont.nextInstr, cont.regVal);
    drreg_unreserve_register(cont.drcontext, cont.instrs, cont.nextInstr, cont.regDstAddr);
}

void loadPointer(instructionContext cont, reg_id_t regSegmBase, int offset) {
    dr_insert_read_raw_tls(cont.drcontext, cont.instrs, cont.nextInstr, regSegmBase, offset, cont.regDstAddr);
}

void addStorePointer(instructionContext cont, reg_id_t regSegmBase, int offset) {
    // Add to regDstAddr
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_add(cont.drcontext, opnd_create_reg(cont.regDstAddr), OPND_CREATE_INT16(sizeof(traceEntry))));

    // Store regDstAddr
    dr_insert_write_raw_tls(cont.drcontext, cont.instrs, cont.nextInstr, regSegmBase, offset, cont.regDstAddr);
}

void writeValue(instructionContext cont, uint64_t val, int offset) {
    // Load val into regVal
    instrlist_insert_mov_immed_ptrsz(cont.drcontext, val, opnd_create_reg(cont.regVal),
                                     cont.instrs, cont.nextInstr, NULL, NULL);

    // Store regVal
    insertStore(cont, opnd_create_reg(cont.regVal), offset);
}

void savePC(instructionContext cont) {
    app_pc pc = instr_get_app_pc(cont.nextInstr);
    writeValue(cont, (uint64_t)pc, offsetof(traceEntry, pc));
}

void saveOpcode(instructionContext cont) {
    int opcode = instr_get_opcode(cont.nextInstr);
    writeValue(cont, opcode, offsetof(traceEntry, opcode));
}

void saveOperands(instructionContext cont) {
    int numVals = 0;

    int srcs = instr_num_srcs(cont.nextInstr);
    for (int i = 0; i < srcs && numVals < VALS_LEN; i++) {
        opnd_t opnd = instr_get_src(cont.nextInstr, i);
        saveOpnd(cont, opnd, &numVals);
    }

    int dsts = instr_num_dsts(cont.nextInstr);
    for (int i = 0; i < dsts && numVals < VALS_LEN; i++) {
        opnd_t opnd = instr_get_dst(cont.nextInstr, i);
        saveOpnd(cont, opnd, &numVals);
    }

    writeValue(cont, numVals, offsetof(traceEntry, numVals));
}

static void insertStore(instructionContext cont, opnd_t opnd, int offset) {
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_store(cont.drcontext, OPND_CREATE_MEMPTR(cont.regDstAddr, offset), opnd));
}

static void saveOpnd(instructionContext cont, opnd_t opnd, int *numVals) {
    uint64_t structOffset = offsetof(traceEntry, vals) + *numVals * sizeof(operandValue);
    uint64_t typeOffset = structOffset + offsetof(operandValue, type);
    uint64_t valOffset = structOffset + offsetof(operandValue, val);

    if (opnd_is_reg_64bit(opnd)) {
        writeValue(cont, (uint64_t)reg, typeOffset);
        insertStore(cont, opnd, valOffset);
    } else if (opnd_is_immed(opnd)) {
        writeValue(cont, (uint64_t)imm, typeOffset);
        //insertStore(cont, opnd, valOffset);
    }

    (*numVals)++;
}
