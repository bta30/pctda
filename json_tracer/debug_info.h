#ifndef DEBUG_INFO_H
#define DEBUG_INFO_H

typedef struct {
    char *name, *path;
    unsigned size;
} type_t;

typedef struct {
    char *varName;
    type_t type;
    int isLocal;
} variable_info_t;

typedef struct {
    variable_info_t varInfo;
    int offset;
} local_variable_t;

typedef struct {
    variable_info_t varInfo;
    char *path;
    void *addr;
} static_variable_t;

typedef struct {
    char *name, *path;
    void *lowPC;
    unsigned length;
    local_variable_t *vars;
    int sizeVars, capacityVars;
} function_info_t;

typedef struct {
    function_info_t *funcs;
    int sizeFuncs, capacityFuncs;
    static_variable_t *vars;
    int sizeVars, capacityVars;
    type_t *types;
    int sizeTypes, capacityTypes;
} debug_info_t;

/*
 * Gets all relevant debugging information from the given file,
 * returning NULL on error
 */
debug_info_t *getDebugInfo(const char *file);

/*
 * Destroys generated debugging information
 */
void destroyDebugInfo(debug_info_t *info);

/*
 * Gets the information of a variable at a certain memory address, given the
 * current PC, segment base and the stack pointer prior to the function call
 *
 * Note: Do not free any of the strings passed back
 */
variable_info_t getVariableInfo(debug_info_t *info, void *varAddr,
                                void *pc, void *segmBase, void *sp);

#endif
