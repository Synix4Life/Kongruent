#include "debugger.h"

#include <iostream>

#include "cfg.h"
#include "dominator_tree.h"
#include "errors.h"
#include "log.h"


/* ================================================================ */
/* ============================= UTIL ============================= */
/* ================================================================ */

const char* get_opcode_name(const opcode_type type) {
    switch (type) {
        case OPCODE_VAR:                        	return "OPCODE_VAR";
        case OPCODE_NOT:                        	return "OPCODE_NOT";
        case OPCODE_NEGATE:                     	return "OPCODE_NEGATE";
        case OPCODE_STORE_VARIABLE:             	return "OPCODE_STORE_VARIABLE";
        case OPCODE_SUB_AND_STORE_VARIABLE:     	return "OPCODE_SUB_AND_STORE_VARIABLE";
        case OPCODE_ADD_AND_STORE_VARIABLE:     	return "OPCODE_ADD_AND_STORE_VARIABLE";
        case OPCODE_DIVIDE_AND_STORE_VARIABLE:  	return "OPCODE_DIVIDE_AND_STORE_VARIABLE";
        case OPCODE_MULTIPLY_AND_STORE_VARIABLE: 	return "OPCODE_MULTIPLY_AND_STORE_VARIABLE";
        case OPCODE_STORE_ACCESS_LIST:          	return "OPCODE_STORE_ACCESS_LIST";
        case OPCODE_SUB_AND_STORE_ACCESS_LIST:  	return "OPCODE_SUB_AND_STORE_ACCESS_LIST";
        case OPCODE_ADD_AND_STORE_ACCESS_LIST:  	return "OPCODE_ADD_AND_STORE_ACCESS_LIST";
        case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST: 	return "OPCODE_DIVIDE_AND_STORE_ACCESS_LIST";
        case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST: return "OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST";
        case OPCODE_LOAD_FLOAT_CONSTANT:        	return "OPCODE_LOAD_FLOAT_CONSTANT";
        case OPCODE_LOAD_INT_CONSTANT:          	return "OPCODE_LOAD_INT_CONSTANT";
        case OPCODE_LOAD_BOOL_CONSTANT:         	return "OPCODE_LOAD_BOOL_CONSTANT";
        case OPCODE_LOAD_ACCESS_LIST:           	return "OPCODE_LOAD_ACCESS_LIST";
        case OPCODE_RETURN:                     	return "OPCODE_RETURN";
        case OPCODE_DISCARD:                    	return "OPCODE_DISCARD";
        case OPCODE_CALL:                       	return "OPCODE_CALL";
        case OPCODE_MULTIPLY:                   	return "OPCODE_MULTIPLY";
        case OPCODE_DIVIDE:                     	return "OPCODE_DIVIDE";
        case OPCODE_MOD:                        	return "OPCODE_MOD";
        case OPCODE_ADD:                        	return "OPCODE_ADD";
        case OPCODE_SUB:                        	return "OPCODE_SUB";
        case OPCODE_EQUALS:                     	return "OPCODE_EQUALS";
        case OPCODE_NOT_EQUALS:                 	return "OPCODE_NOT_EQUALS";
        case OPCODE_GREATER:                    	return "OPCODE_GREATER";
        case OPCODE_GREATER_EQUAL:              	return "OPCODE_GREATER_EQUAL";
        case OPCODE_LESS:                       	return "OPCODE_LESS";
        case OPCODE_LESS_EQUAL:                 	return "OPCODE_LESS_EQUAL";
        case OPCODE_AND:                        	return "OPCODE_AND";
        case OPCODE_OR:                         	return "OPCODE_OR";
        case OPCODE_BITWISE_XOR:                	return "OPCODE_BITWISE_XOR";
        case OPCODE_BITWISE_AND:                	return "OPCODE_BITWISE_AND";
        case OPCODE_BITWISE_OR:                 	return "OPCODE_BITWISE_OR";
        case OPCODE_LEFT_SHIFT:                 	return "OPCODE_LEFT_SHIFT";
        case OPCODE_RIGHT_SHIFT:                	return "OPCODE_RIGHT_SHIFT";
        case OPCODE_IF:                         	return "OPCODE_IF";
        case OPCODE_WHILE_START:                	return "OPCODE_WHILE_START";
        case OPCODE_WHILE_CONDITION:            	return "OPCODE_WHILE_CONDITION";
        case OPCODE_WHILE_END:                  	return "OPCODE_WHILE_END";
        case OPCODE_WHILE_BODY:                 	return "OPCODE_WHILE_BODY";
        case OPCODE_BLOCK_START:                	return "OPCODE_BLOCK_START";
        case OPCODE_BLOCK_END:                  	return "OPCODE_BLOCK_END";
        
        default:                                	return "UNKNOWN_OPCODE";
    }
}


