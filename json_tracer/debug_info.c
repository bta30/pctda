#include "debug_info.h"

#include <dwarf.h>
#include <libdwarf.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MIN_CAPACITY 16

typedef struct {
    int fd;
    Dwarf_Debug dbg;
} dwarf_session_t;

typedef enum {
    func, var, type, error
} entry_type_t;

/*
 * Opens a dwarf session for a given file, returning 0 on success
 */
static int createDwarfSession(const char *file, dwarf_session_t *ses);

/*
 * Closes a dwarf session
 */
static void destroyDwarfSession(dwarf_session_t ses);

/*
 * Returns the debug information for a given dwarf session
 */
static debug_info_t *dwarfToDebugInfo(dwarf_session_t ses);

/*
 * Populates a given debug_info_t with debug information,
 * returning 0 on success
 */
static int populateDebugInfo(dwarf_session_t ses, debug_info_t *info);

/*
 * Allocates memory required in a debug_info_t, returning 0 on success
 */
static int allocate(debug_info_t *info);

/*
 * Gets the root DIE for the next compilation unit of a given dwarf session,
 * returning 0 when other roots remain
 */
static int getNextCURootDie(dwarf_session_t ses, Dwarf_Die *die);

/*
 * Gets the entries for a compilation unit starting at a given DIE,
 * returning 0 on success
 */
static int addCU(dwarf_session_t ses, Dwarf_Die die, debug_info_t *info);

/*
 * Gets the file path of a CU, returning NULL on error
 */
static char *getCUFilePath(dwarf_session_t ses, Dwarf_Die die);

/*
 * Adds an entry from a DIE to the stored debugging information,
 * returning 0 on success
 */
static int addEntry(dwarf_session_t ses, Dwarf_Die die,
                    debug_info_t *info, char *path);

/*
 * Gets the type of an entry to add represented by a DIE
 */
static entry_type_t getEntryType(dwarf_session_t ses, Dwarf_Die die);

/*
 * Adds an entry for a function to the stored debugging information,
 * returning 0 on success
 */
static int addFunc(dwarf_session_t ses, Dwarf_Die die,
                   debug_info_t *info, char *path);

/*
 * Adds an entry for a variable to the stored debugging information,
 * returning 0 on success
 */
static int addVar(dwarf_session_t ses, Dwarf_Die die,
                   debug_info_t *info, char *path);

/*
 * Adds an entry for a type to the stored debugging information,
 * returning 0 on success
 */
static int addType(dwarf_session_t ses, Dwarf_Die die,
                   debug_info_t *info, char *path);

/*
 * Creates an entry for a function from a given DIE
 */
static int getFuncEntry(dwarf_session_t ses, Dwarf_Die die,
                        char *path, function_info_t *func);

/*
 * Creates an entry for a variable from a given DIE
 */
static int getVarEntry(dwarf_session_t ses, Dwarf_Die die,
                        char *path, static_variable_t *var);

/*
 * Creates an entry for a type from a given DIE
 */
static int getTypeEntry(dwarf_session_t ses, Dwarf_Die die,
                        char *path, type_t *type);

/*
 * Appends a function entry to the stored debug information
 */
static int appendFuncEntry(function_info_t func, debug_info_t *info);

/*
 * Appends a variable entry to the stored debug information
 */
static int appendVarEntry(static_variable_t var, debug_info_t *info);

/*
 * Appends a type entry to the stored debug information
 */
static int appendTypeEntry(type_t type, debug_info_t *info);

/*
 * Gets a string value of an attribute and copies it to str,
 * returning 0 on success
 */
static int getAttributeString(Dwarf_Attribute attr, char **str);

/*
 * Gets the type referenced by an attribute, returning 0 on success
 */
static int getAttributeType(dwarf_session_t ses, Dwarf_Attribute attr,
                            Dwarf_Off offsetCU, char *path, type_t *type);

/*
 * Gets the address of a location entry in an attribute,
 * returning 0 on success
 */
static int getLocationEntryAddress(Dwarf_Attribute attr, void **addr);

