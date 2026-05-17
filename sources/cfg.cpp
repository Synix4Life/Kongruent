#include "cfg.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "compiler.h"
#include "errors.h"
#include "functions.h"
#include "global.h"
#include "log.h"


/**
 * Link to blocks -> Create an edge
 * 
 * @param from The source node
 * @param to The target node
 */
void link_blocks(cfg_block* from, cfg_block* to){
	if (!from || !to) return;
	from->succ.push_back(to);
	to->pred.push_back(from);
}


[[nodiscard]] std::vector<cfg> make_cfgs(){
    std::vector<cfg> graphs;
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
		control_graph.name = get_name(f->name);
		control_graph.fun_idx = i;

        uint8_t *data = f->code.o;
		size_t   size = f->code.size;
        size_t index = 0;
		Identifier ident = NONE;

		auto curr = std::make_unique<cfg_block>(id++);
		control_graph.entry = curr.get();

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
					curr.get()->upper_offset_idx = index;
					control_graph.blocks.push_back(std::move(curr));

					curr = std::make_unique<cfg_block>(id++);
					link_blocks(ptr, curr.get());

                    ident = WHILE;
					control_graph.num_edges ++;
                    break;
				}
				case OPCODE_BLOCK_START: {
					curr.get()->upper_offset_idx = index;
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

					curr.get()->upper_offset_idx = index;
					std::unique_ptr<cfg_block> created = std::move(curr);
					curr = std::make_unique<cfg_block>(id++);

					auto& [parent, parent_ident] = stack.top();
					if(parent_ident == IF){
						link_blocks(created.get(), curr.get());
					} 
					else if(parent_ident == WHILE){
						link_blocks(created.get(), parent.get());
					}
					else{
						debug_context context = KONG_INIT_ZERO;
						error(context, "[ERROR] OPCODE_BLOCK_END without stacked parent-identifier in %s", control_graph.name.c_str());
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
			debug_context context = KONG_INIT_ZERO;
			error(context, "[ERROR] CFG-stack isn't empty: %s -> %d block(s) still stacked", control_graph.name.c_str(), stack.size());
		}

		if(!curr->instructions.empty()) {
			curr.get()->upper_offset_idx = index;
			control_graph.blocks.push_back(std::move(curr));
		}

		std::sort(
			control_graph.blocks.begin(), 
			control_graph.blocks.end(), 
			[](const std::unique_ptr<cfg_block>& a, const std::unique_ptr<cfg_block>& b)
			
		{ return a->id < b->id; }
		);

		graphs.push_back(std::move(control_graph));
	}
	
	return graphs;
}
