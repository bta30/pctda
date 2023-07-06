#ifndef INSERTINSTRUCTIONS_H
#define INSERTINSTRUCTIONS_H

#include "dr_api.h"
#include "drreg.h"

#include <stddef.h>
#include <inttypes.h>

typedef enum {
    unknown,
    reg,
    imm,
    mem,
    indir
} valueType;

typedef struct {
    uint64_t name;
    uint64_t val;
} registerValue;

typedef struct {
    uint64_t val;
} immediateValue;

typedef struct {
    uint64_t isFar;
    uint64_t addr;
    uint64_t val;
} memoryValue;

typedef struct {
    uint64_t isFar;
    uint64_t baseNull;
    uint64_t baseName;
    uint64_t baseVal;
    uint64_t disp;
    uint64_t valNull;
    uint64_t val;
} indirectValue;

typedef struct {
    uint64_t type;
    union {
        registerValue reg;
        immediateValue imm;
        memoryValue mem;
        indirectValue indir;
    } val;
} operandValue;

#define VALS_LEN 32

typedef struct {
    uint64_t pc;
    uint64_t opcode;
    uint64_t numVals;
    operandValue vals[VALS_LEN];
} traceEntry;

typedef struct {
    void *drcontext;
    instrlist_t *instrs;
    instr_t *nextInstr;
    reg_id_t regDstAddr, regVal;
} instructionContext;

/*
 * Initialises drreg
 */
void instructionContextInit();

/*
 * Exits drreg
 */
void instructionContextDeinit();

/*
 * Creates an instructionContext for adding instructions
 */
instructionContext createInstructionContext(void *drcontext, instrlist_t *instrs, instr_t *nextInstr);

/*
 * Destroys a given instructionContext
 */
void destroyInstructionContext(instructionContext cont);

/*
 * Loads a pointer into a context's destination address register
 */
void loadPointer(instructionContext cont, reg_id_t regSegmBase, int offset);

/*
 * Adds sizeof(traceEntry) to the context's destination address register,
 * then store this register at the given pointer
 */
void addStorePointer(instructionContext cont, reg_id_t regSegmBase, int offset);

/*
 * Writes a given value at an offset from the destination address register
 */
void writeValue(instructionContext cont, uint64_t val, int offset);

/*
 * Saves the program counter of the following instruction
 */
void savePC(instructionContext cont);

/*
 * Saves the opcode of the following instruction
 */
void saveOpcode(instructionContext cont);

/*
 * Saves operand values of the following instruction
 */
void saveOperands(instructionContext *cont);
#endif
