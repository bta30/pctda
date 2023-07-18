#include "debug_info.h"

#include "dr_api.h"

static int loaded;
static byte *libStart, *libEnd;
static dr_auxlib_handle_t lib;
static const char *libDir = "libdebuginfo.so";

/*
 * Ensures the debugging information library is loaded
 */
static void ensureLoaded();

debug_info_t *getDebugInfo(const char *file) {
    ensureLoaded();

    dr_auxlib_routine_ptr_t ptr;
    ptr = dr_lookup_aux_library_routine(lib, "getDebugInfo");

    debug_info_t *(*func)(const char *);
    func = (debug_info_t *(*)(const char *))ptr;
    return func(file);
}

void destroyDebugInfo(debug_info_t *info) {
    ensureLoaded();
    
    dr_auxlib_routine_ptr_t ptr;
    ptr = dr_lookup_aux_library_routine(lib, "destroyDebugInfo");

    void (*func)(debug_info_t *);
    func = (void(*)(debug_info_t *))ptr;
    return func(info);
}

variable_info_t getVariableInfo(debug_info_t *info, void *varAddr,
                                void *pc, void *segmBase, void *sp) {
    pc -= (size_t)segmBase;
    int stackOffset = varAddr - sp;
    //printf("size  funcs - %i\n", info->sizeFuncs);
    for (int i = 0; i < info->sizeFuncs; i++) {
        if (pc < info->funcs[i].lowPC || pc >= info->funcs[i].lowPC + info->funcs[i].length) {
            continue;
        }

        for (int j = 0; j < info->funcs[i].sizeVars; j++) {
            if (stackOffset >= info->funcs[i].vars[j].offset &&
                stackOffset < info->funcs[i].vars[j].offset + info->funcs[i].vars[j].varInfo.type.size) {

                info->funcs[i].vars[j].varInfo.isLocal = 1;
                return info->funcs[i].vars[j].varInfo;
            }
        }
    }

    void *segmOffset = varAddr - (size_t) segmBase;
    for (int i = 0; i < info->sizeVars; i++) {
        if (segmOffset >= info->vars[i].addr &&
            segmOffset < info->vars[i].addr + info->vars[i].varInfo.type.size) {
            
            info->vars[i].varInfo.isLocal = 0;
            return info->vars[i].varInfo;
        }
    }

    variable_info_t err;
    err.varName = NULL;
    return err;
}


static void ensureLoaded() {
    if (!loaded) {
        lib = dr_load_aux_library(libDir, &libStart, &libEnd);
    }
}


