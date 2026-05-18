#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cfg.h"
#include "construction.h"
#include "debugger.h"
#include "def_use.h"
#include "dominator_tree.h"
#include "functions.h"
#include "log.h"
#include "errors.h"


/**
 * @brief CREATE STATIC SINGLE ASSIGNMENT (SSA) FORM
 * 
 * This function unifies all the necessary methods and calculations to build the Static Single Assignment (SSA) form correctly
 * 
 * It consists of multiple different well-established, industry-standard algorithms, plus a self-developed prototypic framework algorithm
 * 
 * Each algorithm is referred to in the used sections specifically, but they are also listed below.
 * 
 * The prototypic framework algorithm refers to the actual phi-insertion algorithm (NOT the phi-placement calculations, 
 *      i.e. where to place the function).
 * That algorithm creates the phi-functions, places the phi-functions into the bytecode and renumbers the bytecodes internal variables to fit the SSA guidelines
 * 
 * @param sequence_id The ID, where $ val > ID $ is NOT yet used in the bytecode (i.e. available indices)
 * 
 * @note REFERENCES used in sub-methods of the SSA-creation
 * 
 * @note Sreedhar, V. C., & Gao, G. R. (1995). A linear time algorithm for placing ϕ-nodes. Proceedings of the 22nd ACM SIGPLAN-SIGACT symposium on Principles
 * @note Cooper, K. D., Harvey, T. J., & Kennedy, K. (2001). A simple, fast dominance algorithm. Software Practice & Experience, 4(1-10), 1-8.
 * 
 * No data is returned from this function, since every data created becomes unusable after this function executes
 * 
 * -> CFG's (and reference-wise DJ-tree) are misaligned if any arguments in the function change
 * 
 * -> Def-use-maps are only constructed for the non-SSA variables
 * 
 * -> The phi-function creation carries no further data that may be relevant later
 */
inline void static_single_assignment_form(uint64_t sequence_id){
    
    /* ------------------------ CFG ------------------------ */

    std::vector<cfg> graphs = make_cfgs();

#ifndef NDEBUG
		debug_cfgs(graphs);
#endif


    /* -------------------- DOMINATION --------------------- */

    std::vector<dj_tree> DJ_TREES;

    for(auto& f: graphs){
	    dfs(f);
		
        auto res = build_idoms(f);

#ifndef NDEBUG
	    block_id_rpo_map(f);
        kong_log(LOG_LEVEL_INFO, "\nIdom Building Result: ");
		for(int i = 0 ; i<res.size(); i++){
			kong_log(LOG_LEVEL_INFO, "idom(%d) = %d", i, res[i]);
		}
#endif

		res = normalize_IDOM(f, res);
		int offset = f.entry->id;

#ifndef NDEBUG
		kong_log(LOG_LEVEL_INFO, "\nNormalized: ");
		for(int i = 0 ; i<res.size(); i++){
			kong_log(LOG_LEVEL_INFO, "idom(%d) = %d", i + offset, res[i]);
		}
#endif

		dj_tree tree = build_dj(f, res);
		
#ifndef NDEBUG
        debug_DJ(tree);
#endif

		DJ_TREES.push_back(tree);
	}


    for(int i = 0; i < graphs.size(); ++i){
        auto& f = graphs[i];


        /* ---------------- VARIABLE DISCOVERY ----------------- */

        auto def = discover_store(f);
        auto def_use = discover_def_use(f, sequence_id);

#ifndef NDEBUG
        store_use_debugger(f, def_use);
#endif


        /* ------------------ IDF CALCULATION ------------------ */

        std::unordered_map<std::uint32_t, std::vector<uint16_t>> IDFs;
    	std::vector<std::uint16_t> N_alpha;

        for(const auto& [id, data] : def){
            for(auto element: data){
                N_alpha.push_back(element);
            }

            auto IDF = calculate_phi_placement(f, DJ_TREES[i], N_alpha);
            IDFs[id] = IDF;

#ifndef NDEBUG
            debug_IDF_out(id, N_alpha, IDF);
#endif

            N_alpha.clear();
        }


        /* ---------------- CREATE PHI FUNCTION ---------------- */

        auto phi = create_phi(f, DJ_TREES[i], def_use, IDFs, sequence_id);

#ifndef NDEBUG
        debug_phi(phi);
	
    	kong_log(LOG_LEVEL_INFO, "\nPost-rewriting Store-Use-Map Debugger\n");
	    store_use_debugger(f, def_use);
#endif


        /* ------------- CONSTRUCT SSA BYTESTREAM -------------- */

        construct_SSA(f, def_use, phi);

#ifndef NDEBUG
    	kong_log(LOG_LEVEL_INFO, "\n");
#endif
    }
}
