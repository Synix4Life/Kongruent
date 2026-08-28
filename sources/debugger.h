#pragma once

#include <unordered_map>

#include "cfg.h"
#include "construction.h"
#include "def_use.h"
#include "dominator_tree.h"
#include "liveness.h"



/* ----------------------------- UTIL ----------------------------- */

/**
 * Get the opcode String representation
 * @param type Opcode
 */
const char* get_opcode_name(const opcode_type type);



/* --------------------- STORE/ USE DEBUGGER ---------------------- */

/**
 * Prints the store Def-use map of a function
 * @param map The def-use map
 */
void store_use_debugger(const cfg& graph, const std::vector<def_use_map>& map) noexcept;



/* -------------------- PHI FUNCTION DEBUGGER --------------------- */

/**
 * Prints the phi functions
 * @param phis The phi functions
 */
void debug_phi(const std::vector<phi>& phis) noexcept;



/* ---------------------- DOMINATOR DEBUGGER ---------------------- */

/**
 * Prints the "cfg_block::id -> cfg_block::dfs_num" map
 * @param graph The control flow graph
 */
void block_id_rpo_map(const cfg& graph) noexcept;


/**
 * Prints the DJ graph
 * @param tree The DJ-Graph
 */
void debug_DJ(const dj_tree& tree) noexcept;


/**
 * Debug the result of the IDF calculation
 * @param N_alpha The initial set of sparse nodes for a specific variable
 * @param IDF The set IDF = DF+(N_alpha)
 */
void debug_IDF_out(const std::uint32_t id, const std::vector<std::uint16_t>& N_alpha, const std::vector<std::uint16_t>& IDF) noexcept;



/* ------------------------- CFG DEBUGGER ------------------------- */

/**
 * Method to print the Control Flow Graphs 
 * @param graphs: Struct cfgs, list of CFGs for each function
 */
void debug_cfgs(const std::vector<cfg>& graphs) noexcept;



/* ---------------------- LIVENESS ANALYSIS ----------------------- */

/**
 * Method to print the Liveness Analysis' result
 * @param graph CFG
 * @param live_info The liveness information
 */
void debug_liveness(const cfg& graph, ::ssa::recompilation::live_sets live_info) noexcept;

/**
 * Method to print the interference graph
 * @param graph CFG
 * @param i_graph Interference graph
 */
void debug_interference_graph(const cfg& graph, ::ssa::recompilation::interference_graph i_graph) noexcept;