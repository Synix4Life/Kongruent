#pragma once

#include <cstdint>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "cfg.h"
#include "debugger.h"
#include "liveness.h"


namespace ssa {
namespace recompilation {



/** ---------------------------------------------------------------
 *  ------------------------ SREEDHAR UTIL ------------------------
 *  --------------------------------------------------------------- */

using congruence_set = std::unordered_set<std::uint64_t>;
using congruence_class = std::unordered_map<std::uint64_t, congruence_set>;

using unresolved_neighbor_map = std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>>;

/**
 * Defines the variable type, i.e. being either a source or target in a phi instruction
 * 
 * @note Example : x_0 = phi(x_1, x_2, ...);
 * 
 * - TARGET: x_0
 * 
 * - SOURCE: x_1, x_2, ...
 */
enum Type{
    SOURCE,
    TARGET
};

/**
 * Structure to track the candidate resources for the candidate resource set
 */
struct candidate_resource {
    std::uint64_t target;
    std::uint64_t new_target = -1; // Used for deferring the actual copies
    std::uint16_t bb_for_copy;
    Type type;

    bool operator<(const candidate_resource& other) const {
        if (target != other.target)
            return target < other.target;
        if (bb_for_copy != other.bb_for_copy)
            return bb_for_copy < other.bb_for_copy;
        if (type != other.type)
            return type < other.type;
        return new_target < other.new_target;
    }

    bool operator==(const candidate_resource& other) const noexcept {
        return target == other.target
            && type == other.type
            && bb_for_copy == other.bb_for_copy;
    }

    candidate_resource(std::uint64_t target, Type type, std::uint16_t bb_for_copy) : 
        target(target), type(type), bb_for_copy(bb_for_copy) {}
    candidate_resource(std::uint64_t target, std::uint64_t new_target, Type type, std::uint16_t bb_for_copy) : 
        target(target), new_target(new_target), type(type), bb_for_copy(bb_for_copy) {}
};

/**
 * Hash-representation for the candidate_resource struct
 */
struct candidate_resource_hash {
    std::size_t operator()(const candidate_resource& r) const noexcept {
        std::size_t h1 = std::hash<std::uint64_t>{}(r.target);
        std::size_t h2 = std::hash<Type>{}(r.type);
        std::size_t h3 = std::hash<std::uint16_t>{}(r.bb_for_copy);

        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

/**
 * FIFO queue for candidate_resources_set with hash-based duplicate elimination.
 */
struct candidate_resource_set {
    std::queue<candidate_resource> queue;
    std::unordered_set<candidate_resource, candidate_resource_hash> seen;

    /**
     * Add a resource if it is not already present.
     * @param resource The resource to add
     */
    void push(const candidate_resource& resource) {
        if (seen.insert(resource).second) {
            queue.push(resource);
        }
    }

    /**
     * Pop the first added value from the queue (FIFO).
     * Doesn't return the element!
     */
    void pop() {
        queue.pop();
    }

    /**
     * Returns the first added value. Doesn't pop it!
     * @return The FIFO oldest element
     */
    candidate_resource& front() {
        return queue.front();
    }

    /**
     * If the queue is empty
     * @return If the queue is empty
     */
    bool empty() const {
        return queue.empty();
    }
};




/** ---------------------------------------------------------------
 *  ----------------------- SREEDHAR ET AL. -----------------------
 *  --------------------------------------------------------------- */

/**
 * The algorithm presented by Sreedhar et al., which resolves the phi interference in the CFG and 
 * inserts the copies afterwards.
 * 
 * @param sequence_id Variable sequence id
 * @param f The control flow graph
 * @param i_graph The interference graph
 * @param live_vars The LiveIn and LiveOut sets per CFG block
 * @return The phi congruence classes for every variable
 * 
 * @note REFERENCE: Sreedhar, V. C., Ju, R. D.-C., Gillies, D. M., & Santhanam, V. (1999). Translating out of static single assignment form. International Static Analysis Symposium, 194–210.
 */
congruence_class eliminate_phi_resource_interference(
    uint64_t& sequence_id,
    cfg& f, 
    interference_graph& i_graph, 
    live_sets& live_vars
);


/** ---------------------------------------------------------------
 *  ------------------------- COALESCING --------------------------
 *  --------------------------------------------------------------- */

/**
 * Perform a coalescing pass, which removes the phi functions and joins the variables
 * 
 * @param sequence_id Variable sequence id
 * @param cfg CFG
 * @param classes Congruence classes
 */
void clean_phi_functions(uint64_t& sequence_id, cfg& graph, congruence_class& classes);



} //namespace recompilation
} // namespace SSA
