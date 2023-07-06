#ifndef INSERT_INSTRUMENTATION_H
#define INSERT_INSTRUMENTATION_H

#include <stddef.h>

#include "dr_api.h"
#include "drreg.h"

#include "trace_entry.h"

/*
 * Initialises drreg
 */
void instrContextInit();

/*
 * Exits drreg
 */
void instrContextDeinit();

/*
 * Inserts recording instrumentation before an instruction
 */
void insertInstrumentation(void *drcontext, instrlist_t *instrs,
                           instr_t *instr, reg_id_t regSegmBase, uint offset);

#endif
