#ifndef JSONOUT_H
#define JSONOUT_H

#include "insertInstructions.h"

#include "dr_api.h"

typedef struct {
    file_t fileHandle;
    FILE *file;
    bool firstLine;
} jsonTrace;

/*
 * Creates a JSON trace in a unique file
 */
jsonTrace createTraceFile();

/*
 * Closes the file belonging to a JSON trace
 */
void destroyTraceFile(jsonTrace traceFile);

/*
 * Writes a trace entry to the file
 */
void writeTraceEntry(jsonTrace *traceFile, traceEntry entry);

#endif
