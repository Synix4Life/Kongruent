#pragma once

#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "cfg.h"


namespace ssa {
namespace recompilation {


/** ---------------------------------------------------------------
 *  ---------------------- LIVENESS ANALYSIS ----------------------
 *  --------------------------------------------------------------- */

/**
 * Holds the liveness information
 * 
 * The data structure is used as following: 
 * - One live_sets-instance represents the liveness information for one whole function
 * - The LiveIn and LiveOut sets hold the live variables indexed by the bb
 * → map < bb-id , live sets for the bb >
 */
struct live_sets {
    const function_id fun_idx;

    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> LiveIn;
    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> LiveOut;

    live_sets(function_id fun_idx) : fun_idx(fun_idx) {}
    live_sets(
        function_id fun_idx,
        std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> LiveIn, 
        std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> LiveOut
    ) : fun_idx(fun_idx), LiveIn(LiveIn), LiveOut(LiveOut) {}
};


/**
 * Compute the liveness information of a graph
 * 
 * @param f The control flow graph
 * @return A struct live_sets, containing LiveIn and LiveOut information for each BB in f
 */
live_sets liveness_analysis(cfg& f);



/** ---------------------------------------------------------------
 *  ------------------- INTERFERENCE GRAPH UTIL -------------------
 *  --------------------------------------------------------------- */

using interference_graph = std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>>;


/**
 * Exposed helper to link a variable to a set
 * 
 * @param i_graph Interference graph
 * @param current_live The variable to link to
 * @param var Variable
 */
void link_var(interference_graph& i_graph, std::unordered_set<std::uint64_t>& current_live, std::uint64_t var);



/** ---------------------------------------------------------------
 *  --------------------- INTERFERENCE GRAPH ----------------------
 *  --------------------------------------------------------------- */

/**
 * Build the interference graph
 * 
 * @param f The control flow graph
 * @param live The LiveIn and LiveOut sets
 * @return The interference graph
 */
interference_graph build_interference_graph(cfg& f, live_sets& live);



} // namespace ssa
} // namespace recompilation
