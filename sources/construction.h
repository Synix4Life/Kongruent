#pragma once

#include <cstdint>
#include <vector>

#include "def_use.h"

/* ================================================================== */
/* ================= PHI CONSTRUCTION + USE REWRITE ================= */
/* ================================================================== */

/**
 * Structure that stores a phi function
 * 
 * @note Stores the following data:
 * @note -> bb_id = Basic block where the phi needs to go
 * @note -> id = The phi-functions target id
 * @note -> origin = The origin value
 * @note -> elems = Function parameters
 */
struct phi{
    std::uint16_t bb_id;
    std::uint64_t id;
    std::uint32_t origin;
    std::vector<std::uint64_t> elems;
    
    phi(std::uint16_t bb_id, std::uint64_t id, std::uint32_t origin)
    : bb_id(bb_id), 
      id(id),
      origin(origin),
      elems()
    {}
};


/**
 * @brief REFINE THE DEF-USE-MAP & CREATE THE PHI-FUNCTIONS
 * 
 * This method uses the calculations to perform two actions:
 * 
 *    1. Create the phi-functions
 * 
 *    2. Refine the variable usages
 * 
 * Creating the phi-functions refers to the construction of a phi-function struct array that keeps the necessary informations 
 *    for the actual opcode creation and the placement
 * 
 * Refining the variable usages refers to rewriting the usages of the non-SSA variables with the newly assigned indexes from 
 *    the phi-functions and the variable renaming
 * 
 * @param graph The CFG
 * @param dj The DJ-tree
 * @param map The def-use-map
 * @param Is The list of IDFs for each variable v ∉ SSA-form
 * @param var_id Global next available number (ascending numbers also available)
 * 
 * @return The list of constructed phi function
 * 
 * @note Also heavily modifies the def-use-map in memory
 */
std::vector<phi> create_phi(cfg& graph, dj_tree& dj, std::vector<def_use_map>& map, std::unordered_map<std::uint32_t, std::vector<uint16_t>>& Is, std::uint64_t& var_id);



/* ================================================================== */
/* ======================== SSA CONSTRUCTION ======================== */
/* ================================================================== */

/**
 * @brief CONSTRUCT THE SSA FORM FOR ONE FUNCTION
 * 
 * This method uses the created phi's and the refined def-use-map to build the actual form
 * 
 * It creates a copy if the bytecode and replaces the old one afterwards.
 * 
 * @param graph The CFG
 * @param map The refined def-use-map
 * @param phis The list of the phi functions that need to be inserted
 */
void construct_SSA(cfg& graph, std::vector<def_use_map>& map, std::vector<phi>& phis);
