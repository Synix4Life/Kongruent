#pragma once

#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cfg.h"
#include "dominator_tree.h"
#include "functions.h"
#include "compiler.h"
#include "log.h"
#include "errors.h"


/* ================================================================== */
/* ========================= DEF-USE CHAINS ========================= */
/* ================================================================== */


/**
 * Create a def-map for a given CFG
 * 
 * @param graph The control flow graph, where the non-SSA memory variables will be listed from
 * 
 * @return A store-map for each variable not in SSA
 */
[[nodiscard]] std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> discover_store(const cfg& graph);


/**
 * EnumType for the def_use_map
 */
enum MapType{
    STORE,
    ASSIGN,
    USE
};


/**
 * A map able to store definitions and usage for the correct replacement & phi construction
 */
struct def_use_map {
    std::uint64_t id;
    MapType type;
    std::uint16_t bb;
    std::uint64_t replacement_id;
    opcode_type OP;

    def_use_map(std::uint64_t id, MapType type, std::uint16_t bb, std::uint64_t replacement_id, opcode_type OP)
    : id(id), 
      type(type),
      bb(bb),
      replacement_id(replacement_id),
      OP(OP)
    {}
};


/**
 * Create a def-use-map for a given CFG
 * 
 * @param graph The control flow graph, where the non-SSA memory variables will be listed from
 * @param var_id Global next available number (ascending numbers also available)
 * 
 * @return A def-use-map for each variable in non-SSA form
 */
[[nodiscard]] std::vector<def_use_map> discover_def_use(const cfg& graph, std::uint64_t& var_id);
