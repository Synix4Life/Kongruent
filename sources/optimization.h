#pragma once

#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "functions.h"


namespace opt {


/**
 * Data type specifiers for the constant_var structure -> Used in constant folding
 */
enum ConstantDataType {
    BOOLEAN,
    FLOAT,
    INTEGER
};


/**
 * Constant folding's constant list element struct
 */
struct constant_var {
    ConstantDataType type;
    union {
        struct {
            bool val;
        } boolean;
        struct {
            int val;
        } integer;
        struct {
            float val;
        } floating;
    };
};



/* ------------------------------------------------------------------ */
/* ----------------------- PHI-OPTIMIZATIONS ------------------------ */
/* ------------------------------------------------------------------ */

/**
 * Remove the trivial phi functions from the function. 
 * 
 * This method removes all phi functions that have either one distinct operand, or one distinct operand besides itself.
 * 
 * @param function_id The function-id where the optimization should occur on
 */
void remove_trivial_phi(function_id id);



/* ------------------------------------------------------------------ */
/* --------------------- FOLDING-OPTIMIZATIONS ---------------------- */
/* ------------------------------------------------------------------ */

/**
 * Discovers all constants and the replacements
 * 
 * @param function_id The function-id
 * 
 * @return A tuple consisting of: 
 * 
 * 1. Replacement-map: A map of replacements (i.e. values that are just assigned, NOT calculated)
 * 
 * 2. Constant-map: A map of all constants and their assignees
 */
std::tuple<
    std::unordered_map<uint64_t, uint64_t>,
    std::unordered_map<uint64_t, constant_var>
> discover(function_id id);


/**
 * CONSTANT FOLDING 
 * 
 * Executes the constant folding optimization pass, where values get pre-computed if they only use constants
 * 
 * @param function_id The function_id
 * @param constants The list of constants
 */
void constant_folding(function_id id, std::unordered_map<uint64_t, constant_var>& constants);



/* ------------------------------------------------------------------ */
/* ------------------- PROPAGATION OPTIMIZATIONS -------------------- */
/* ------------------------------------------------------------------ */

/**
 * COPY PROPAGATION
 * 
 * Executes the copy propagation optimization pass, where a variable is replaced by one other, 
 *      if the variable assigned without changes
 * 
 * @param function_id The function id
 * @param replacement_map Mapping of variables eligible for copy propagation
 */
void copy_propagation(function_id id, std::unordered_map<uint64_t, uint64_t>& replacement_map);



/* ------------------------------------------------------------------ */
/* --------------------- DEAD CODE ELIMINATION ---------------------- */
/* ------------------------------------------------------------------ */

/**
 * LOCAL DEAD CODE ELIMINATION
 * 
 * Perform the local dead code elimination pass, where variables get eliminated if they are
 *      assigned, but never used
 * 
 * @param function_id The function id
 */
void local_dead_code_elimination(function_id id);



/* ================================================================== */
/* ===================== OPTIMIZATION PIPELINE ====================== */
/* ================================================================== */

/**
 * Computes the necessary function ids (i.e. those functions that are not empty)
 * 
 * @return function id array
 */
inline std::vector<function_id> fun_ids(){
    std:: vector<function_id> ids;
    for (function_id i = 0; get_function(i) != NULL; ++i) {
        function *f = get_function(i);
        
        if (f->block == NULL) continue;

        ids.push_back(i);
    }
    return ids;
}


/**
 * OPTIMIZATION PIPELINE
 *
 * Executes a single forward SSA optimization pass over all functions
 *
 * The pipeline applies a fixed sequence of local and SSA-based optimizations
 * exactly once per function in a fixed order.
 *
 * Optimization stages per function:
 * 
 *   1. Trivial ϕ-function elimination (ϕ simplification + trivial ϕ elimination)
 * 
 *   2. Copy propagation
 * 
 *   3. Constant folding
 * 
 *   4. Local dead code elimination
 *
 * @note
 * - Each optimization runs once per function per invocation.
 * 
 * - The pipeline does not repeat until a fixed point.
 */
inline void optimize(){
    /* ---------------------- FUN-IDs ---------------------- */    
    auto ids = fun_ids();

    /* ------------------- OPTIMIZATIONS ------------------- */
    for(function_id i: ids){
        remove_trivial_phi(i);

        auto [replacement, constant] = discover(i);
        copy_propagation(i, replacement);
        constant_folding(i, constant);

        local_dead_code_elimination(i);
    }
}

} // namespace opt
