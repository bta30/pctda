#include "insertInstructions.h"

/*
 * Inserts instruction to store an operand at a given offset
 */
static void insertStore(instructionContext cont, opnd_t opnd, int offset);

/*
 * Returns the type of an operand
 */
static valueType getType(opnd_t opnd);

/*
 * Saves values for an operand
 */
static void saveOpnd(instructionContext *cont, opnd_t opnd, int *numVals);

/*
 * Changes the value and destination address registers to be some given register,
 * may change the value held in the value register
 */
static void ensureNotUsing(instructionContext *cont, reg_id_t reg1, reg_id_t reg2);

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

void saveOperands(instructionContext *cont) {
    int numVals = 0;

    int srcs = instr_num_srcs(cont->nextInstr);
    for (int i = 0; i < srcs && numVals < VALS_LEN; i++) {
        opnd_t opnd = instr_get_src(cont->nextInstr, i);
        saveOpnd(cont, opnd, &numVals);
    }

    int dsts = instr_num_dsts(cont->nextInstr);
    for (int i = 0; i < dsts && numVals < VALS_LEN; i++) {
        opnd_t opnd = instr_get_dst(cont->nextInstr, i);
        saveOpnd(cont, opnd, &numVals);
    }

    writeValue(*cont, numVals, offsetof(traceEntry, numVals));
}

static void insertStore(instructionContext cont, opnd_t opnd, int offset) {
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_store(cont.drcontext, OPND_CREATE_MEMPTR(cont.regDstAddr, offset), opnd));
}

static valueType getType(opnd_t opnd) {
    if (opnd_is_reg(opnd)) return reg;
    else if (opnd_is_immed(opnd)) return imm;
    else if (opnd_is_abs_addr(opnd) && !opnd_is_base_disp(opnd)) return mem;
    else if (opnd_is_base_disp(opnd)) return indir;
    else return unknown;
}

static void saveOpnd(instructionContext *cont, opnd_t opnd, int *numVals) {
    // Write operand type
    valueType opndType = getType(opnd);
    writeValue(*cont, (uint64_t)opndType, offsetof(traceEntry, vals[*numVals].type));
    
    // Write operand value
    if (opndType == reg) {
        reg_id_t opndReg = opnd_get_reg(opnd);

        const char *regName = get_register_name(opndReg);
        writeValue(*cont, (uint64_t)regName, offsetof(traceEntry, vals[*numVals].val.reg.name));
        
        reg_id_t reg = reg_to_pointer_sized(opndReg);
        if (reg_is_64bit(reg)) {
            ensureNotUsing(cont, reg, reg);
            insertStore(*cont, opnd_create_reg(reg), offsetof(traceEntry, vals[*numVals].val.reg.val));
        }
    } else if (opndType == imm) {
        opnd_set_size(&opnd, OPSZ_8);
        writeValue(*cont, opnd_get_immed_int(opnd), offsetof(traceEntry, vals[*numVals].val.imm.val));
    } else if (opndType == mem) {
        uint64_t isFar = opnd_is_far_abs_addr(opnd);
        writeValue(*cont, isFar, offsetof(traceEntry, vals[*numVals].val.mem.isFar));

        uint64_t *addr = opnd_get_addr(opnd);
        writeValue(*cont, (uint64_t)addr, offsetof(traceEntry, vals[*numVals].val.mem.addr));

        instrlist_meta_preinsert(cont->instrs, cont->nextInstr,
            XINST_CREATE_load(cont->drcontext, opnd_create_reg(cont->regVal), opnd));

        insertStore(*cont, opnd_create_reg(cont->regVal), offsetof(traceEntry, vals[*numVals].val.mem.val));
    } else if (opndType == indir) {
        uint64_t isFar = opnd_is_far_base_disp(opnd);
        writeValue(*cont, isFar, offsetof(traceEntry, vals[*numVals].val.indir.isFar));

        int disp = opnd_get_disp(opnd);
        writeValue(*cont, (uint64_t)disp, offsetof(traceEntry, vals[*numVals].val.indir.disp));

        reg_id_t base = opnd_get_base(opnd);
        const char *baseName = get_register_name(base);
        writeValue(*cont, (uint64_t)baseName, offsetof(traceEntry, vals[*numVals].val.indir.baseName));
        base = reg_to_pointer_sized(base);

        reg_id_t index = opnd_get_index(opnd);
        index = reg_to_pointer_sized(index);

        ensureNotUsing(cont, base, index);

        if (!reg_is_pointer_sized(base)) {
            writeValue(*cont, 1, offsetof(traceEntry, vals[*numVals].val.indir.baseNull));
        } else {
            writeValue(*cont, 0, offsetof(traceEntry, vals[*numVals].val.indir.baseNull));

            insertStore(*cont, opnd_create_reg(base),
                offsetof(traceEntry, vals[*numVals].val.indir.baseVal));
        }

        char opcodeFirstLetter = decode_opcode_name(instr_get_opcode(cont->nextInstr))[0];
        if (opcodeFirstLetter != 'p') {
            writeValue(*cont, 1, offsetof(traceEntry, vals[*numVals].val.indir.valNull));
        } else {
            instrlist_meta_preinsert(cont->instrs, cont->nextInstr,
                XINST_CREATE_load(cont->drcontext, opnd_create_reg(cont->regVal), opnd));
            insertStore(*cont, opnd_create_reg(cont->regVal),
                offsetof(traceEntry, vals[*numVals].val.indir.val));
        }
    }

    (*numVals)++;
}

static void ensureNotUsing(instructionContext *cont, reg_id_t reg1, reg_id_t reg2) {
    drvector_t allowedRegs;
    drreg_init_and_fill_vector(&allowedRegs, true);

    // Only register non-null arguments
    if (reg_is_pointer_sized(reg1)) {
        drreg_set_vector_entry(&allowedRegs, reg, false);
    }
    if (reg_is_pointer_sized(reg2)) {
        drreg_set_vector_entry(&allowedRegs, reg2, false);
    }

    // Swap destination address register to value register if necessary
    if (reg_overlap(reg1, cont->regDstAddr) || reg_overlap(reg2, cont->regDstAddr)) {
        instrlist_meta_preinsert(cont->instrs, cont->nextInstr,
            XINST_CREATE_move(cont->drcontext, opnd_create_reg(cont->regVal), opnd_create_reg(cont->regDstAddr)));
        drreg_unreserve_register(cont->drcontext, cont->instrs, cont->nextInstr, cont->regDstAddr);
        cont->regDstAddr = cont->regVal;
        drreg_reserve_register(cont->drcontext, cont->instrs, cont->nextInstr, &allowedRegs, &cont->regVal);
    }

    // Use new value register if necessary
    if (reg_overlap(reg1, cont->regVal) || reg_overlap(reg2, cont->regVal)) {
        drreg_unreserve_register(cont->drcontext, cont->instrs, cont->nextInstr, cont->regVal);
        drreg_reserve_register(cont->drcontext, cont->instrs, cont->nextInstr, &allowedRegs, &cont->regVal);
    }
}
