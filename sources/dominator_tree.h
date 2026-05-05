#pragma once

#include <deque>
#include <vector>

#include "cfg.h"


/** ---------------------------------------------------------------
 *  -------------------------- DJ GRAPH ---------------------------
 *  --------------------------------------------------------------- */

/**
 * Edge type by the definition of "Sreedhar and Gao"
 */
enum EdgeType { D_EDGE, J_EDGE, NIL_TYPE = 0xFF };


/**
 * DJ edge struct, which serves as an edge placeholder
 */
struct dj_edge {
    EdgeType type;
    std::uint16_t dest_id;

    dj_edge() : type(NIL_TYPE), dest_id(0xFFFF) {}
    dj_edge(EdgeType type) : type(type), dest_id(0xFFFF) {}
    dj_edge(std::uint16_t dest) : type(NIL_TYPE), dest_id(dest) {}
    dj_edge(EdgeType type, std::uint16_t dest) : type(type), dest_id(dest) {}
};


/**
 * Dominator Block, serves as a cfg_block wrapper. 
 * Data is NOT copied, but linked.
 */
struct dominator_block {
    // Reference block (with reference id "cfg_block::id")
    cfg_block* ref;
    std::uint16_t ref_id;

    std::uint8_t level;

    std::uint16_t dom_parent;
    
    // Sreedhar's required attributes
    bool visited = false; 
    bool alpha   = false;
    bool in_phi   = false;

    // Outgoing edges in the DJ-Graph
    std::vector<dj_edge> edges;

    // Constructor
    dominator_block()
    : ref(nullptr), 
      ref_id(0xFFFF),
      level(0xFF)
    {}
    
    dominator_block(cfg_block* r, std::uint8_t lvl)
    : ref(r), 
      ref_id(r->id),
      level(lvl),
      dom_parent(0xFF)
    {}

    dominator_block(cfg_block* r)
    : ref(r), 
      ref_id(r->id),
      level(0xFF),
      dom_parent(0xFF)
    {}
};


/**
 * DJ tree 
 */
struct dj_tree{
    std::vector<dominator_block> blocks;
    std::uint8_t max_level = 0;
    
    std::string name;
};


/**
 * Method to construct the DJ_tree. This method constructs the graph, but DOES NOT yet run the algorithm. 
 * The algorithm of Sreedhar and Gao can/ needs to be run on multiple data sets. Therefore, this just produces the tree.
 * 
 * @param graph The cfg for a function where the DJ_graph shall be build on
 * @param IDOMs The immediate dominator set constructed by Cooper et al.
 * 
 * @return The constructed DJ-graph
 * 
 * @throws [Internal Error]: Runtime Exception -> If the graph has no basic blocks
 */
dj_tree build_dj(cfg& graph, std::vector<std::uint16_t>& IDOMs);


/**
 * Method that reset the dominator_block attributes of a given DJ-graph
 * 
 * @param tree The DJ tree
 */
void reset_attributes(dj_tree& tree);



/** ---------------------------------------------------------------
 *  ---------------------- SREEDHAR AND GAO -----------------------
 *  --------------------------------------------------------------- */

/**
 * A data structure representing one level of a PiggyBank
 * @note Using std::deque<> instead of std::vector<> for nodes, since faster node removal time complexity (O(1) instead of O(n))
 */
struct PiggyLevel {
  std::deque<std::uint16_t> nodes;
  std::uint8_t lvl;

  PiggyLevel(std::uint8_t lvl) : lvl(lvl) {}
};


/**
 * PiggyBank data structure - Representing the idea of Sreedhar and Gao with practical changes
 */
struct PiggyBank {
  std::vector<PiggyLevel> bank_levels;
};


/**
 * This function uses the algorithm of Sreedhar and Gao to place the phi-functions. 
 * It makes use of the constructed DJ-graph and the PiggyBank data structure
 * 
 * @param graph The control flow graph for the function
 * @param tree The DJ-graph (here called tree because "graph" is ambiguous) for the function
 * @param N_alpha The initial set of sparse nodes for a specific variable (set must be pre-defined)
 * 
 * @return The set IDF = DF+(N_alpha)
 * 
 * @note REFERENCE: Sreedhar, V. C., & Gao, G. R. (1995). A linear time algorithm for placing ϕ-nodes. Proceedings of the 22nd ACM SIGPLAN-SIGACT symposium on Principles of programming languages - POPL ’95, 62–73. https://doi.org/10.1145/199448.199464
 */
std::vector<std::uint16_t> calculate_phi_placement(cfg& graph, dj_tree& tree, std::vector<std::uint16_t> N_alpha);



/** ---------------------------------------------------------------
 *  ------------------------ COOPER ET AL. ------------------------
 *  --------------------------------------------------------------- */

/**
 * Method to build the immediate dominator after the algorithm proposed by Cooper et al.
 * 
 * @param graph The CFG for one function
 * 
 * @return A Vector of immediate dominators
 * 
 * @note REFERENCE: Cooper, K. D., Harvey, T. J., & Kennedy, K. (2001). A simple, fast dominance algorithm. Software Practice & Experience, 4(1-10), 1-8.
 * @note The return vectors structure is: RET[dfs_num] <= IDOM[dfs_num] . This means that the identification is purely done via the cfg_block internal numbers, NOT their ID's
 */
std::vector<std::uint16_t> build_idoms(cfg& graph);


/**
 * Normalize the IDOMs to fit the cfg_block::id's rather than cfg_block::dfs_num
 * 
 * @param graph The cfg graph
 * @param IDOMs Immediate dominators regarding dfs_num
 * 
 * @return The normalized IDOMs regarding cfg:block::id
 * 
 * @note * IMPORTANT: The return's value'ing is not trivial: Each cfg_block has, by design, a unique ID.
 * It is supposed that ID's are sequential without gaps within a function. 
 * The entry block provides the lowest ID of a function. This results in the following indexing: 
 * The value at vec[n] is the actual cfg_block::id which is the IDOM. The index n is NOT the cfg_block::id, it has been moved by offset "cfg::entry::id"! 
 * This is needed because otherwise, we utilize useless memory.
 * 
 * @note * EXAMPLE: Suppose a CFG with ID's[22, 24]. In the returned vector, we have [IDOM_OF(22), IDOM_OF(23), IDOM_OF(24)]. 
 * As an example, "IDOM_OF(24) → 23". Getting the IDOM via "vec[0]"-call. If one wants to know what IDOM is being worked with, use "i + offset"
 */
std::vector<std::uint16_t> normalize_IDOM(cfg& graph, std::vector<std::uint16_t>& IDOMs);


/**
 * Do a DFS-search and number the cfg_blocks in postorder
 * 
 * @param graph The control flow graph
 */
void dfs(cfg& graph);
