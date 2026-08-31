#include "dominator_tree.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "cfg.h"
#include "global.h"
#include "log.h"
#include "errors.h"


/** ---------------------------------------------------------------
 *  -------------------------- DJ GRAPH ---------------------------
 *  --------------------------------------------------------------- */

void reset_attributes(dj_tree& tree) noexcept{
    for(auto& block: tree.blocks){
        block.alpha = false;
        block.in_phi = false;
        block.visited = false;
    }
}


[[nodiscard]] dj_tree build_dj(cfg& graph, std::vector<std::uint16_t>& IDOMs){
    /* --------------- INIT --------------- */

    const int N = graph.blocks.size();

    if(N == 0){
        debug_context context = KONG_INIT_ZERO;
        error(context, "[ ERROR ] DJ Graph for CFG(%s) couldn't be build, since no Basic Blocks exist in it", graph.name.c_str());
    }
    
    dj_tree tree;
    tree.name = graph.name;
    tree.max_level = 0;
    int offset = graph.entry->id;

    tree.blocks.resize(N, dominator_block());


    /* ---------- BASIC FILLING ----------- */

    for (auto& b_ptr : graph.blocks) {
        int rel_idx = b_ptr->id - offset;
        tree.blocks[rel_idx] = dominator_block(b_ptr.get());
    }

    int entry_idx = graph.entry->id - offset;
    tree.blocks[entry_idx].level = 0;


    /* ----------- LINK & LEVEL ----------- */

    bool changed = true;
    while(changed){
        changed = false;
        for(int i = 0; i < N; i++){
            if(i == entry_idx){ continue; }
            if(tree.blocks[i].level == 0xFF){
                if(tree.blocks[IDOMs[i] - offset].level == 0xFF){ continue; }
                changed = true;
                tree.blocks[i].level = tree.blocks[IDOMs[i] - offset].level + 1;
                tree.blocks[IDOMs[i] - offset].edges.push_back(dj_edge(D_EDGE, i + offset));
                tree.blocks[i].dom_parent = tree.blocks[IDOMs[i] - offset].ref_id;
            }
        }
    }


    /* ------------- J-EDGES -------------- */

    for(int i = 0; i < N; i++){
        auto& block = tree.blocks[i];
        for(cfg_block* p: block.ref->pred){
            if(p->id != block.dom_parent){
                tree.blocks[p->id - offset].edges.push_back(dj_edge(J_EDGE, block.ref_id));
            }
        }
        if(block.level > tree.max_level){
            tree.max_level = block.level;
        }
    }

    /* ------------- FINALIZE ------------- */

    return tree;
}





/** ---------------------------------------------------------------
 *  ---------------------- SREEDHAR AND GAO -----------------------
 *  --------------------------------------------------------------- */

/**
 * Verify the correct leveling of the PiggyBank
 * 
 * @param bank The PiggyBank to verify
 * 
 * @throws [Internal Error]: Runtime Exception -> If there was a construction error
 */
void verify_leveling(const PiggyBank& bank){
    for(int i = 0; i < bank.size(); i++){
        if(bank[i].lvl != i){
            debug_context context = KONG_INIT_ZERO;
            error(context, "[ INTERNAL ERROR ] PiggyBank Levels incorrectly initialized");
        }
    }
}


/**
 * A next node getter algorithm, based on the GetNode()-function from "Sreedhar and Gao"
 * 
 * @param bank The PiggyBank
 * @param current_level The current level in the tree
 * 
 * @return Index of the next node to look at
 */
[[nodiscard]] std::uint16_t get_next(PiggyBank& bank, std::uint8_t& current_level){
    while (true){
        auto& current_nodes = bank[current_level].nodes;

        if(!current_nodes.empty()){
            std::uint16_t res = current_nodes.front();
            current_nodes.pop_front();
            return res;
        }

        if(current_level == 0) break; 
        current_level --;
    }

    return 0xFFFF;
}


/**
 * Insert a node into the PiggyBank, based on the InsertNode()-function from "Sreedhar and Gao"
 * 
 * @param bank The PiggyBank
 * @param tree The DJ-graph
 * @param ix The index ix (offset- cleaned) -> I.e. if we have node at index n and , pass ix = n - offset
 * @param offset The offset
 */
void insert_node(PiggyBank& bank, dj_tree& tree, const std::uint16_t ix, const int offset){
    bank[tree.blocks[ix].level].nodes.push_back(ix + offset);
}


/**
 * Visit a node and process it, based on the Visit()-function from "Sreedhar and Gao"
 * 
 * @param IDF The IDF-set
 * @param graph The CFG
 * @param tree The DJ-graph
 * @param bank The PiggyBank
 * @param offset The offset
 * @param node The current dominator_block looking at
 * @param current_level The current level
 */
void visit(std::vector<std::uint16_t>& IDF, cfg& graph, dj_tree& tree, PiggyBank& bank, const int offset, dominator_block& node, std::uint8_t current_level){
    for(auto& edge: node.edges){
        if(edge.dest_id - offset >= tree.blocks.size()){
            kong_log(LOG_LEVEL_ERROR, "[ INTERNAL ERROR ] CRITICAL - Offset calculations produced unsigend integer underflow");
            exit(1);
        }
        auto& to_block = tree.blocks[edge.dest_id - offset];
        if(edge.type == J_EDGE){
            if(to_block.level <= node.level){
                if(! to_block.in_phi){
                    to_block.in_phi = true;
                    IDF.push_back(to_block.ref_id);
                    if(! to_block.alpha){
                        insert_node(bank, tree, to_block.ref_id - offset, offset);
                    }
                }
            }
        } 
        else{ // D-Edge
            if(! to_block.visited){
                to_block.visited = true;
                visit(IDF, graph, tree, bank, offset, to_block, current_level);
            }
        }
    }
}


