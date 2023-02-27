 /**
 * @file addons/rules/rules.h
 * @brief Internal types and functions for rules addon.
 */

#include "../../private_api.h"

#ifdef FLECS_RULES

typedef uint8_t ecs_var_id_t;
typedef int16_t ecs_rule_lbl_t;
typedef ecs_flags64_t ecs_write_flags_t;

#define EcsRuleMaxVarCount      (64)
#define EcsVarNone              ((ecs_var_id_t)-1)
#define EcsThisName             "this"

/* -- Variable types -- */
typedef enum {
    EcsVarEntity,          /* Variable that stores an entity id */
    EcsVarTable,           /* Variable that stores a table */
    EcsVarAny              /* Used when requesting either entity or table var */
} ecs_var_kind_t;

typedef struct ecs_rule_var_t {
    int8_t kind;           /* variable kind (EcsVarEntity or EcsVarTable)*/
    ecs_var_id_t id;       /* variable id */
    ecs_var_id_t table_id; /* id to table variable, if any */
    const char *name;      /* variable name */
#ifdef FLECS_DEBUG
    const char *label;     /* for debugging */
#endif
} ecs_rule_var_t;

/* -- Instruction kinds -- */
typedef enum {
    EcsRuleAnd,            /* And operator: find or match id against table */
    EcsRuleTrav,           /* EcsRuleAnd with support for transitive/reflexive queries */
    EcsRuleIdsRight,       /* Find ids in use that match (R, *) wildcard */
    EcsRuleIdsLeft,        /* Find ids in use that match (*, T) wildcard */
    EcsRuleEach,           /* Iterate entities in table, populate entity variable */
    EcsRuleStore,          /* Store table or entity in variable */
    EcsRuleUnion,          /* Combine output of multiple operations */
    EcsRuleEnd,            /* Used to denote end of EcsRuleUnion block */
    EcsRuleNot,            /* Sets iterator state after term was not matched */
    EcsRuleSetVars,        /* Populate it.sources from variables */
    EcsRuleSetThis,        /* Populate This entity variable */
    EcsRuleContain,        /* Test if table contains entity */
    EcsRulePairEq,         /* Test if both elements of pair are the same */
    EcsRuleSetCond,        /* Set conditional value for EcsRuleJmpCondFalse */
    EcsRuleJmpCondFalse,   /* Jump if condition is false */
    EcsRuleJmpNotSet,      /* Jump if variable(s) is not set */
    EcsRuleYield,          /* Yield result back to application */
    EcsRuleNothing         /* Must be last */
} ecs_rule_op_kind_t;

/* Op flags to indicate if ecs_rule_ref_t is entity or variable */
#define EcsRuleIsEntity  (1 << 0)
#define EcsRuleIsVar     (1 << 1)
#define EcsRuleIsSelf    (1 << 6)

/* Op flags used to shift EcsRuleIsEntity and EcsRuleIsVar */
#define EcsRuleSrc     0
#define EcsRuleFirst   2
#define EcsRuleSecond  4

/* References to variable or entity */
typedef union {
    ecs_var_id_t var;
    ecs_entity_t entity;
} ecs_rule_ref_t;

/* Query instruction */
typedef struct ecs_rule_op_t {
    uint8_t kind;              /* Instruction kind */
    ecs_flags8_t flags;        /* Flags storing whether 1st/2nd are variables */
    int8_t field_index;        /* Query field corresponding with operation */
    ecs_rule_lbl_t prev;       /* Backtracking label (no data) */
    ecs_rule_lbl_t next;       /* Forwarding label. Must come after prev */
    ecs_rule_lbl_t other;      /* Misc register used for control flow */
    ecs_flags16_t match_flags; /* Flags that modify matching behavior */
    ecs_rule_ref_t src;
    ecs_rule_ref_t first;
    ecs_rule_ref_t second;
    ecs_flags64_t written;     /* Bitset with variables written by op */
} ecs_rule_op_t;

 /* And context */
typedef struct {
    ecs_id_record_t *idr;
    ecs_table_cache_iter_t it;
    int32_t remaining;
} ecs_rule_and_ctx_t;

 /* Each context */
typedef struct {
    int32_t row;
} ecs_rule_each_ctx_t;

 /* Setthis context */
typedef struct {
    ecs_table_range_t range;
} ecs_rule_setthis_ctx_t;

/* Cache for storing results of downward traversal */
typedef struct {
    ecs_entity_t entity;
    ecs_id_record_t *idr;
    int32_t column;
} ecs_trav_elem_t;

typedef struct {
    ecs_id_t id;
    ecs_id_record_t *idr;
    ecs_vec_t entities;
    bool up;
} ecs_trav_cache_t;

/* Trav context */
typedef struct {
    ecs_rule_and_ctx_t and;
    ecs_trav_cache_t cache;
    int32_t index;
    int32_t offset;
    int32_t count;
    bool yield_reflexive;
} ecs_rule_trav_ctx_t;

/* Trav context */
typedef struct {
    ecs_id_record_t *cur;
} ecs_rule_ids_ctx_t;

