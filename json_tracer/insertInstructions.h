#ifndef INSERTINSTRUCTIONS_H
#define INSERTINSTRUCIONS_H

#include "dr_api.h"
#include "drreg.h"

#include <stddef.h>
#include <inttypes.h>

typedef enum {
    reg,
    imm
} valueType;

typedef struct {
    uint64_t type;
    uint64_t val;
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
void saveOperands(instructionContext cont);
#endif
