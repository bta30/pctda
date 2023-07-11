#include "insert_instrumentation.h"

typedef struct {
    void *drcontext;
    instrlist_t *instrs;
    instr_t *nextInstr;
    reg_id_t regDstAddr, regVal;
} add_instr_context_t;

/*
 * Creates an instructionContext for adding instructions
 */
static add_instr_context_t createInstrContext(void *drcontext,
                                              instrlist_t *instrs,
                                              instr_t *nextInstr);

/*
 * Destroys a given instructionContext
 */
static void destroyInstrContext(add_instr_context_t cont);

/*
 * Loads a pointer into a context's destination address register
 */
static void loadPointer(add_instr_context_t cont, reg_id_t regSegmBase,
                        int offset);

/*
 * Adds some amount to the destination address register
 */
static void addPointer(add_instr_context_t cont, int amount);


/*
 * Store's the destination address register at a given address
 */
static void storePointer(add_instr_context_t cont, reg_id_t regSegmBase,
                            int offset);

/*
 * Loads a given value into the value register
 */
static void loadValueImm(add_instr_context_t cont, uint64_t val);

/*
 * Loads a given operand into the value register
 */
static void loadValueOpnd(add_instr_context_t cont, opnd_t opnd);

/*
 * Stores the value register at an offset from the destination address register
 */
static void storeValue(add_instr_context_t cont, int offset);

/*
 * Saves the program counter of the following instruction
 */
static void savePC(add_instr_context_t cont);

/*
 * Saves the opcode of the following instruction
 */
static void saveOpcode(add_instr_context_t cont);

/*
 * Saves operand values of the following instruction
 */
static void saveOperands(add_instr_context_t *cont);

/*
 * Inserts instruction to store a given register at a given offset
 */
static void storeReg(add_instr_context_t cont, reg_id_t reg, int offset);

/*
 * Returns the type of an operand
 */
static value_type_t getType(opnd_t opnd);

/*
 * Saves values for an operand
 */
static void saveOpnd(add_instr_context_t *cont, opnd_t opnd, int *numVals);

/*
 * Saves a register operand
 */
static void saveReg(add_instr_context_t *cont, opnd_t opnd, int numVals);

/*
 * Saves an immediate operand
 */
static void saveImm(add_instr_context_t *cont, opnd_t opnd, int numVals);

/*
 * Saves a memory operand
 */
static void saveMem(add_instr_context_t *cont, opnd_t opnd, int numVals);

/*
 * Saves an indirect operand
 */
static void saveIndir(add_instr_context_t *cont, opnd_t opnd, int numVals);

/*
 * Changes the value and destination address registers to not be some given
 * register, may change the value held in the value register
 */
static void ensureNotUsing(add_instr_context_t *cont, reg_id_t reg1,
                           reg_id_t reg2);

/*
 * Inserts a move instruction from src to dst
 */
static void move(add_instr_context_t cont, reg_id_t dst, reg_id_t src);

void instrContextInit() {
    drreg_options_t ops = {sizeof(ops), 3, false};
    drreg_init(&ops);
}

void instrContextDeinit() {
    drreg_exit();
}

void insertInstrumentation(void *drcontext, instrlist_t *instrs,
                           instr_t *instr, reg_id_t regSegmBase, uint offset) {

    add_instr_context_t cont = createInstrContext(drcontext, instrs, instr);
    loadPointer(cont, regSegmBase, offset);

    savePC(cont);
    saveOpcode(cont);
    saveOperands(&cont);

    addPointer(cont, sizeof(trace_entry_t));
    storePointer(cont, regSegmBase, offset);
    destroyInstrContext(cont);
}

static add_instr_context_t createInstrContext(void *drcontext,
                                              instrlist_t *instrs,
                                              instr_t *nextInstr) {

    add_instr_context_t cont;
    cont.drcontext = drcontext;
    cont.instrs = instrs;
    cont.nextInstr = nextInstr;

    drreg_reserve_register(drcontext, instrs, nextInstr, NULL,
                           &cont.regDstAddr);
    drreg_reserve_register(drcontext, instrs, nextInstr, NULL, &cont.regVal);

    return cont;
}

static void destroyInstrContext(add_instr_context_t cont) {
    drreg_unreserve_register(cont.drcontext, cont.instrs, cont.nextInstr,
                             cont.regVal);
    drreg_unreserve_register(cont.drcontext, cont.instrs, cont.nextInstr,
                             cont.regDstAddr);
}

