#include "construction.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "compiler.h"
#include "debugger.h"


/* ================================================================== */
/* ================ PHI FUNCTION BUILD + USE REWRITE ================ */
/* ================================================================== */

/**
 * A safe dominator parent fetch method
 * 
 * @param bb_id Basic block id
 * @param offset CFG numbering offset
 * @param tree DJ-tree
 * 
 * @return ID of the bb's immediate dominator 
 */
std::uint16_t get_dom_parent(
    const std::uint16_t bb_id,
    const int offset,
    dj_tree& tree
){
    if (bb_id < offset) {
        kong_log(LOG_LEVEL_ERROR, "[INTERNAL ERROR] bb_id < offset");
        exit(1);
    }

    auto idx = bb_id - offset;

    if (idx >= tree.blocks.size()) {
        kong_log(LOG_LEVEL_ERROR, "[INTERNAL ERROR] bb_id out of bounds -> idx(%d) greater size(%d) from bb(%d) and offset(%d)", idx, tree.blocks.size(), bb_id, offset);
        exit(1);
    }

    auto parent_id = tree.blocks[idx].dom_parent;

    if(parent_id == bb_id){
        kong_log(LOG_LEVEL_ERROR, "[INTERNAL ERROR] Recursive lookup failed");
        exit(1);
    }

    return parent_id;
}


/**
 * Method to retrieve the a recursively retrieve a phi-function parameter
 * 
 * @param map_ The sub-def-use-map for the specific variable that the phi-function references
 * @param bb_id Basic block id
 * @param offset CFG numbering offset
 * @param tree DJ-tree
 * 
 * @return The phi-function parameter (variable ID)
 */
std::uint64_t get_param(
    std::unordered_map<
        std::uint16_t, 
        std::uint64_t
    >& map_,
    const std::uint16_t bb_id,
    const int offset,
    dj_tree& tree
){
    if(map_.find(bb_id) != map_.end()){
        return map_[bb_id];
    }
    // DT-Property: In general, a definition that goes to the block can only appear in the dom_parent or above
    auto parent_id = get_dom_parent(bb_id, offset, tree);
    return get_param(map_, parent_id, offset, tree);
}


/**
 * Retrieve a phi from the phi-vector
 * 
 * @param phis Phi-vector
 * @param target_bb The phi's basic block
 * @param target_origin The phi's origin value
 * 
 * @throws Runtime Error: Phi function not in list, although one is expected
 */
std::uint64_t get_from_phi(const std::vector<phi>& phis, const std::uint16_t target_bb, const std::uint32_t target_origin){
    for (const auto& p : phis) {
        if (p.bb_id == target_bb && p.origin == target_origin){
            return p.id;
        }
    }

    kong_log(LOG_LEVEL_ERROR, "[INTERNAL ERROR] No Phi in List, although one should exist");
    exit(1);
}


/**
 * Recursive lookup for retrieving the correct new reference variable ID
 * 
 * @param map_ The sub-def-use-map for the specific variable that the phi-function references
 * @param phi_indexing The phi-function reference list
 * @param bb_id Basic block id
 * @param offset CFG numbering offset
 * @param origin The phi's origin value
 * @param phis The phi-functions
 * @param tree DJ-tree
 */
std::uint64_t get_recursive(
    std::unordered_map<
            std::uint16_t, 
            std::uint64_t
        >& map_,
    std::vector<std::uint16_t>& phi_indexing,
    const std::uint16_t bb_id,
    const int offset,
    const std::uint32_t origin,
    std::vector<phi>& phis, 
    dj_tree& tree
){
    auto parent_id = get_dom_parent(bb_id, offset, tree);
    if(map_.find(parent_id) == map_.end()){ // Case 1
        return get_recursive(map_, phi_indexing, parent_id, offset, origin, phis, tree);
    } else{
        if(std::find(phi_indexing.begin(), phi_indexing.end(), parent_id) == phi_indexing.end()) { // Case 4
            return map_[parent_id];
        } else { // Case 2 + 3
            return get_from_phi(phis, parent_id, origin);
        }
    }
}


