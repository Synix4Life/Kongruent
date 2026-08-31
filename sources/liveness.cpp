#include "liveness.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>


#include "dominator_tree.h"
#include "error.h"
#include "log.h"


namespace ssa {
namespace recompilation {


/** ---------------------------------------------------------------
 *  ------------------- LIVENESS ANALYSIS UTIL --------------------
 *  --------------------------------------------------------------- */

 /**
 * Add src[id] to the target
 * 
 * @param target The target to add the elements onto
 * @param src The source of the elements
 * @param id bb_id 
 * 
 * @note oT → "onto Target"
 */
void union_oT(
    std::unordered_set<std::uint64_t>& target, 
    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>>& src, 
    std::uint64_t id
){
    if(src.count(id)){
        for(std::uint64_t var_id : src[id])
            target.insert(var_id);
    }
}

/**
 * Remove src[id] from the target
 * 
 * @param target The target to remove the elements from
 * @param src The source of the elements
 * @param id bb_id
 * 
 * @note oT → "onto Target"
 */
void complement_oT(
    std::unordered_set<std::uint64_t>& target, 
    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>>& src, 
    std::uint64_t id
){
    if(src.count(id)){
        for(std::uint64_t var_id : src[id])
            target.erase(var_id);
    }
}



/** ---------------------------------------------------------------
 *  ---------------------- LIVENESS ANALYSIS ----------------------
 *  --------------------------------------------------------------- */

live_sets liveness_analysis(cfg& f){

    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> LiveIn;
    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> LiveOut;

    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> GEN;
    std::unordered_map<std::uint16_t, std::unordered_set<std::uint64_t>> KILL;
    

    /* ---------- CONSTRUCT GEN & KILL ---------- */
    
    /**
     * Construction following the idea:
     * 
     * For each instruction (moving from the top of the block to the bottom):
     *      for each var on RHS (Uses) -> If var NOT in (current) KILL -> Add to GEN
     *      for each var on LHS (Defs) -> Add to KILL
     */

    for(auto& bb : f.blocks){
        const std::uint16_t bb_id = bb.get()->id;
        for(auto& instr : bb.get()->instructions){
            switch (instr->type){
                case OPCODE_STORE_VARIABLE:
                case OPCODE_SUB_AND_STORE_VARIABLE:
                case OPCODE_ADD_AND_STORE_VARIABLE:
                case OPCODE_DIVIDE_AND_STORE_VARIABLE:
                case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_store_var.from.index)){
                        GEN[bb_id].insert(instr->op_store_var.from.index);
                    }
                    KILL[bb_id].insert(instr->op_store_var.to.index);
                    break;
                case OPCODE_LOAD_ACCESS_LIST:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_load_access_list.from.index)){
                        GEN[bb_id].insert(instr->op_load_access_list.from.index);
                    }
                    KILL[bb_id].insert(instr->op_load_access_list.to.index);
                    break;
                case OPCODE_STORE_ACCESS_LIST:
                case OPCODE_SUB_AND_STORE_ACCESS_LIST:
                case OPCODE_ADD_AND_STORE_ACCESS_LIST:
                case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
                case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
                    // TODO: Structs etc using Access Lists for now kept in Memory
                    KILL[bb_id].insert(instr->op_store_access_list.to.index);
                    break;
                case OPCODE_VAR:
                    KILL[bb_id].insert(instr->op_var.var.index);
                    break;
                case OPCODE_NOT:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_not.from.index)){
                        GEN[bb_id].insert(instr->op_not.from.index);
                    }
                    KILL[bb_id].insert(instr->op_not.to.index);
                    break;
                case OPCODE_NEGATE:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_negate.from.index)){
                        GEN[bb_id].insert(instr->op_negate.from.index);
                    }
                    KILL[bb_id].insert(instr->op_negate.to.index);
                    break;
                case OPCODE_RETURN:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_return.var.index)){
                        GEN[bb_id].insert(instr->op_return.var.index);
                    }
                    break;
                case OPCODE_CALL:
                    for(int p = 0; p < instr->op_call.parameters_size; ++p){
                        if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_call.parameters[p].index)){
                            GEN[bb_id].insert(instr->op_call.parameters[p].index);
                        }
                    }
                    KILL[bb_id].insert(instr->op_call.var.index);
                    break;
                case OPCODE_MULTIPLY:
                case OPCODE_DIVIDE:
                case OPCODE_MOD:
                case OPCODE_ADD:
                case OPCODE_SUB:
                case OPCODE_EQUALS:
                case OPCODE_NOT_EQUALS:
                case OPCODE_GREATER:
                case OPCODE_GREATER_EQUAL:
                case OPCODE_LESS:
                case OPCODE_LESS_EQUAL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_BITWISE_XOR:
                case OPCODE_BITWISE_AND:
                case OPCODE_BITWISE_OR:
                case OPCODE_LEFT_SHIFT:
                case OPCODE_RIGHT_SHIFT:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_binary.left.index)){
                        GEN[bb_id].insert(instr->op_binary.left.index);
                    }
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_binary.right.index)){
                        GEN[bb_id].insert(instr->op_binary.right.index);
                    }
                    KILL[bb_id].insert(instr->op_binary.result.index);
                    break;
                case OPCODE_LOAD_FLOAT_CONSTANT:
                    KILL[bb_id].insert(instr->op_load_float_constant.to.index);
                    break;
                case OPCODE_LOAD_INT_CONSTANT:
                    KILL[bb_id].insert(instr->op_load_int_constant.to.index);
                    break;
                case OPCODE_LOAD_BOOL_CONSTANT:
                    KILL[bb_id].insert(instr->op_load_bool_constant.to.index);
                    break;
                case OPCODE_IF:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_if.condition.index)){
                        GEN[bb_id].insert(instr->op_if.condition.index);
                    }
                    break;
                case OPCODE_WHILE_CONDITION:
                    if(!KILL.count(bb_id) || !KILL[bb_id].count(instr->op_while.condition.index)){
                        GEN[bb_id].insert(instr->op_while.condition.index);
                    }
                    break;
                case OPCODE_PHI:
                    // Phi is treated Live upon block entry
                    // Therefore, the target is added to the live variables and the args are not considered live in the block
                    GEN[bb_id].insert(instr->op_phi.to);
                    break;
                default:
                    // Not specifically treated: 
                    //   DISCARD → NiL , BLOCK_* → Just indices , ASSIGN → Shouldn't exist
                    break;
            }
        }
    }


    /* ----------- COMPUTE LIVE INFO ------------ */

    dfs(f);

    bool changed = false;

    // PHIs are added in this loop as conditional entries
    do{
        changed = false;

        for(auto& block : f.PO){
            auto bb_id = block->id;
            std::unordered_set<std::uint64_t> working_set;

            // ---------- LiveOut ----------
            int i = 0;
            for(auto& succ : block->succ){ 
                union_oT(working_set, LiveIn, succ->id); 

                // Handle Phi
                // Instead of adding all, add only the one for the specific block
                for(auto& instr : succ->instructions){
                    if(instr->type != OPCODE_PHI){ break; }
                    working_set.insert(instr->op_phi.preds[i]);
                }
                ++i;
            }
            LiveOut[bb_id] = working_set;

            // ---------- LiveIn ---------- 
            // LiveIn = GEN + (LiveOut - KILL)
            complement_oT(working_set, KILL, bb_id);
            union_oT(working_set, GEN, bb_id);

            // ---------- Track changes ----------
            if(working_set != LiveIn[bb_id]){
                changed = true;
                LiveIn[bb_id] = std::move(working_set);
            }
        }
    } while(changed);


    /* ----------- FINALIZE ------------ */
    // Variables given as arguments are added into LiveIn[entry] → These are removed here
    LiveIn[f.entry->id].clear();

    for(auto& block: f.PO){
        if(block->succ.size() == 0){
            if(!LiveOut[block->id].empty()){
                kong_log(LOG_LEVEL_ERROR, "[ INTERNAL ERROR ] Liveness analysis yielded a non-empty exit block -> LiveOut[BB(%d)] != {} (%s)", block->id, f.name.c_str());
                exit(1);
            }
        }
    }

    return live_sets(f.fun_idx, LiveIn, LiveOut);
}



