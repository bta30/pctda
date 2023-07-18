#ifndef TRACE_ENTRY_H
#define TRACE_ENTRY_H

#include <inttypes.h>

typedef enum {
    unknown,
    reg,
    imm,
    mem,
    indir,
    target
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
    uint64_t pc;
    char name[64];
    void *sp;
} call_target_t;

typedef struct {
    uint64_t type;
    union {
        register_value_t reg;
        immediate_value_t imm;
        memory_value_t mem;
        indirect_value_t indir;
        call_target_t target;
    } val;
} operand_value_t;

typedef struct {
    uint64_t pc;
    uint64_t opcode;
    uint64_t numVals;
    uint64_t bp;
    operand_value_t vals[8];
} trace_entry_t;

extern reg_id_t regSegmBase;
extern uint offset;
extern int tlsSlot;

extern reg_id_t regSegmBase;
extern uint offset;
extern int tlsSlot;

#endif