std::vector<phi> create_phi(cfg& graph, dj_tree& dj, std::vector<def_use_map>& map, std::unordered_map<std::uint32_t, std::vector<uint16_t>>& Is, std::uint64_t& var_id){
    std::unordered_map<
            std::uint32_t,
            std::unordered_map<
                std::uint16_t, 
                std::uint64_t
            >
        > definition_map;
    
    const int offset = graph.entry->id;


    /* -------- BLOCK MAP FILLING --------- */

    for(auto& entry: map){
        if(entry.type == STORE || entry.type == ASSIGN){
            definition_map[entry.id][entry.bb] = entry.replacement_id;
        }
        if(entry.type == USE){
            auto it = definition_map.find(entry.id);
            if (it != definition_map.end()){
                if(it->second.find(entry.bb) != it->second.end()){
                    entry.replacement_id = definition_map[entry.id][entry.bb];
                } 
                else { /* No definition in the current block => Added in a later pass */ }
            }
        }
    }


    /* -------- PHI CONSTRUCTION --------- */

    std::vector<phi> phis;
    std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> phi_indexing;

    for(auto& [id, IDF] : Is){
        for(auto val : IDF){
            phi p = phi(val, var_id++, id);
            
            if(definition_map[id].find(val) == definition_map[id].end()){
                definition_map[id][val] = p.id;
            } 
            phi_indexing[id].push_back(val);
            
            for(auto pred: graph.blocks[val - offset].get()->pred){
                p.elems.push_back(get_param(definition_map[id], pred->id, offset, dj));
            }
            
            phis.push_back(p);
        }
    }


    /* -------- MISSING USE FILL --------- */

    /** Cases:
     *      1. NO DEFINITION => There exists no definition in the current block (-> neither defined before, nor phi)
                -> Recursive search over the dom-parents
                    ¬ ORIG ∧ ¬ PHI  =>  ¬ DEF
     *      2. SINGLE NEW DEFINITION => No initial definition, but there exists one now (-> phi inserted)
                -> Insert from table (Because it is a phi by design)
                    ¬ ORIG ∧ PHI  =>  DEF ∧ PHI
     *      3. REDEFINITION BY PHI => Inital definition, but hasn't been assigned in the first pass, and there exists a phi
                -> I.e. the definition occurred later and a phi was added afterwards, therefore use the phi
                -> Insert from phi's
                    ORIG ∧ PHI   =>   DEF ∧ PHI
     *      4. NO REDEFINITION => Case 3, but no phi function inserted
                -> Can be treated as a recursive search
                    ORIG ∧ ¬ PHI   =>   DEF ∧ ¬ PHI
    */
    for(auto& entry: map){
        if(entry.type == USE && entry.replacement_id == NULL){
            if(definition_map[entry.id].find(entry.bb) == definition_map[entry.id].end()){ // Case 1
                entry.replacement_id = get_recursive(definition_map[entry.id], phi_indexing[entry.id], entry.bb, offset, entry.id, phis, dj);
            } else{
                if(std::find(phi_indexing[entry.id].begin(), phi_indexing[entry.id].end(), entry.bb) == phi_indexing[entry.id].end()) { // Case 4
                    entry.replacement_id = get_recursive(definition_map[entry.id], phi_indexing[entry.id], entry.bb, offset, entry.id, phis, dj);
                } else { // Case 2 + 3
                    entry.replacement_id = get_from_phi(phis, entry.bb, entry.id);
                }
            }
        }
    }


    /* ------------- FINISH -------------- */

    return phis;
}





/* ================================================================== */
/* ======================== SSA CONSTRUCTION ======================== */
/* ================================================================== */

/**
 * Storage for the new opcodes (based on transformer.c)
 */
static opcodes new_opcode;


/**
 * Copy an opcode into the new opcode stream (copied from transformer.c)
 * 
 * @param o Opcode to copy
 */
static void copy_opcode(opcode *o) {
	uint8_t *new_data = &new_opcode.o[new_opcode.size];

	assert(new_opcode.size + o->size < OPCODES_SIZE);

	memcpy(new_data, o, o->size);

	new_opcode.size += o->size;
}


/**
 * Create the opcode for the phi-function and copy it to the new stream
 * 
 * @param p The phi function struct
 */