/*
 * Gets an unsigned value of an attribute, returning 0 on success
 */
static int getAttributeUnsigned(Dwarf_Attribute attr, unsigned *dst);

/*
 * Fills the non-variable fields of a function entry, returning 0 on success
 */
static int fillFuncNonVars(dwarf_session_t ses, Dwarf_Die die,
                           function_info_t *func);

/*
 * Fills the variable fields of a function entry, returning 0 on success
 */
static int fillFuncVars(dwarf_session_t ses, Dwarf_Die die,
                        function_info_t *func);

/*
 * Gets the local variable entry for a given die, returning 0 on success
 */
static int getLocalVarEntry(dwarf_session_t ses, Dwarf_Die die,
                            local_variable_t *var);

/*
 * Appends a local variable entry to those stored in a function_info_t,
 * returning 0 on success
 */
static int appendLocalVarEntry(local_variable_t var, function_info_t *func);

/*
 * Gets a frame offset from an attribute, returning 0 on success
 */
static int getAttributeFrameOffset(Dwarf_Attribute attr, int *offset);

/*
 * Gets the offset for the CU of the current DIE, returning 0 on success
 */
static int getOffsetCU(Dwarf_Die die, Dwarf_Off *offset);

/*
 * Gets an address value of an attribute, returning 0 on success
 */
static int getAttributeAddr(Dwarf_Attribute attr, void **addr);

debug_info_t *getDebugInfo(const char *file) {
    dwarf_session_t ses;
    if (createDwarfSession(file, &ses)) {
        return NULL;
    }

    debug_info_t *info = dwarfToDebugInfo(ses);

    destroyDwarfSession(ses);
    return info;
}

void destroyDebugInfo(debug_info_t *info) {
    for (int i = 0; i < info->sizeFuncs; i++) {
        free(info->funcs[i].name);

        for (int j = 0; j < info->funcs[i].sizeVars; j++) {
            free(info->funcs[i].vars[j].varInfo.varName);
        }

        free(info->funcs[i].vars);
    }
    free(info->funcs);

    for (int i = 0; i < info->sizeVars; i++) {
        free(info->vars[i].varInfo.varName);
        free(info->vars[i].varInfo.type.name);
    }
    free(info->vars);

    for (int i = 0; i < info->sizeTypes; i++) {
        free(info->types[i].name);
    }
    free(info->types);

    free(info);
}

static int createDwarfSession(const char *file, dwarf_session_t *ses) {
    ses->fd = open(file, O_RDONLY);
    if (ses->fd < 0) {
        fprintf(stderr, "Error: Could not open file %s", file);
        return 1;
    }

    Dwarf_Error err;
    int res = dwarf_init_b(ses->fd, DW_GROUPNUMBER_ANY, 0, 0, &ses->dbg, &err);

    if (res == DW_DLV_ERROR) {
        dwarf_dealloc_error(ses->dbg, err);
    }

    if (res != DW_DLV_OK) {
        close(ses->fd);
        return 1;
    }

    return 0;
}

static void destroyDwarfSession(dwarf_session_t ses) {
    dwarf_finish(ses.dbg);
    close(ses.fd);
}

static debug_info_t *dwarfToDebugInfo(dwarf_session_t ses) {
    debug_info_t *info = malloc(sizeof(debug_info_t));
    if (info == NULL) {
        return NULL;
    }

    if (populateDebugInfo(ses, info)) {
        free(info);
        return NULL;
    }

    return info;
}

static int populateDebugInfo(dwarf_session_t ses, debug_info_t *info) {
    if (allocate(info)) {
        return 1;
    }

    while (1) {
        Dwarf_Die die = 0;
        if (getNextCURootDie(ses, &die)) {
            return 0;
        }

        int res = addCU(ses, die, info);
        dwarf_dealloc_die(die);
        if (res) {
            return 1;
        }
    }
}

