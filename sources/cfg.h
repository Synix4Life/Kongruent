#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <list>
#include <memory>

#include "compiler.h"

enum Identifier {
    NONE,
    IF,
    WHILE
};

/**
 * Block of straight-line instructions
 */
struct cfg_block{
    std::uint16_t id;

    std::vector<opcode*> instructions;

    std::uint16_t edge_count;

    std::vector<cfg_block*> succ;
    std::vector<cfg_block*> pred;
};

/**
 * Structure that represents the Control Flow Graph (CFG) from one function
 */
struct cfg{
    std::vector<std::unique_ptr<cfg_block>> blocks;

    std::uint8_t num_edges;

    std::string name;
};

/**
 * Structure that holds CFGS for multiple functions
 */
struct cfgs{
    std::vector<cfg> function_graphs;
};

/**
 * Method to create the Control Flow Graphs (CFGs) for all functions in memory separately
 * @return struct cfgs: Structure holding a CFG for each function
 */
cfgs make_cfgs();

/**
 * Method to print the Control Flow Graphs (Debug only)
 * @param graphs: Struct cfgs, list of CFGs for each function
 */
void debug_cfgs(const cfgs& graphs);
