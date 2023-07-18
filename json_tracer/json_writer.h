#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include "trace_entry.h"
#include "debug_info.h"

typedef struct {
    file_t fileHandle;
    FILE *file;
    bool firstLine;

    debug_info_t *info;
    void *pc, *segmBase, *sp;
} json_trace_t;

/*
 * Creates a JSON trace in a unique file
 */
json_trace_t createTraceFile();

/*
 * Closes the file belonging to a JSON trace
 */
void destroyTraceFile(json_trace_t traceFile);

/*
 * Writes a trace entry to the file
 */
void writeTraceEntry(json_trace_t *traceFile, trace_entry_t entry);

#endif
