#include <stdio.h>

#include "dr_api.h"
#include "drx.h"

#include "json_writer.h"

/*
 * Generates a unique file handle
 */
static file_t getUniqueHandle();

/*
 * Writes an operand entry
 */
static void writeOpnd(json_trace_t *traceFile, operand_value_t opndVal);

/*
 * Writes a register operand entry
 */
static void writeReg(json_trace_t *traceFile, register_value_t regVal);

/*
 * Writes an immediate operand entry
 */
static void writeImm(json_trace_t *traceFile, immediate_value_t immVal);

/*
 * Writes a memory operand entry
 */
static void writeMem(json_trace_t *traceFile, memory_value_t memVal);

/*
 * Writes an indirect operand entry
 */
static void writeIndir(json_trace_t *traceFile, indirect_value_t indirVal);

/*
 * Writes a target operand entry
 */
static void writeTarget(json_trace_t *traceFile, call_target_t target);

/*
 * Writes an null operand entry
 */
static void writeNullOpnd(json_trace_t *traceFile);

json_trace_t createTraceFile() {
    json_trace_t traceFile;
    traceFile.fileHandle = getUniqueHandle();
    traceFile.file = fdopen(traceFile.fileHandle, "w");
    traceFile.firstLine = true;
    
    fprintf(traceFile.file, "[\n");

    return traceFile;
}

void destroyTraceFile(json_trace_t traceFile) {
    fprintf(traceFile.file, "\n]");

    if (traceFile.file != NULL) {
        fclose(traceFile.file);
    }
}

void writeTraceEntry(json_trace_t *traceFile, trace_entry_t entry) {
    fprintf(traceFile->file,
            "%s{\"pc\": 0x%lx, \"opcode\": {\"value\": %d, \"name\": %s}, "
            "\"operands\": [",
            traceFile->firstLine ? "" : ",\n", entry.pc,
            (int)entry.opcode, decode_opcode_name((int)entry.opcode));

    traceFile->firstLine = false;

    for (int i = 0; i < entry.numVals; i++) {
        if (i != 0) {
            fprintf(traceFile->file, ", ");
        }

        writeOpnd(traceFile, entry.vals[i]);

    }

    fprintf(traceFile->file, "]}");
}

static file_t getUniqueHandle() {
    return drx_open_unique_file("./", "trace", "log",
                                DR_FILE_ALLOW_LARGE, NULL, 0);
}

static void writeOpnd(json_trace_t *traceFile, operand_value_t opndVal) {
    switch (opndVal.type) {
        case (uint64_t)reg:
            writeReg(traceFile, opndVal.val.reg);
            break; 

        case (uint64_t)imm:
            writeImm(traceFile, opndVal.val.imm);
            break;

        case (uint64_t)mem:
            writeMem(traceFile, opndVal.val.mem);
            break;

        case (uint64_t)indir:
            writeIndir(traceFile, opndVal.val.indir);
            break;

        case (uint64_t) target:
            writeTarget(traceFile, opndVal.val.target);

        default:
            writeNullOpnd(traceFile);
            break;
    }
}

static void writeReg(json_trace_t *traceFile, register_value_t regVal) {
    fprintf(traceFile->file, 
            "{\"type\": \"register\", \"name\": \"%s\", \"value\": 0x%lx}",
            (char *)regVal.name,
            regVal.val);
}

static void writeImm(json_trace_t *traceFile, immediate_value_t immVal) {
    fprintf(traceFile->file,
            "{\"type\": \"immediate\", \"value\": 0x%lx}",
            immVal.val);
}

static void writeMem(json_trace_t *traceFile, memory_value_t memVal) {
    fprintf(traceFile->file,
            "{\"type\": \"memory\", \"distance\": \"%s\", "
            "\"address\": 0x%lx, \"value\": 0x%lx}",
            memVal.isFar ? "far" : "near",
            memVal.addr,
            memVal.val);
}

static void writeIndir(json_trace_t *traceFile, indirect_value_t indirVal) {
    fprintf(traceFile->file,
            "{\"type\": \"indirect\", \"distance\": \"%s\", ",
            indirVal.isFar ? "far" : "near");

    if (indirVal.baseNull) {
        fprintf(traceFile->file, "\"base\": null, \"baseValue\": null, ");
    } else {
        fprintf(traceFile->file,
                "\"base\": \"%s\", \"baseValue\": 0x%lx, ",
                (char *)indirVal.baseName,
                indirVal.baseVal);
    }

    fprintf(traceFile->file, "\"offset\": 0x%lx, ", indirVal.disp);
    
    if(indirVal.valNull) {
        fprintf(traceFile->file, "\"value\": null}");
    } else {
        fprintf(traceFile->file,
                "\"value\": 0x%lx}",
                indirVal.val);
    }
}

static void writeTarget(json_trace_t *traceFile, call_target_t target) {
    fprintf(traceFile->file, "{\"type\": \"target\", \"pc\": 0x%lx, "
            "\"name\": \"%s\"}", target.pc, target.name);
}

static void writeNullOpnd(json_trace_t *traceFile) {
    fprintf(traceFile->file, "{\"type\": null}");
}