[[nodiscard]] const std::string make_string(const std::vector<std::uint16_t> ids) noexcept{
	if (ids.empty()) return "";

    std::string result;
    for (size_t i = 0; i < ids.size(); ++i) {
        result += std::to_string(ids[i]);
		if (i < ids.size() - 1) {
            result += ", ";
        }
    }
    return result;
}


[[nodiscard]] std::string make_instruction_string(const std::vector<opcode_type>& types) {
    if (types.empty()) return "[]";

    std::string result = "[";
    for (size_t i = 0; i < types.size(); ++i) {
        result += get_opcode_name(types[i]);
        
        if (i < types.size() - 1) {
            result += ", ";
        }
    }
    result += "]";
    return result;
}

/**
 * Get the String-representation of an EdgeType
 */
const char* get_edge_type_name(const EdgeType type) {
    switch (type) {
        case J_EDGE:        return "J-Edge";
        case D_EDGE:        return "D-Edge";
        case NIL_TYPE:      return "[NIL -> Edge has no type]";
        
        default:            return "UNKNOWN";
    }
}



/* ================================================================ */
/* ====================== DOMINATOR DEBUGGER ====================== */
/* ================================================================ */

void block_id_rpo_map(cfg& graph){
    for(auto& block: graph.blocks){
        kong_log(LOG_LEVEL_INFO, "BLOCK: ID[%d] has DFS_NUM[%d]", block.get()->id, block.get()->dfs_num);
    }
}


void debug_DJ(dj_tree tree){
    kong_log(LOG_LEVEL_INFO, "% ============================== %");
	kong_log(LOG_LEVEL_INFO, "FUNC %s -> DJ-TREE", tree.name.c_str());
	kong_log(LOG_LEVEL_INFO, "% ============================== %\n");

    for(auto& block : tree.blocks){
        kong_log(LOG_LEVEL_INFO, "BLOCK %d at level %d", block.ref_id, block.level);
        for(auto& edge : block.edges){
            kong_log(LOG_LEVEL_INFO, "\t %s(%d->%d)", get_edge_type_name(edge.type), block.ref_id, edge.dest_id);
        }
    }
}


void debug_IDF_out(std::vector<std::uint16_t>& N_alpha, std::vector<std::uint16_t>& IDF){
    kong_log(LOG_LEVEL_INFO, "\n---------------");
	kong_log(LOG_LEVEL_INFO, "IDF CALCULATION");
	kong_log(LOG_LEVEL_INFO, "---------------\n");

    kong_log(LOG_LEVEL_INFO, "N_alpha { %s }\n", make_string(N_alpha).c_str());

    kong_log(LOG_LEVEL_INFO, "IDF { %s }\n", make_string(IDF).c_str());
}



/* ================================================================ */
/* ========================= CFG DEBUGGER ========================= */
/* ================================================================ */

void debug_cfgs(const std::vector<cfg>& graphs){
	for(int i=0; i<graphs.size(); ++i){
		
		const cfg& curr = graphs[i];

		kong_log(LOG_LEVEL_INFO, "% ============================== %");
		kong_log(LOG_LEVEL_INFO, "FUNC %s, nums_edges %d", curr.name.c_str(), curr.num_edges);
		kong_log(LOG_LEVEL_INFO, "% ============================== %\n");

		std::vector<uint16_t> store;

		for(const auto& block : curr.blocks){

			kong_log(LOG_LEVEL_INFO, "Parsing Block id=%u:", (unsigned int)block->id);

			kong_log(LOG_LEVEL_INFO, "\tPRED: ");
			for (auto* pred_block : block->pred) {
				store.push_back(pred_block->id);
			}
			kong_log(LOG_LEVEL_INFO, "\t\t%s", make_string(store).c_str());
			store.clear();

			kong_log(LOG_LEVEL_INFO, "\tSUCC: ");
			for (auto* succ_block : block->succ) {
				store.push_back(succ_block->id);
			}
			kong_log(LOG_LEVEL_INFO, "\t\t%s", make_string(store).c_str());
			store.clear();

			std::vector<opcode_type> code;
			kong_log(LOG_LEVEL_INFO, "\tMember: ");
			for (auto* instruct : block->instructions) {
				code.push_back(instruct->type);
			}
			kong_log(LOG_LEVEL_INFO, "\t\t%s\n", make_instruction_string(code).c_str());
			code.clear();
		}
	}
	kong_log(LOG_LEVEL_INFO, "\n\n");
}