static int allocate(debug_info_t *info) {
    info->funcs = malloc(sizeof(function_info_t) * MIN_CAPACITY);
    info->vars = malloc(sizeof(static_variable_t) * MIN_CAPACITY);
    info->types = malloc(sizeof(type_t) * MIN_CAPACITY);

    if (info->funcs == NULL || info->vars == NULL
                                 || info->types == NULL) {

        return 1;
    }

    info->sizeFuncs = 0;
    info->sizeVars = 0;
    info->sizeTypes = 0;

    info->capacityFuncs = MIN_CAPACITY;
    info->capacityVars = MIN_CAPACITY;
    info->capacityTypes = MIN_CAPACITY;

    return 0;
}

static int getNextCURootDie(dwarf_session_t ses, Dwarf_Die *die) {
    Dwarf_Unsigned headerLen;
    Dwarf_Half verStamp;
    Dwarf_Off abbrevOffset;
    Dwarf_Half addrSize;
    Dwarf_Half lenSize;
    Dwarf_Half extSize;
    Dwarf_Sig8 typeSign;
    Dwarf_Unsigned typeOffset;
    Dwarf_Unsigned nextCUHeaderOffset;
    Dwarf_Half headerCUType;
    Dwarf_Error err;

    int res = dwarf_next_cu_header_d(ses.dbg, 1, &headerLen, &verStamp,
                                     &abbrevOffset, &addrSize, &lenSize,
                                     &extSize, &typeSign, &typeOffset,
                                     &nextCUHeaderOffset, &headerCUType,
                                     &err);

    if (res == DW_DLV_NO_ENTRY) {
        return 1;
    }

    res = dwarf_siblingof_b(ses.dbg, 0, 1, die, &err);
    return res != DW_DLV_OK;
}

static int addCU(dwarf_session_t ses, Dwarf_Die die, debug_info_t *info) {
    char *path = getCUFilePath(ses, die);

    Dwarf_Die child;
    Dwarf_Error err;
    if (dwarf_child(die, &child, &err) == DW_DLV_NO_ENTRY) {
        return 0;
    }
    
    int res;
    do {
        addEntry(ses, child, info, path);

        Dwarf_Die sibling;
        res = dwarf_siblingof_b(ses.dbg, child, 1, &sibling, &err);
        dwarf_dealloc_die(child);
        child = sibling;
    } while (res == DW_DLV_OK);

    return res != DW_DLV_NO_ENTRY;
}

