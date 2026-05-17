#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "compiler.h"
#include "functions.h"


/**
 * Identifier for block-separation
 */
enum Identifier {
    NONE,
    IF,
    WHILE
};


/**
 * Block of straight-line instructions
 */
struct cfg_block{
    const std::uint16_t id;

    std::vector<opcode*> instructions;

    std::uint16_t edge_count = 0;

    std::vector<cfg_block*> succ;
    std::vector<cfg_block*> pred;

    std::uint16_t dfs_num = 0xFFFF;
    size_t upper_offset_idx;

    cfg_block(std::uint16_t id) : id(id) {}
};


/**
 * Structure that represents the Control Flow Graph (CFG) from one function
 * 
 * @note By design, cfg::PO is in Postorder (PO) after the DFS. Although the algorithm used to calculate IDOM 
 * uses the Reverse Postorder (RPO), this implementation has decided to let "PO" be in Postorder.
 * This is because it makes assignment more trivial. The algorithm itself (dominator_tree::build_idoms)
 * then swaps "PO" to be in RPO
 */
struct cfg{
    std::vector<std::unique_ptr<cfg_block>> blocks;

    /** @note: By design, entry has the lowest ID in the CFG, although it can be anywhere in BLOCKS */
    cfg_block* entry;

    std::vector<cfg_block*> PO;

    std::uint8_t num_edges = 0;

    std::string name;
    function_id fun_idx;

    bool dfs_run = false;
};


/**
 * Method to create the Control Flow Graphs (CFGs) for all functions in memory separately
 * @return struct cfgs: Structure holding a CFG for each function
 * 
 * Time complexity: O(S) with S = Number of statements
 */
[[nodiscard]] std::vector<cfg> make_cfgs();