[[nodiscard]] std::vector<std::uint16_t> calculate_phi_placement(cfg& graph, dj_tree& tree, std::vector<std::uint16_t> N_alpha){
    /* --------------- INIT --------------- */
    
    std::vector<std::uint16_t> IDF;

    reset_attributes(tree);
    std::uint8_t current_level = tree.max_level;

    const int offset = graph.entry->id;

    PiggyBank bank;
    for(int i = 0; i <= current_level; i++){
        bank.push_back(PiggyLevel(i));
    }

    verify_leveling(bank);


    /* --------- ALGORITHM "MAIN" --------- */

    for(std::uint16_t n: N_alpha){
        tree.blocks[n - offset].alpha = true;
        insert_node(bank, tree, n - offset, offset);
    }

    while(true){
        std::uint16_t n_idx = get_next(bank, current_level);
        if(n_idx == 0xFFFF){ break; }
        
        auto& node = tree.blocks[n_idx - offset];
        node.visited = true;

        visit(IDF, graph, tree, bank, offset, node, current_level);
    }


    /* ------------- FINALIZE ------------- */

    std::sort(IDF.begin(), IDF.end());
    return IDF;
}





/** ---------------------------------------------------------------
 *  ------------------------ COOPER ET AL. ------------------------
 *  --------------------------------------------------------------- */

 /**
 * Finds the nearest common dominator from two nodes in the post-dominator tree.
 *
 * This is commonly referred to as the "two-finger" intersection algorithm
 * 
 * @param finger1 The DFS index (postorder) of the first block
 * @param finger2 The DFS index (postorder) of the second block
 * @param idoms The current immediate dominators
 * 
 * @return The DFS index of the common dominator
 * 
 * @note REFERENCE: Cooper, K. D., Harvey, T. J., & Kennedy, K. (2001). A simple, fast dominance algorithm. Software Practice & Experience, 4(1-10), 1-8.
 */
int intersect_idom(int finger1, int finger2, const std::vector<std::uint16_t>& idoms) {
    while (finger1 != finger2) {
        while (finger1 < finger2) finger1 = idoms[finger1];
        while (finger2 < finger1) finger2 = idoms[finger2];
    }
    return finger1;
}


[[nodiscard]] std::vector<std::uint16_t> build_idoms(cfg& graph) {
    if (!graph.dfs_run) dfs(graph);

    const int N = graph.blocks.size();
    kong_log(LOG_LEVEL_INFO, "N : %d", N);
    
    std::vector<std::uint16_t> idoms(N, 0xFFFF);

    // The entry node always has the highest PO ID, here at N-1
    idoms[N-1] = N-1;

    bool changed = true;
    while (changed) {
        changed = false;

        for (int i = N - 2; i >= 0; i--) {
            cfg_block* b = graph.PO[i];
            int b_idx = i;

            int new_idom = -1; 

            /* 
            * Originally, "new_idom ← first (processed) predecessor of b (pick one)"
            * For simplification, -1 as placeholder and assigned during pred-traversal
            */
           for (auto* pred_block : b->pred) {
                int p_idx = pred_block->dfs_num;

                if (idoms[p_idx] != 0xFFFF) {
                    if (new_idom == -1) {
                        new_idom = p_idx;
                    } else {
                        new_idom = intersect_idom(p_idx, new_idom, idoms);
                    }
                }
            }

            if (new_idom != -1 && idoms[b_idx] != new_idom) {
                idoms[b_idx] = new_idom;
                changed = true;
            }
        }
    }
    return idoms; 
}


std::vector<std::uint16_t> normalize_IDOM(cfg& graph, std::vector<std::uint16_t>& IDOMs){
    const int offset = graph.entry->id;
    const int N = graph.blocks.size();

    // This will hold IDOMs indexed by the original block->id
    std::vector<std::uint16_t> idoms_by_id(N);

    for (int i = 0; i < N; i++) {
        cfg_block* b = graph.blocks[i].get();
        
        // Find b's IDOM in the DFS domain
        // b->dfs_num is the index used in the build_idoms result
        std::uint16_t idom_dfs_idx = IDOMs[b->dfs_num];

        // Map DFS index back to the real block ID
        // The block whose dfs_num is idom_dfs_idx is graph.PO[idom_dfs_idx]
        std::uint16_t idom_real_id = graph.PO[idom_dfs_idx]->id;

        // Store it in normalized vector at the block's own ID index
        idoms_by_id[b->id - offset] = idom_real_id;
    }

    return idoms_by_id;
}


/**
 * Recursive DFS search
 * 
 * @param block Current cfg_block
 * @param rpo The graphs rpo list as reference
 * @param postorder_id Current Postorder index ID to correctly assign the values
 * 
 * @return New postorder_id
 */
std::uint16_t dfs_recursive(cfg_block& block, std::vector<cfg_block*>& rpo , std::uint16_t postorder_id){
    // Check if it has already been visited before
    if (block.dfs_num != 0xFFFF) return postorder_id;

    block.dfs_num = 0xFFFE; // Prevent looping

    for (auto* succ : block.succ) {
        postorder_id = dfs_recursive(*succ, rpo, postorder_id);
    }

    block.dfs_num = postorder_id;
    rpo.push_back(&block);

    return postorder_id + 1;
}


void dfs(cfg& graph){ 
    if(graph.dfs_run) return;

    graph.dfs_run = true; 
    
    dfs_recursive(*graph.entry, graph.PO, 0); 
}
