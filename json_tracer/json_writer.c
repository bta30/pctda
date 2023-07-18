#include <stdio.h>
#include <string.h>

#include "dr_api.h"
#include "drx.h"
#include "drsyms.h"

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

/*
 * Writes information for an identified variable
 */
static void writeVar(json_trace_t *traceFile, variable_info_t varInfo);

json_trace_t createTraceFile() {
    json_trace_t traceFile;
    traceFile.fileHandle = getUniqueHandle();
    traceFile.file = fdopen(traceFile.fileHandle, "w");
    traceFile.firstLine = true;

    module_data_t *mainModule = dr_get_main_module();
    traceFile.info = getDebugInfo(mainModule->full_path);
    traceFile.segmBase = mainModule->start;
    dr_free_module_data(mainModule);
    
    fprintf(traceFile.file, "[\n");

    return traceFile;
}

void destroyTraceFile(json_trace_t traceFile) {
    fprintf(traceFile.file, "\n]");

    destroyDebugInfo(traceFile.info);

    if (traceFile.file != NULL) {
        fclose(traceFile.file);
    }
}

void writeTraceEntry(json_trace_t *traceFile, trace_entry_t entry) {
    module_data_t *module = dr_lookup_module((byte *)entry.pc);
    size_t offset = (void *)entry.pc - (void *)module->start;

    drsym_info_t info;
    char name[512], file[512];
    info.struct_size = sizeof(info);
    info.name = name;
    info.name_available_size = sizeof(name);
    info.file = file;
    info.file_size = sizeof(file);
    drsym_lookup_address(module->full_path, offset, &info, DRSYM_DEFAULT_FLAGS);

    fprintf(traceFile->file,
            "%s{\"pc\": 0x%lx, \"opcode\": {\"value\": %d, \"name\": \"%s\"}, ",
            traceFile->firstLine ? "" : ",\n", entry.pc,
            (int)entry.opcode, decode_opcode_name((int)entry.opcode));

    if (info.file_available_size > 0) {
        fprintf(traceFile->file, "\"file\": \"%s\", \"line\": %li, ", file, info.line);
    }

    fprintf(traceFile->file, "\"operands\": [");
    module_data_t *mainModule = dr_get_main_module();
    if (strcmp(module->full_path, mainModule->full_path) == 0) {
        traceFile->pc = (void *)entry.pc;
        traceFile->sp = (void *)entry.bp + 0x10;
    }
    dr_free_module_data(mainModule);
    dr_free_module_data(module);

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
        fprintf(traceFile->file, "\"value\": null");
    } else {
        fprintf(traceFile->file,
                "\"value\": 0x%lx",
                indirVal.val);
    }

    uint64_t addr = indirVal.baseVal + indirVal.disp;
    if (traceFile->info != NULL) {
        variable_info_t varInfo = getVariableInfo(traceFile->info,
            (void *)addr, traceFile->pc, traceFile->segmBase,
            traceFile->sp);
        if (varInfo.varName != NULL) {
            fprintf(traceFile->file, ", \"variable\": ");
            writeVar(traceFile, varInfo);
        }
    }

    fprintf(traceFile->file, "}");
}

static void writeTarget(json_trace_t *traceFile, call_target_t target) {
    fprintf(traceFile->file, "{\"type\": \"target\", \"pc\": 0x%lx, "
            "\"name\": \"%s\"}", target.pc, target.name);
}

static void writeNullOpnd(json_trace_t *traceFile) {
    fprintf(traceFile->file, "{\"type\": null}");
}

static void writeVar(json_trace_t *traceFile, variable_info_t varInfo) {
    fprintf(traceFile->file, "{\"name\": \"%s\", \"local\": %s",
            varInfo.varName, varInfo.isLocal ? "true" : "false");

    if (varInfo.type.name != NULL) {
        fprintf(traceFile->file, ", \"type\": {\"name\": \"%s\", \"size\": %u}}",
                varInfo.type.name, varInfo.type.size);
    }
}
