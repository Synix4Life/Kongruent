#pragma once

#include "cfg.h"
#include "dominator_tree.h"


/* ----------------------------- UTIL ----------------------------- */

/**
 * Get the opcode String representation
 * @param type Opcode
 */
const char* get_opcode_name(const opcode_type type);



/* ---------------------- DOMINATOR DEBUGGER ---------------------- */

/**
 * Prints the "cfg_block::id -> cfg_block::dfs_num" map
 * @param graph The control flow graph
 */
void block_id_rpo_map(cfg& graph);


/**
 * Prints the DJ graph
 * @param tree The DJ-Graph
 */
void debug_DJ(dj_tree tree);


/**
 * Debug the result of the IDF calculation
 * @param N_alpha The initial set of sparse nodes for a specific variable
 * @param IDF The set IDF = DF+(N_alpha)
 */
void debug_IDF_out(std::vector<std::uint16_t>& N_alpha, std::vector<std::uint16_t>& IDF);



/* ------------------------- CFG DEBUGGER ------------------------- */

/**
 * Method to print the Control Flow Graphs 
 * @param graphs: Struct cfgs, list of CFGs for each function
 */
void debug_cfgs(const std::vector<cfg>& graphs);