static void create_phi_opcode(const phi& p){
    opcode new_phi = {
        .type = OPCODE_PHI,
        .op_phi = {
            .to = p.id,
            .preds = NULL,
            .preds_size = (uint8_t)p.elems.size()
        }
    };

    new_phi.op_phi.preds =
        (uint64_t*)malloc(sizeof(uint64_t) * p.elems.size());

    for(size_t i = 0; i < p.elems.size(); i++){
        new_phi.op_phi.preds[i] = p.elems[i];
    }

    new_phi.size = OP_SIZE(new_phi, op_phi);

    copy_opcode(&new_phi);
}


/**
 * Function to traverse the phi's and insert the necessary phi's
 * 
 * @param phis The list of phi functions
 * @param bb_idx The basic block currently starting with
 */
void insert_phi(std::vector<phi>& phis, const std::uint16_t bb_idx){
    int phi_idx = 0;
    while(phi_idx < phis.size()){
        if(phis[phi_idx].bb_id == bb_idx){
            create_phi_opcode(phis[phi_idx]);
            phis.erase(phis.begin() + phi_idx);
        } else {
            phi_idx ++;
        }
    }
}


void construct_SSA(cfg& graph, std::vector<def_use_map>& map, std::vector<phi>& phis){
    function *f = get_function(graph.fun_idx);
    if (f->block == NULL) {
        return;
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

	new_opcode.size = 0;

    int map_idx = 0;
    std::uint16_t block_idx = graph.entry->id;

    bool block_start = false;

	size_t index = 0;
	while (index < size) {
        opcode *o = (opcode *)&data[index];

        if(map_idx >= map.size()){
            copy_opcode(o);
            index += o->size;
            continue;
        }
        
        switch (o->type) {
            case OPCODE_STORE_VARIABLE:
                if(map[map_idx].type == USE && o->op_store_var.from.index == map[map_idx].id){
                    o->op_store_var.to.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                if(map[map_idx].type == STORE && o->op_store_var.to.index == map[map_idx].id){
                    o->op_store_var.to.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
            case OPCODE_VAR:
                if(map[map_idx].type == ASSIGN && o->op_var.var.index == map[map_idx].id){
                    o->op_var.var.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
            case OPCODE_NOT:
                if(map[map_idx].type == USE && o->op_not.from.index == map[map_idx].id){
                    o->op_not.from.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                if(map[map_idx].type == ASSIGN && o->op_not.to.index == map[map_idx].id){
                    o->op_not.to.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
            case OPCODE_NEGATE:
                if(map[map_idx].type == USE && o->op_negate.from.index == map[map_idx].id){
                    o->op_negate.from.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                if(map[map_idx].type == ASSIGN && o->op_negate.to.index == map[map_idx].id){
                    o->op_negate.to.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
            case OPCODE_RETURN:
                if(map[map_idx].type == USE && o->op_return.var.index == map[map_idx].id){
                    o->op_return.var.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
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
                if(map[map_idx].type == USE && o->op_binary.left.index == map[map_idx].id){
                    o->op_binary.left.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                if(map[map_idx].type == USE && o->op_binary.right.index == map[map_idx].id){
                    o->op_binary.right.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                if(map[map_idx].type == ASSIGN && o->op_binary.result.index == map[map_idx].id){
                    o->op_binary.result.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
            case OPCODE_IF:
                if(map[map_idx].type == USE && o->op_if.condition.index == map[map_idx].id){
                    o->op_if.condition.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
            case OPCODE_WHILE_CONDITION:
                if(map[map_idx].type == USE && o->op_while.condition.index == map[map_idx].id){
                    o->op_while.condition.index = map[map_idx].replacement_id;
                    map_idx++;
                }
                copy_opcode(o);
                break;
    		default:
	    		copy_opcode(o);
		    	break;
		}

		index += o->size;

        // Handle Phi
        if(block_start){
            block_start = false;
            insert_phi(phis, block_idx);
        }
        
        if(index >= graph.blocks[block_idx - graph.entry->id].get()->upper_offset_idx){
            block_start = true;
            block_idx ++;
        }
	}

    if(phis.size() != 0){
        kong_log(LOG_LEVEL_ERROR, "[INTERNAL ERROR] Phi list NOT empty after bytecode traversal!");
        exit(1);
    }

	f->code = new_opcode;
}