/** ---------------------------------------------------------------
 *  ------------------- INTERFERENCE GRAPH UTIL -------------------
 *  --------------------------------------------------------------- */

/**
 * Create an edge between two variables in the interference graph
 * 
 * @param i_graph Interference graph
 * @param a First variable
 * @param b Second variable
 */
void add_edge(interference_graph& i_graph, std::uint64_t a, std::uint64_t b){
    if (a == b) return;

    i_graph[a].insert(b);
    i_graph[b].insert(a);
}


// Globally exposed helper
void link_var(interference_graph& i_graph, std::unordered_set<std::uint64_t>& current_live, std::uint64_t var){
    for(auto n: current_live){
        add_edge(i_graph, var, n);
    }
}


/**
 * Link a variable to a set and remove that variable from the set (if it exists)
 * 
 * @param i_graph Interference graph
 * @param current_live The currently live set to link to
 * @param var Variable
 */
void link_var_remove(interference_graph& i_graph, std::unordered_set<std::uint64_t>& current_live, std::uint64_t var){
    for(auto n: current_live){
        add_edge(i_graph, var, n);
    }
    current_live.erase(var);
}


/**
 * Link a variable to a set and insert that variable into the set (if it doesn't yet exists)
 * 
 * @param i_graph Interference graph
 * @param current_live The currently live set to link to
 * @param var Variable
 */
void link_var_insert(interference_graph& i_graph, std::unordered_set<std::uint64_t>& current_live, std::uint64_t var){
    for(auto n: current_live){
        add_edge(i_graph, var, n);
    }
    current_live.insert(var);
}



/** ---------------------------------------------------------------
 *  --------------------- INTERFERENCE GRAPH ----------------------
 *  --------------------------------------------------------------- */

