#ifndef TRACE_ENTRY_H
#define TRACE_ENTRY_H

#include <inttypes.h>

typedef enum {
    unknown,
    reg,
    imm,
    mem,
    indir
} value_type_t;

typedef struct {
    uint64_t name;
    uint64_t val;
} register_value_t;

typedef struct {
    uint64_t val;
} immediate_value_t;

typedef struct {
    uint64_t isFar;
    uint64_t addr;
    uint64_t val;
} memory_value_t;

typedef struct {
    uint64_t isFar;
    uint64_t baseNull;
    uint64_t baseName;
    uint64_t baseVal;
    uint64_t disp;
    uint64_t valNull;
    uint64_t val;
} indirect_value_t;

typedef struct {
    uint64_t type;
    union {
        register_value_t reg;
        immediate_value_t imm;
        memory_value_t mem;
        indirect_value_t indir;
    } val;
} operand_value_t;

typedef struct {
    uint64_t pc;
    uint64_t opcode;
    uint64_t numVals;
    operand_value_t vals[8];
} trace_entry_t;

#endif