static void loadPointer(add_instr_context_t cont, reg_id_t regSegmBase,
                        int offset) {

    dr_insert_read_raw_tls(cont.drcontext, cont.instrs, cont.nextInstr,
                           regSegmBase, offset, cont.regDstAddr);
}

static void addPointer(add_instr_context_t cont, int amount) {
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_add(cont.drcontext, opnd_create_reg(cont.regDstAddr),
        OPND_CREATE_INT16(amount)));
}

static void storePointer(add_instr_context_t cont, reg_id_t regSegmBase,
                            int offset) {

    dr_insert_write_raw_tls(cont.drcontext, cont.instrs, cont.nextInstr,
                            regSegmBase, offset, cont.regDstAddr);
}

static void loadValueImm(add_instr_context_t cont, uint64_t val) {
    instrlist_insert_mov_immed_ptrsz(cont.drcontext, val,
                                     opnd_create_reg(cont.regVal),
                                     cont.instrs, cont.nextInstr, NULL, NULL);
}

static void loadValueOpnd(add_instr_context_t cont, opnd_t opnd) {
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_load(cont.drcontext, opnd_create_reg(cont.regVal), opnd));
}

static void storeValue(add_instr_context_t cont, int offset) {
    storeReg(cont, cont.regVal, offset);
}

static void savePC(add_instr_context_t cont) {
    app_pc pc = instr_get_app_pc(cont.nextInstr);
    loadValueImm(cont, (uint64_t)pc);
    storeValue(cont, offsetof(trace_entry_t, pc));
}

static void saveOpcode(add_instr_context_t cont) {
    int opcode = instr_get_opcode(cont.nextInstr);
    loadValueImm(cont, (uint64_t)opcode);
    storeValue(cont, offsetof(trace_entry_t, opcode));
}

static void saveOperands(add_instr_context_t *cont) {
    int numVals = 0;

    int srcs = instr_num_srcs(cont->nextInstr);
    for (int i = 0; i < srcs; i++) {
        opnd_t opnd = instr_get_src(cont->nextInstr, i);
        saveOpnd(cont, opnd, &numVals);
    }

    int dsts = instr_num_dsts(cont->nextInstr);
    for (int i = 0; i < dsts; i++) {
        opnd_t opnd = instr_get_dst(cont->nextInstr, i);
        saveOpnd(cont, opnd, &numVals);
    }

    loadValueImm(*cont, numVals);
    storeValue(*cont, offsetof(trace_entry_t, numVals));
}

static void storeReg(add_instr_context_t cont, reg_id_t reg, int offset) {
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_store(cont.drcontext,
        OPND_CREATE_MEMPTR(cont.regDstAddr, offset), opnd_create_reg(reg)));
}

static value_type_t getType(opnd_t opnd) {
    if (opnd_is_reg(opnd)) return reg;
    else if (opnd_is_immed(opnd)) return imm;
    else if (opnd_is_abs_addr(opnd) && !opnd_is_base_disp(opnd)) return mem;
    else if (opnd_is_base_disp(opnd)) return indir;
    else return unknown;
}

static void saveOpnd(add_instr_context_t *cont, opnd_t opnd, int *numVals) {
    // Write operand type
    value_type_t opndType = getType(opnd);
    loadValueImm(*cont, (uint64_t)opndType);
    storeValue(*cont, offsetof(trace_entry_t, vals[*numVals].type));
    
    // Write operand value
    switch (opndType) {
        case reg:
            saveReg(cont, opnd, *numVals);
            break;

        case imm:
            saveImm(cont, opnd, *numVals);
            break;

        case mem:
            saveMem(cont, opnd, *numVals);
            break;

        case indir:
            saveIndir(cont, opnd, *numVals);
            break;
    }

    (*numVals)++;
}

static void saveReg(add_instr_context_t *cont, opnd_t opnd, int numVals) {
    reg_id_t opndReg = opnd_get_reg(opnd);

    // Save register name
    const char *regName = get_register_name(opndReg);
    loadValueImm(*cont, (uint64_t)regName);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.reg.name));
    
    // Save register value
    reg_id_t reg = reg_to_pointer_sized(opndReg);
    if (reg_is_pointer_sized(reg)) {
        ensureNotUsing(cont, reg, reg);
        storeReg(*cont, reg, offsetof(trace_entry_t, vals[numVals].val.reg.val));
    }
}

