#include "dr_api.h"
#include "drmgr.h"
#include "utils.h"

#include "insertInstructions.h"

#include <stdio.h>

#define BUF_SIZE (1024 * sizeof(traceEntry))

typedef struct {
    byte *segmentBase;
    traceEntry *buf;
    file_t fileHandle;
    FILE *file;
} threadData;

reg_id_t regSegmentBase;
uint offset;
int tlsSlot;

/*
 * Cleans allocated objects
 */
static void event_exit(void);

/*
 * Allocates a buffer for the current thread
 */
static void event_thread_init(void *drcontext);

/*
 * Deallocates the buffer for the current thread
 */
static void event_thread_exit(void *drcontext);

/*
 * Inserts clean call into basic block
 */
static dr_emit_flags_t event_instruction(void *drcontext, void *tag,
    instrlist_t *bb, instr_t *instr, bool for_trace, bool translating,
    void *user_data);

/*
 * Inserts instructions to stores the information for the next instruction in the buffer
 */
static void insertRecordInstr(void *drcontext, instrlist_t *bb, instr_t *instr);

/*
 * Outputs instructions stored in buffer to JSON file
 */
static void outputInstr(void *drcontext);

/*
 * Clean call to call outputInstr
 */
static void cleanCall();

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])  {
    drmgr_init();

    instructionContextInit();

    dr_register_exit_event(event_exit);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_instruction, NULL);

    tlsSlot = drmgr_register_tls_field();

    dr_raw_tls_calloc(&regSegmentBase, &offset, 1, 0);
}

static void event_exit(void) {
    dr_raw_tls_cfree(offset, 1);

    drmgr_unregister_tls_field(tlsSlot);
    drmgr_unregister_bb_insertion_event(event_instruction);
    drmgr_unregister_thread_exit_event(event_thread_exit);
    drmgr_unregister_thread_init_event(event_thread_init);

    instructionContextDeinit();

    drmgr_exit();
}

static void event_thread_init(void *drcontext) {
    threadData *data = dr_thread_alloc(drcontext, sizeof(threadData));
    drmgr_set_tls_field(drcontext, tlsSlot, data);

    data->segmentBase = dr_get_dr_segment_base(regSegmentBase);
    data->buf = dr_raw_mem_alloc(BUF_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    *(traceEntry **)(data->segmentBase  + offset) = data->buf;
    
    data->fileHandle = log_file_open(0, drcontext, "./", "jsontracer", DR_FILE_ALLOW_LARGE);
    data->file = log_stream_from_file(data->fileHandle);
    fprintf(data->file, "Opened file\n");
}

static void event_thread_exit(void *drcontext) {
    outputInstr(drcontext);
    threadData *data = drmgr_get_tls_field(drcontext, tlsSlot);
    log_stream_close(data->file);
    dr_raw_mem_free(data->buf, BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(threadData));
}

static dr_emit_flags_t event_instruction(void *drcontext, void *tag,
    instrlist_t *bb, instr_t *instr, bool for_trace, bool translating,
    void *user_data) {

    drmgr_disable_auto_predication(drcontext, bb);

    if (!instr_is_app(instr)) return DR_EMIT_DEFAULT;
    
    insertRecordInstr(drcontext, bb, instr);

    if (drmgr_is_first_instr(drcontext, instr)) {
        dr_insert_clean_call(drcontext, bb, instr, cleanCall, false, 0);
    }

    return DR_EMIT_DEFAULT;
}

static void insertRecordInstr(void *drcontext, instrlist_t *bb, instr_t *instr) {
    instructionContext cont = createInstructionContext(drcontext, bb, instr);
    loadPointer(cont, regSegmentBase, offset);

    savePC(cont);
    saveOpcode(cont);
    saveOperands(&cont);

    addStorePointer(cont, regSegmentBase, offset);
    destroyInstructionContext(cont);
}

static void cleanCall() {
    void *drcontext = dr_get_current_drcontext();
    outputInstr(drcontext);
}

static void outputInstr(void *drcontext) {
    threadData *data = drmgr_get_tls_field(drcontext, tlsSlot);
    traceEntry *buf = *(traceEntry **)(data->segmentBase + offset);

    for (traceEntry *curr = data->buf; curr < buf; curr++) {
        fprintf(data->file, "PC: %lu, Opcode %s - Operands: ", curr->pc, decode_opcode_name((int)curr->opcode));
        for (int i = 0; i < curr->numVals; i++) {
            switch (curr->vals[i].type) {
                case (uint64_t)reg:
                    fprintf(data->file, "Reg %s: %lx, ", (char *)curr->vals[i].val.reg.name,
                                                         curr->vals[i].val.reg.val);
                    break;

                case (uint64_t)imm:
                    fprintf(data->file, "Imm: %lx, ", curr->vals[i].val.imm.val);
                    break;

                case (uint64_t)mem:
                    fprintf(data->file, "%s Absolute Memory Address %lx: %lx, ",
                        curr->vals[i].val.mem.isFar ? "Far" : "Near",
                        curr->vals[i].val.mem.addr, curr->vals[i].val.mem.val);
                    break;

                case (uint64_t)indir:
                    fprintf(data->file, "%s Indirect ", curr->vals[i].val.indir.isFar ? "Far" : "Near");

                    if (curr->vals[i].val.indir.baseNull) {
                        fprintf(data->file, "No Base + ");
                    } else {
                        fprintf(data->file, "Base %s (%lx) + ", 
                            (char *)curr->vals[i].val.indir.baseName,
                            curr->vals[i].val.indir.baseVal);
                    }

                    fprintf(data->file, "Offset %lx: ", curr->vals[i].val.indir.disp);
                    
                    if(curr->vals[i].val.indir.valNull) {
                        fprintf(data->file, "No value read, ");
                    } else {
                        fprintf(data->file, "%lx, ", curr->vals[i].val.indir.val);
                    }
                    break;

                default:
                    break;
            }
        }
        fprintf(data->file, "\n");
    }

    *(traceEntry **)(data->segmentBase + offset) = data->buf;
}
