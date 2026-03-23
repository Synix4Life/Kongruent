#include "cfg.h"

#include <stack>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>

#include "functions.h"
#include "compiler.h"
#include "log.h"
#include "errors.h"


void link_blocks(cfg_block* from, cfg_block* to){
	if (!from || !to) return;
	from->succ.push_back(to);
	to->pred.push_back(from);
}

cfgs make_cfgs(){
    cfgs graphs;
	std::uint16_t id = 0;

	/**
	 * The stack is for finished blocks only. 
	 * It is used to track pred and succ assignments.
	 */
	std::stack<std::tuple<std::unique_ptr<cfg_block>, Identifier>> stack;

    for (function_id i = 0; get_function(i) != NULL; ++i) {
        function *f = get_function(i);

        if (f->block == NULL) continue;

        cfg control_graph;
		control_graph.num_edges = 0;
		control_graph.name = get_name(f->name);

        uint8_t *data = f->code.o;
		size_t   size = f->code.size;
        size_t index = 0;
		Identifier ident = NONE;

		auto curr = std::make_unique<cfg_block>(id++);

        while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
				case OPCODE_CALL: 
					break;
				case OPCODE_IF:
					curr->instructions.push_back(o);
					ident = IF;
					break;
				case OPCODE_WHILE_START: {
					curr->instructions.push_back(o);

					cfg_block* ptr = curr.get();
					control_graph.blocks.push_back(std::move(curr));

					curr = std::make_unique<cfg_block>(id++);
					link_blocks(ptr, curr.get());

                    ident = WHILE;
					control_graph.num_edges ++;
                    break;
				}
				case OPCODE_BLOCK_START: {
					stack.push(std::make_tuple(std::move(curr), ident));
					
					curr = std::make_unique<cfg_block>(id++);
					link_blocks(std::get<0>(stack.top()).get(), curr.get());

					curr->instructions.push_back(o);
					ident = NONE;
					control_graph.num_edges ++;
					break;
				}
				case OPCODE_BLOCK_END: {
					curr->instructions.push_back(o);

					std::unique_ptr<cfg_block> created = std::move(curr);
					curr = std::make_unique<cfg_block>(id++);

					auto& [parent, parent_ident] = stack.top();
					if(parent_ident == IF){
						link_blocks(created.get(), curr.get());
					} 
					else if(parent_ident == WHILE){
						link_blocks(created.get(), parent.get());
					}

					link_blocks(parent.get(), curr.get());

					control_graph.blocks.push_back(std::move(parent));
					control_graph.blocks.push_back(std::move(created));

					stack.pop();
					control_graph.num_edges += 2;
					break;
				}
				default: {
					curr->instructions.push_back(o);
					break;
				}
			}
			index += o->size;
		}

		if(!stack.empty()){
			debug_context context = {0};
			error(context, "[ERROR] CFG-stack isn't empty: %s -> %d block(s) still stacked", control_graph.name.c_str(), stack.size());
		}

		if(!curr->instructions.empty()) 
			control_graph.blocks.push_back(std::move(curr));

		graphs.function_graphs.push_back(std::move(control_graph));
	}
	return graphs;
}

[[nodiscard]] const std::string make_string(const std::vector<uint16_t> ids) noexcept{
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

void debug_cfgs(const cfgs& graphs){
	for(int i=0; i<graphs.function_graphs.size(); ++i){
		
		const cfg& curr = graphs.function_graphs[i];

		kong_log(LOG_LEVEL_INFO, "FUNC %s, nums_edges %d\n", curr.name.c_str(), curr.num_edges);

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