static void saveImm(add_instr_context_t *cont, opnd_t opnd, int numVals) {
    opnd_set_size(&opnd, OPSZ_8);
    loadValueImm(*cont, opnd_get_immed_int(opnd));
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.imm.val));
}

static void saveMem(add_instr_context_t *cont, opnd_t opnd, int numVals) {
    // Save if close or far
    uint64_t isFar = opnd_is_far_abs_addr(opnd);
    loadValueImm(*cont, isFar);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.mem.isFar));
 
    // Save address
    uint64_t *addr = opnd_get_addr(opnd);
    loadValueImm(*cont, (uint64_t)addr);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.mem.addr));
 
    // Save value 
    loadValueOpnd(*cont, opnd);
    storeReg(*cont, cont->regVal, offsetof(trace_entry_t, vals[numVals].val.mem.val));
}

static void saveIndir(add_instr_context_t *cont, opnd_t opnd, int numVals) {
    // Save if close or far
    uint64_t isFar = opnd_is_far_base_disp(opnd);
    loadValueImm(*cont, isFar);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.indir.isFar));

    // Save disp
    int disp = opnd_get_disp(opnd);
    loadValueImm(*cont, (uint64_t)disp);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.indir.disp));

    // Save base name
    reg_id_t base = opnd_get_base(opnd);
    const char *baseName = get_register_name(base);
    loadValueImm(*cont, (uint64_t)baseName);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.indir.baseName));
    base = reg_to_pointer_sized(base);

    // Save base value
    bool baseNull = !reg_is_pointer_sized(base);
    loadValueImm(*cont, baseNull);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.indir.baseNull));
    if (!baseNull) { 
        storeReg(*cont, base,
                 offsetof(trace_entry_t, vals[numVals].val.indir.baseVal));
    }

    // Save value
    reg_id_t index = reg_to_pointer_sized(opnd_get_index(opnd));
    ensureNotUsing(cont, base, index);
    bool validOpcode = instr_reads_memory(cont->nextInstr) && !isFar;
    
    loadValueImm(*cont, !validOpcode);
    storeValue(*cont, offsetof(trace_entry_t, vals[numVals].val.indir.valNull));

    if (validOpcode) {
        opnd = opnd_create_base_disp(base, index, opnd_get_scale(opnd), disp, OPSZ_8);
        loadValueOpnd(*cont, opnd);
        storeReg(*cont, cont->regVal, 
                 offsetof(trace_entry_t, vals[numVals].val.indir.val));
    } else {
    }
}

static void ensureNotUsing(add_instr_context_t *cont, reg_id_t reg1,
                           reg_id_t reg2) {

    drvector_t allowedRegs;
    drreg_init_and_fill_vector(&allowedRegs, true);

    // Only register non-null arguments
    if (reg_is_pointer_sized(reg1)) {
        drreg_set_vector_entry(&allowedRegs, reg, false);
    }
    if (reg_is_pointer_sized(reg2)) {
        drreg_set_vector_entry(&allowedRegs, reg2, false);
    }

    // Swap destination address register and value register if necessary
    if (reg_overlap(reg1, cont->regDstAddr) ||
        reg_overlap(reg2, cont->regDstAddr)) {

        move(*cont, cont->regVal, cont->regDstAddr);
        reg_id_t regTmp = cont->regDstAddr;
        cont->regDstAddr = cont->regVal;
        cont->regVal = regTmp;
    }

    // Use new value register if necessary
    if (reg_overlap(reg1, cont->regVal) || reg_overlap(reg2, cont->regVal)) {
        drreg_unreserve_register(cont->drcontext, cont->instrs,
                                 cont->nextInstr, cont->regVal);
        drreg_reserve_register(cont->drcontext, cont->instrs, cont->nextInstr,
                               &allowedRegs, &cont->regVal);
    }
}

static void move(add_instr_context_t cont, reg_id_t dst, reg_id_t src) {
    instrlist_meta_preinsert(cont.instrs, cont.nextInstr,
        XINST_CREATE_move(cont.drcontext, opnd_create_reg(dst), 
                          opnd_create_reg(src)));
    
}