/* End context (used with Union) */
typedef struct {
    ecs_rule_lbl_t lbl;
} ecs_rule_ctrlflow_ctx_t;

/* Condition context */
typedef struct {
    bool cond;
} ecs_rule_cond_ctx_t;

typedef struct ecs_rule_op_ctx_t {
    union {
        ecs_rule_and_ctx_t and;
        ecs_rule_trav_ctx_t trav;
        ecs_rule_ids_ctx_t ids;
        ecs_rule_each_ctx_t each;
        ecs_rule_setthis_ctx_t setthis;
        ecs_rule_ctrlflow_ctx_t ctrlflow;
        ecs_rule_cond_ctx_t cond;
    } is;
} ecs_rule_op_ctx_t;

/* Rule compiler state */
typedef struct {
    ecs_vector_t *ops;
    ecs_write_flags_t written; /* Bitmask to check which variables have been written */
    ecs_write_flags_t cond_written; /* Track conditional writes (optional operators) */

    /* Labels used for control flow */
    ecs_rule_lbl_t lbl_union;
    ecs_rule_lbl_t lbl_not;
    ecs_rule_lbl_t lbl_option;
    ecs_rule_lbl_t lbl_cond_eval;
    ecs_rule_lbl_t lbl_or;
    ecs_rule_lbl_t lbl_none;
    ecs_rule_lbl_t lbl_prev; /* If set, use this as default value for prev */
} ecs_rule_compile_ctx_t;    

/* Rule run state */
typedef struct {
    uint64_t *written;            /* Bitset to check which variables have been written */
    ecs_rule_lbl_t op_index;      /* Currently evaluated operation */
    ecs_rule_lbl_t prev_index;    /* Previously evaluated operation */
    ecs_rule_lbl_t jump;          /* Set by control flow operations to jump to operation */
    ecs_var_t *vars;              /* Variable storage */
    ecs_iter_t *it;               /* Iterator */
    ecs_rule_op_ctx_t *op_ctx;    /* Operation context (stack) */
    ecs_world_t *world;           /* Reference to world */
    const ecs_rule_t *rule;       /* Reference to rule */
    const ecs_rule_var_t *rule_vars; /* Reference to rule variable array */
} ecs_rule_run_ctx_t;

typedef struct {
    ecs_rule_var_t var;
    const char *name;
} ecs_rule_var_cache_t;

struct ecs_rule_t {
    ecs_header_t hdr;             /* Poly header */
    ecs_filter_t filter;          /* Filter */

    /* Variables */
    ecs_rule_var_t *vars;         /* Variables */
    int32_t var_count;            /* Number of variables */
    int32_t var_pub_count;        /* Number of public variables */
    bool has_table_this;          /* Does rule have [$this] */
    ecs_hashmap_t tvar_index;     /* Name index for table variables */
    ecs_hashmap_t evar_index;     /* Name index for entity variables */
    ecs_rule_var_cache_t vars_cache; /* For trivial rules with only This variables */
    char **var_names;             /* Array with variable names for iterator */
    ecs_var_id_t *src_vars;       /* Array with ids to source variables for fields */

    ecs_rule_op_t *ops;           /* Operations */
    int32_t op_count;             /* Number of operations */

    /* Mixins */
    ecs_iterable_t iterable;
    ecs_poly_dtor_t dtor;

#ifdef FLECS_DEBUG
    int32_t var_size;             /* Used for out of bounds check during compilation */
#endif
};

/* Convert integer to label */
ecs_rule_lbl_t flecs_itolbl(
    int64_t val);

/* Get ref flags (IsEntity) or IsVar) for ref (Src, First, Second) */
ecs_flags16_t flecs_rule_ref_flags(
    ecs_flags16_t flags,
    ecs_flags16_t kind);

/* Check if variable is written */
bool flecs_rule_is_written(
    ecs_var_id_t var_id,
    uint64_t written);

/* Check if ref is written (calls flecs_rule_is_written)*/
bool flecs_ref_is_written(
    const ecs_rule_op_t *op,
    const ecs_rule_ref_t *ref,
    ecs_flags16_t kind,
    uint64_t written);

/* Compile filter to list of operations */
void flecs_rule_compile(
    ecs_world_t *world,
    ecs_stage_t *stage,
    ecs_rule_t *rule);

/* Get allocator from iterator */
ecs_allocator_t* flecs_rule_get_allocator(
    const ecs_iter_t *it);

/* Find all entities when traversing downwards */
void flecs_rule_get_down_cache(
    const ecs_rule_run_ctx_t *ctx,
    ecs_trav_cache_t *cache,
    ecs_entity_t trav,
    ecs_entity_t entity);

/* Find all entities when traversing upwards */
void flecs_rule_get_up_cache(
    const ecs_rule_run_ctx_t *ctx,
    ecs_trav_cache_t *cache,
    ecs_entity_t trav,
    ecs_table_t *table);

/* Free traversal cache */
void flecs_rule_trav_cache_fini(
    ecs_allocator_t *a,
    ecs_trav_cache_t *cache);

#endif