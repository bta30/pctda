#include "jsonout.h"

#include "dr_api.h"
#include "drx.h"

#include <stdio.h>

/*
 * Generates a unique file handle
 */
static file_t getUniqueHandle();

jsonTrace createTraceFile() {
    jsonTrace traceFile;
    traceFile.fileHandle = getUniqueHandle();
    traceFile.file = fdopen(traceFile.fileHandle, "w");
    traceFile.firstLine = true;
    
    fprintf(traceFile.file, "[\n");

    return traceFile;
}

void destroyTraceFile(jsonTrace traceFile) {
    fprintf(traceFile.file, "\n]");

    if (traceFile.file != NULL) {
        fclose(traceFile.file);
    }
}

void writeTraceEntry(jsonTrace *traceFile, traceEntry entry) {
    fprintf(traceFile->file,
            "%s{\"pc\": 0x%lx, \"opcode\": {\"value\": %d, \"name\": %s}, \"operands\": [",
            traceFile->firstLine ? "" : ",\n", entry.pc,
            (int)entry.opcode, decode_opcode_name((int)entry.opcode));
    traceFile->firstLine = false;

    for (int i = 0; i < entry.numVals; i++) {
        if (i != 0) {
            fprintf(traceFile->file, ", ");
        }

        switch (entry.vals[i].type) {
            case (uint64_t)reg:
                fprintf(traceFile->file, 
                        "{\"type\": \"register\", \"name\": \"%s\", \"value\": 0x%lx}",
                        (char *)entry.vals[i].val.reg.name,
                        entry.vals[i].val.reg.val);
                break; 

            case (uint64_t)imm:
                fprintf(traceFile->file,
                        "{\"type\": \"immediate\", \"value\": 0x%lx}",
                        entry.vals[i].val.imm.val);
                break;

            case (uint64_t)mem:
                fprintf(traceFile->file,
                        "{\"type\": \"memory\", \"distance\": \"%s\", "
                        "\"address\": 0x%lx, \"value\": 0x%lx}",
                        entry.vals[i].val.mem.isFar ? "far" : "near",
                        entry.vals[i].val.mem.addr,
                        entry.vals[i].val.mem.val);
                break;

            case (uint64_t)indir:
                fprintf(traceFile->file,
                        "{\"type\": \"indirect\", \"distance\": \"%s\", ",
                        entry.vals[i].val.indir.isFar ? "far" : "near");

                if (entry.vals[i].val.indir.baseNull) {
                    fprintf(traceFile->file,
                            "\"base\": null, \"baseValue\": null, ");
                } else {
                    fprintf(traceFile->file,
                            "\"base\": \"%s\", \"baseValue\": 0x%lx, ",
                            (char *)entry.vals[i].val.indir.baseName,
                            entry.vals[i].val.indir.baseVal);
                }

                fprintf(traceFile->file,
                        "\"offset\": 0x%lx, ",
                        entry.vals[i].val.indir.disp);
                
                if(entry.vals[i].val.indir.valNull) {
                    fprintf(traceFile->file,
                            "\"value\": null}");
                } else {
                    fprintf(traceFile->file,
                            "\"value\": 0x%lx}",
                            entry.vals[i].val.indir.val);
                }
                break;

            default:
                break;
        }
    }

    fprintf(traceFile->file, "]}");
}

static file_t getUniqueHandle() {
    return drx_open_unique_file("./", "trace", "log",
        DR_FILE_ALLOW_LARGE, NULL, 0);
}