interference_graph build_interference_graph(cfg& f, live_sets& live){
    interference_graph i_graph;

    if(f.fun_idx != live.fun_idx){
        kong_log(LOG_LEVEL_ERROR, " [ INTERNAL ERROR ] While building interference graph:\nLive Sets and CFG(%s) don't match", f.name.c_str());
        exit(1);
    }

    for(auto& bb : f.blocks){
        std::unordered_set<std::uint64_t> current_live = live.LiveOut[bb.get()->id];

        for(std::size_t i = bb->instructions.size(); i-- > 0; ){
            auto& instr = bb.get()->instructions[i];

            switch (instr->type){
                case OPCODE_STORE_VARIABLE:
                case OPCODE_SUB_AND_STORE_VARIABLE:
                case OPCODE_ADD_AND_STORE_VARIABLE:
                case OPCODE_DIVIDE_AND_STORE_VARIABLE:
                case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                    link_var_insert(i_graph, current_live, instr->op_store_var.from.index);
                    link_var_remove(i_graph, current_live, instr->op_store_var.to.index);
                    break;
                case OPCODE_LOAD_ACCESS_LIST:
                    link_var_insert(i_graph, current_live, instr->op_load_access_list.from.index);
                    link_var_remove(i_graph, current_live, instr->op_load_access_list.to.index);
                    break;
                case OPCODE_STORE_ACCESS_LIST:
                case OPCODE_SUB_AND_STORE_ACCESS_LIST:
                case OPCODE_ADD_AND_STORE_ACCESS_LIST:
                case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
                case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
                    // TODO: structs for now kept in memory and therefore not taken into consideration
                    link_var_remove(i_graph, current_live, instr->op_store_access_list.to.index);
                    break;
                case OPCODE_VAR:
                    link_var_remove(i_graph, current_live, instr->op_var.var.index);
                    break;
                case OPCODE_NOT:
                    link_var_insert(i_graph, current_live, instr->op_not.from.index);
                    link_var_remove(i_graph, current_live, instr->op_not.to.index);
                    break;
                case OPCODE_NEGATE:
                    link_var_insert(i_graph, current_live, instr->op_negate.from.index);
                    link_var_remove(i_graph, current_live, instr->op_negate.to.index);
                    break;
                case OPCODE_RETURN:
                    link_var_insert(i_graph, current_live, instr->op_return.var.index);
                    break;
                case OPCODE_CALL:
                    for(int i=0; i<instr->op_call.parameters_size; ++i){
                        link_var_insert(i_graph, current_live, instr->op_call.parameters[i].index);
                    }
                    link_var_remove(i_graph, current_live, instr->op_call.var.index);
                    break;
                case OPCODE_MULTIPLY:
                case OPCODE_DIVIDE:
                case OPCODE_MOD:
                case OPCODE_ADD:
                case OPCODE_SUB:
                case OPCODE_EQUALS:
                case OPCODE_NOT_EQUALS:
                case OPCODE_GREATER:
                case OPCODE_GREATER_EQUAL:
                case OPCODE_LESS:
                case OPCODE_LESS_EQUAL:
                case OPCODE_AND:
                case OPCODE_OR:
                case OPCODE_BITWISE_XOR:
                case OPCODE_BITWISE_AND:
                case OPCODE_BITWISE_OR:
                case OPCODE_LEFT_SHIFT:
                case OPCODE_RIGHT_SHIFT:
                    link_var_insert(i_graph, current_live, instr->op_binary.left.index);
                    link_var_insert(i_graph, current_live, instr->op_binary.right.index);
                    link_var_remove(i_graph, current_live, instr->op_binary.result.index);
                    break;
                case OPCODE_LOAD_FLOAT_CONSTANT:
                    link_var_remove(i_graph, current_live, instr->op_load_float_constant.to.index);
                    break;
                case OPCODE_LOAD_INT_CONSTANT:
                    link_var_remove(i_graph, current_live, instr->op_load_int_constant.to.index);
                    break;
                case OPCODE_LOAD_BOOL_CONSTANT:
                    link_var_remove(i_graph, current_live, instr->op_load_bool_constant.to.index);
                    break;
                case OPCODE_IF:
                    link_var_insert(i_graph, current_live, instr->op_if.condition.index);
                    break;
                case OPCODE_WHILE_CONDITION:
                    link_var_insert(i_graph, current_live, instr->op_while.condition.index);
                    break;
                case OPCODE_PHI:
                    // PHIs are treated as live upon block entry here
                    // Therefore, the target is added to the live variables and the args are not considered live in the block
                    // Additionally, they don't interfere among each other, as they are not really live at the same time
                    link_var_insert(i_graph, current_live, instr->op_phi.to);
                    break;
                default:
                    // Not specifically treated: 
                    //   DISCARD → NiL , BLOCK_* → Just indices , ASSIGN → Shouldn't exist
                    break;
            }
        }
        
        current_live.clear();
    }

    return i_graph;
}


} // namespace ssa
} // namespace recompilation
