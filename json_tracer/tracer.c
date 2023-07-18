#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"

#include "trace_entry.h"
#include "insert_instrumentation.h"
#include "json_writer.h"
#include "debug_info.h"

#include <string.h>

#define BUF_SIZE (1024 * sizeof(trace_entry_t))

typedef struct {
    byte *segmBase;
    trace_entry_t *buf;
    json_trace_t traceFile;
} thread_data_t;

reg_id_t regSegmBase;
uint offset;
int tlsSlot;

/*
 * Cleans allocated objects
 */
static void eventExit(void);

/*
 * Stores a user module in
 */
static void eventModuleLoad(void *drcontext, const module_data_t *info, bool loaded);

/*
 * 
 */
static void eventModuleUnload(void *drcontext, const module_data_t *info);

/*
 * Allocates a buffer for the current thread
 */
static void eventThreadInit(void *drcontext);

/*
 * Deallocates the buffer for the current thread
 */
static void eventThreadExit(void *drcontext);

/*
 * Inserts clean call into basic block
 */
static dr_emit_flags_t eventInstr(void *drcontext, void *tag,
    instrlist_t *instrs, instr_t *nextInstr, bool for_trace, bool translating,
    void *user_data);

/*
 * Clean call to call outputInstr
 */
static void cleanCall(void);

/*
 * Outputs instructions stored in buffer to JSON file
 */
static void outputInstr(void *drcontext);

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])  {
    drmgr_init();
    drsym_init(0);
    instrContextInit();

    dr_register_exit_event(eventExit);
    drmgr_register_module_load_event(eventModuleLoad);
    drmgr_register_module_unload_event(eventModuleUnload);
    drmgr_register_thread_init_event(eventThreadInit);
    drmgr_register_thread_exit_event(eventThreadExit);
    drmgr_register_bb_instrumentation_event(NULL, eventInstr, NULL);

    tlsSlot = drmgr_register_tls_field();
    dr_raw_tls_calloc(&regSegmBase, &offset, 1, 0);
}

static void eventExit(void) {
    dr_raw_tls_cfree(offset, 1);

    drmgr_unregister_tls_field(tlsSlot);
    drmgr_unregister_bb_insertion_event(eventInstr);
    drmgr_unregister_thread_exit_event(eventThreadExit);
    drmgr_unregister_thread_init_event(eventThreadInit);
    drmgr_unregister_module_unload_event(eventModuleUnload);
    drmgr_unregister_module_load_event(eventModuleLoad);

    instrContextDeinit();
    drsym_exit();
    drmgr_exit();
}

static void eventModuleLoad(void *drcontext, const module_data_t *info, bool loaded) {
    printf("Loading module: %s\n", info->full_path);
}

static void eventModuleUnload(void *drcontext, const module_data_t *info) {
    printf("Unloading module: %s\n", info->full_path);
}

static void eventThreadInit(void *drcontext) {
    thread_data_t *data = dr_thread_alloc(drcontext, sizeof(thread_data_t));
    drmgr_set_tls_field(drcontext, tlsSlot, data);

    data->segmBase = dr_get_dr_segment_base(regSegmBase);
    data->buf = dr_raw_mem_alloc(BUF_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE,
                                 NULL);
    *(trace_entry_t **)(data->segmBase + offset) = data->buf;
    data->traceFile = createTraceFile();
}

static void eventThreadExit(void *drcontext) {
    outputInstr(drcontext);
    thread_data_t *data = drmgr_get_tls_field(drcontext, tlsSlot);
    destroyTraceFile(data->traceFile);
    dr_raw_mem_free(data->buf, BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(thread_data_t));
}

static dr_emit_flags_t eventInstr(void *drcontext, void *tag,
    instrlist_t *instrs, instr_t *nextInstr, bool for_trace, bool translating,
    void *user_data) {

    if (!instr_is_app(nextInstr)) {
        return DR_EMIT_DEFAULT;
    }
    
    insertInstrumentation(drcontext, instrs, nextInstr, regSegmBase, offset);

    if (drmgr_is_first_instr(drcontext, nextInstr)) {
        dr_insert_clean_call(drcontext, instrs, nextInstr, cleanCall, false, 0);
    }

    return DR_EMIT_DEFAULT;
}

static void cleanCall(void) {
    void *drcontext = dr_get_current_drcontext();
    outputInstr(drcontext);
}

static void outputInstr(void *drcontext) {
    thread_data_t *data = drmgr_get_tls_field(drcontext, tlsSlot);
    trace_entry_t *buf = *(trace_entry_t **)(data->segmBase + offset);

    for (trace_entry_t *curr = data->buf; curr < buf; curr++) {
        writeTraceEntry(&data->traceFile, *curr);
    }

    *(trace_entry_t **)(data->segmBase + offset) = data->buf;
}