static char *getCUFilePath(dwarf_session_t ses, Dwarf_Die die) {
    Dwarf_Attribute *attrs;
    Dwarf_Signed numAttrs;
    Dwarf_Error err;

    if (dwarf_attrlist(die, &attrs, &numAttrs, &err) != DW_DLV_OK) {
        return NULL;
    }

    char *path = NULL, *name = NULL;

    for (int i = 0; i < numAttrs; i++) {
        Dwarf_Half attr;
        if (dwarf_whatattr(attrs[i], &attr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(attrs[i]);
            continue;
        }

        if (attr == DW_AT_comp_dir) {
            if (dwarf_formstring(attrs[i], &path, &err) != DW_DLV_OK) {
                path = NULL;
            }
        }

        if (attr == DW_AT_name) {
            if (dwarf_formstring(attrs[i], &name, &err) != DW_DLV_OK) {
                name = NULL;
            }
        }

        dwarf_dealloc_attribute(attrs[i]);
    }

    int lenPath = strlen(path);
    int fullPathSize = lenPath + strlen(name) + 2;
    char *fullPath = malloc(sizeof(char) * fullPathSize);
    if (fullPath != NULL) {
        strcpy(fullPath, path);
        fullPath[lenPath] = '/';
        fullPath[lenPath + 1] = '\0';
        strcat(fullPath, name);
    }

    dwarf_dealloc(ses.dbg, attrs, DW_DLA_LIST);
    return fullPath;
}

static int addEntry(dwarf_session_t ses, Dwarf_Die die,
                     debug_info_t *info, char *path) {

    entry_type_t entryType = getEntryType(ses, die);

    switch (entryType) {
    case func:
        addFunc(ses, die, info, path);
        break;

    case var:
        addVar(ses, die, info, path);
        break;

    case type:
        addType(ses, die, info, path);
        break;

    default:
        return 1;
    }

    return 0;
}

static entry_type_t getEntryType(dwarf_session_t ses, Dwarf_Die die) {
    Dwarf_Attribute *attrs;
    Dwarf_Signed numAttrs;
    Dwarf_Error err;

    if (dwarf_attrlist(die, &attrs, &numAttrs, &err) != DW_DLV_OK) {
        return 1;
    }

    entry_type_t ret = error;

    for (int i = 0; i < numAttrs; i++) {
        Dwarf_Half attr;
        if (dwarf_whatattr(attrs[i], &attr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(attrs[i]);
            continue;
        }

        switch (attr) {
        case DW_AT_low_pc:
            ret = func;
            break;

        case DW_AT_location:
            ret = var;
            break;

        case DW_AT_byte_size:
            ret = type;
            break;
        }

        dwarf_dealloc_attribute(attrs[i]);
    }

    dwarf_dealloc(ses.dbg, attrs, DW_DLA_LIST);
    return ret;
}

static int addFunc(dwarf_session_t ses, Dwarf_Die die,
                   debug_info_t *info, char *path) {

    function_info_t func;
    if (getFuncEntry(ses, die, path, &func)) {
        return 1;
    }
    return appendFuncEntry(func, info);
}

static int addVar(dwarf_session_t ses, Dwarf_Die die,
                   debug_info_t *info, char *path) {

    static_variable_t var;
    if (getVarEntry(ses, die, path, &var)) {
        return 1;
    }
    return appendVarEntry(var, info);
}

static int addType(dwarf_session_t ses, Dwarf_Die die,
                   debug_info_t *info, char *path) {

    type_t type;
    if (getTypeEntry(ses, die, path, &type)) {
        return 1;
    }
    return appendTypeEntry(type, info);
}

static int getFuncEntry(dwarf_session_t ses, Dwarf_Die die,
                        char *path, function_info_t *func) {

    func->vars = malloc(sizeof(local_variable_t) * MIN_CAPACITY);
    if (func->vars == NULL) {
        return 1;
    }

    func->sizeVars = 0;
    func->capacityVars = MIN_CAPACITY;

    func->name = NULL;
    func->path = path;

    return fillFuncNonVars(ses, die, func) ||
           fillFuncVars(ses, die, func);
}

static int getVarEntry(dwarf_session_t ses, Dwarf_Die die,
                        char *path, static_variable_t *var) {

    var->varInfo.varName = NULL;
    var->varInfo.type.size = 0;
    var->path = path;
    var->addr = NULL;

    Dwarf_Attribute *attrs;
    Dwarf_Signed numAttrs;
    Dwarf_Error err;
    if (dwarf_attrlist(die, &attrs, &numAttrs, &err) != DW_DLV_OK) {
        return 1;
    }

    for (int i = 0; i < numAttrs; i++) {
        Dwarf_Half attr;
        if (dwarf_whatattr(attrs[i], &attr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(attrs[i]);
            continue;
        }

        switch(attr) {
        case DW_AT_name:
            if (getAttributeString(attrs[i], &var->varInfo.varName)) {
                var->varInfo.varName = NULL;
            }
            break;

        case DW_AT_type:
            Dwarf_Off offsetCU;
            if (getOffsetCU(die, &offsetCU) ||
                getAttributeType(ses, attrs[i], offsetCU, path,
                                 &var->varInfo.type)) {

                var->varInfo.type.size = 0;
            }
            break;

        case DW_AT_location:
            if (getLocationEntryAddress(attrs[i], &var->addr)) {
                var->addr = 0;
            }
            break;
        }

        dwarf_dealloc_attribute(attrs[i]);
    }

    dwarf_dealloc(ses.dbg, attrs, DW_DLA_LIST);
    return var->varInfo.varName == NULL || var->varInfo.type.size == 0
                                        || var->addr == NULL;
}

static int getTypeEntry(dwarf_session_t ses, Dwarf_Die die,
                        char *path, type_t *type) {
    type->name = NULL;
    type->path = NULL;
    type->size = 0;

    Dwarf_Attribute *attrs;
    Dwarf_Signed numAttrs;
    Dwarf_Error err;
    if (dwarf_attrlist(die, &attrs, &numAttrs, &err) != DW_DLV_OK) {
        return 1;
    }

    for (int i = 0; i < numAttrs; i++) {
        Dwarf_Half attr;
        if (dwarf_whatattr(attrs[i], &attr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(attrs[i]);
            continue;
        }

        switch (attr) {
        case DW_AT_name:
            if (getAttributeString(attrs[i], &type->name)) {
                type->name = NULL;
            }
            break;

        case DW_AT_byte_size:
            if (getAttributeUnsigned(attrs[i], &type->size)) {
                type->size = 0;
            }
            break;
        }

        dwarf_dealloc_attribute(attrs[i]);
    }

    dwarf_dealloc(ses.dbg, attrs, DW_DLA_LIST);
    return type->name == NULL || type->size == 0;
}

static int appendFuncEntry(function_info_t func, debug_info_t *info) {
    if (info->sizeFuncs == info->capacityFuncs) {
        info->capacityFuncs *= 2;
        function_info_t *funcs = reallocarray(info->funcs,
                                              info->capacityFuncs,
                                              sizeof(function_info_t));

        if (funcs == NULL) {
            return 1;
        }

        info->funcs = funcs;
    }

    info->funcs[info->sizeFuncs++] = func;
    return 0;
}

static int appendVarEntry(static_variable_t var, debug_info_t *info) {
    if (info->sizeVars == info->capacityVars) {
        info->capacityVars *= 2;
        static_variable_t *vars = reallocarray(info->vars,
                                               info->capacityVars,
                                               sizeof(static_variable_t));

        if (vars == NULL) {
            return 1;
        }

        info->vars = vars;
    }

    info->vars[info->sizeVars++] = var;
    return 0;
}

static int appendTypeEntry(type_t type, debug_info_t *info) {
    if (info->sizeTypes == info->capacityTypes) {
        info->capacityTypes *= 2;
        type_t *types = reallocarray(info->types,
                                     info->capacityTypes,
                                     sizeof(type_t));

        if (types == NULL) {
            return 1;
        }

        info->types = types;
    }

    info->types[info->sizeTypes++] = type;
    return 0;
}

static int getAttributeString(Dwarf_Attribute attr, char **str) {
    Dwarf_Error err;
    char *givenStr;

    if (dwarf_formstring(attr, &givenStr, &err) != DW_DLV_OK) {
        return 1;
    }

    int size = strlen(givenStr) + 1;
    *str = malloc(sizeof(char) * size);
    if (*str == NULL) {
        return 1;
    }

    strcpy(*str, givenStr);

    return 0;
}

static int getAttributeType(dwarf_session_t ses, Dwarf_Attribute attr,
                            Dwarf_Off offsetCU, char *path, type_t *type) {

    Dwarf_Off dieOffset;
    Dwarf_Bool isInfo;
    Dwarf_Error err;
    if (dwarf_formref(attr, &dieOffset, &isInfo, &err) != DW_DLV_OK) {
        return 1;
    }

    dieOffset += offsetCU;

    Dwarf_Die die;
    if (dwarf_offdie_b(ses.dbg, dieOffset, isInfo, &die, &err) != DW_DLV_OK) {
        return 1;
    }

    int res = getTypeEntry(ses, die, path, type);
    dwarf_dealloc_die(die);
    return res;
}

static int getLocationEntryAddress(Dwarf_Attribute attr, void **addr) {
    Dwarf_Loc_Head_c head;
    Dwarf_Unsigned size;
    Dwarf_Error err;

    if (dwarf_get_loclist_c(attr, &head, &size, &err) != DW_DLV_OK) {
        return 1;
    }

    Dwarf_Small lleVal;
    Dwarf_Unsigned lopc, hipc;
    Dwarf_Bool debugAddrAvail;
    Dwarf_Addr lopcCooked, hipcCooked;
    Dwarf_Unsigned opsSize;
    Dwarf_Locdesc_c entry;
    Dwarf_Small source;
    Dwarf_Unsigned exprOffset, descOffset;
    int res = dwarf_get_locdesc_entry_d(head, 0, &lleVal, &lopc, &hipc,
                                        &debugAddrAvail, &lopcCooked,
                                        &hipcCooked, &opsSize, &entry, &source,
                                        &exprOffset, &descOffset, &err);
    if (res != DW_DLV_OK || opsSize != 1) {
        return 1;
    }

    Dwarf_Small op;
    Dwarf_Unsigned opnds[4];
    res = dwarf_get_location_op_value_c(entry, 0, &op, &opnds[0], &opnds[1],
                                        &opnds[2], &opnds[3], &err);

    if (res != DW_DLV_OK) {
        return 1;
    }

    if (op != DW_OP_addr) {
        fprintf(stderr, "Error: Expected DW_OP_addr");
        return 1;
    }

    *addr = (void *)opnds[0];
    return 0;
}

static int getAttributeUnsigned(Dwarf_Attribute attr, unsigned *dst) {
    Dwarf_Unsigned val;
    Dwarf_Error err;

    if (dwarf_formudata(attr, &val, &err) != DW_DLV_OK) {
        return 1;
    }

    *dst = val;
    return 0;
}

static int fillFuncNonVars(dwarf_session_t ses, Dwarf_Die die,
                           function_info_t *func) {
    
    Dwarf_Attribute *attrs;
    Dwarf_Signed numAttrs;
    Dwarf_Error err;
    if (dwarf_attrlist(die, &attrs, &numAttrs, &err) != DW_DLV_OK) {
        return 1;
    }

    func->name = NULL;
    func->lowPC = NULL;
    int setLength = 0;
    for (int i = 0; i < numAttrs; i++) {
        Dwarf_Half attr;
        if (dwarf_whatattr(attrs[i], &attr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(attrs[i]);
            continue;
        }

        switch (attr) {
        case DW_AT_name:
            if (getAttributeString(attrs[i], &func->name)) {
                func->name = NULL;
            }
            break;

        case DW_AT_low_pc:
            if (getAttributeAddr(attrs[i], &func->lowPC)) {
                func->lowPC = NULL;
            }
        
        case DW_AT_high_pc:
            void *highAddr;
            if (getAttributeUnsigned(attrs[i], &func->length) == 0) {
                setLength = 1;
            } else if (getAttributeAddr(attrs[i], &highAddr) == 0) {
                func->length = func->lowPC - highAddr;
                setLength = 1;
            }
            break;
        }

        dwarf_dealloc_attribute(attrs[i]);
    }

    dwarf_dealloc(ses.dbg, attrs, DW_DLA_LIST);

    
    return func->name == NULL || func->lowPC == NULL
                              || !setLength;
}

static int fillFuncVars(dwarf_session_t ses, Dwarf_Die die,
                        function_info_t *func) {

    Dwarf_Die child;
    Dwarf_Error err;
    if (dwarf_child(die, &child, &err) == DW_DLV_NO_ENTRY) {
        return 0;
    }

    int res;
    do {
        local_variable_t var;
        if (getLocalVarEntry(ses, child, &var) == 0) {
            appendLocalVarEntry(var, func);
        }

        Dwarf_Die sibling;
        res = dwarf_siblingof_b(ses.dbg, child, 1, &sibling, &err);
        dwarf_dealloc_die(child);
        child = sibling;
    } while (res == DW_DLV_OK);

    return 0;
}

static int getLocalVarEntry(dwarf_session_t ses, Dwarf_Die die,
                            local_variable_t *var) {

    var->varInfo.varName = NULL;
    var->varInfo.type.size = 0;
    int setOffset = 0;

    Dwarf_Attribute *attrs;
    Dwarf_Signed numAttrs;
    Dwarf_Error err;

    if (dwarf_attrlist(die, &attrs, &numAttrs, &err) != DW_DLV_OK) {
        return 1;
    }

    for (int i = 0; i < numAttrs; i++) {
        Dwarf_Half attr;
        if (dwarf_whatattr(attrs[i], &attr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(attrs[i]);
            continue;
        }

        switch (attr) {
        case DW_AT_name:
            if (getAttributeString(attrs[i], &var->varInfo.varName)) {
                var->varInfo.varName = NULL;
            }
            break;

        case DW_AT_type:
            Dwarf_Off offsetCU;
            if (getOffsetCU(die, &offsetCU) ||
                getAttributeType(ses, attrs[i], offsetCU, NULL,
                                 &var->varInfo.type)) {

                var->varInfo.type.size = 0;
            }
            break;

        case DW_AT_location:
            if (getAttributeFrameOffset(attrs[i], &var->offset) == 0) {
                setOffset = 1;
            }
            break;
        }

        dwarf_dealloc_attribute(attrs[i]);
    }

    dwarf_dealloc(ses.dbg, attrs, DW_DLA_LIST);
    return var->varInfo.varName == NULL || var->varInfo.type.size == 0
                                        || !setOffset;
}

static int appendLocalVarEntry(local_variable_t var, function_info_t *func) {
    if (func->sizeVars == func->capacityVars) {
        func->capacityVars *= 2;
        local_variable_t *vars = reallocarray(func->vars,
                                              func->capacityVars,
                                              sizeof(local_variable_t));

        if (vars == NULL) {
            return 1;
        }

        func->vars = vars;
    }

    func->vars[func->sizeVars++] = var;
    return 0;
}


static int getAttributeFrameOffset(Dwarf_Attribute attr, int *offset) {
    Dwarf_Loc_Head_c head;
    Dwarf_Unsigned size;
    Dwarf_Error err;

    if (dwarf_get_loclist_c(attr, &head, &size, &err) != DW_DLV_OK) {
        return 1;
    }

    Dwarf_Small lleVal;
    Dwarf_Unsigned lopc, hipc;
    Dwarf_Bool debugAddrAvail;
    Dwarf_Addr lopcCooked, hipcCooked;
    Dwarf_Unsigned opsSize;
    Dwarf_Locdesc_c entry;
    Dwarf_Small source;
    Dwarf_Unsigned exprOffset, descOffset;
    int res = dwarf_get_locdesc_entry_d(head, 0, &lleVal, &lopc, &hipc,
                                        &debugAddrAvail, &lopcCooked,
                                        &hipcCooked, &opsSize, &entry, &source,
                                        &exprOffset, &descOffset, &err);
    if (res != DW_DLV_OK || opsSize != 1) {
        return 1;
    }

    Dwarf_Small op;
    Dwarf_Unsigned opnds[4];
    res = dwarf_get_location_op_value_c(entry, 0, &op, &opnds[0], &opnds[1],
                                        &opnds[2], &opnds[3], &err);

    if (res != DW_DLV_OK) {
        return 1;
    }

    if (op != DW_OP_fbreg) {
        fprintf(stderr, "Error: Expected DW_OP_fbreg");
        return 1;
    }

    *offset = (int)opnds[0];
    return 0;
}

static int getOffsetCU(Dwarf_Die die, Dwarf_Off *offset) {
    Dwarf_Half version;
    Dwarf_Bool isInfo, isDwo;
    Dwarf_Half offsetSize, addrSize, extSize;
    Dwarf_Sig8 *signature;
    Dwarf_Unsigned length;
    Dwarf_Error err;

    int res = dwarf_cu_header_basics(die, &version, &isInfo, &isDwo,
                                     &offsetSize, &addrSize, &extSize,
                                     &signature, offset, &length, &err);

    return res != DW_DLV_OK;
}

static int getAttributeAddr(Dwarf_Attribute attr, void **addr) {
    Dwarf_Addr retAddr;
    Dwarf_Error err;

    if (dwarf_formaddr(attr, &retAddr, &err) != DW_DLV_OK) {
        return 1;
    }

    *addr = (void *)retAddr;
    return 0;
}
