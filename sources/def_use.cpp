#include "def_use.h"

#include <tuple>
#include <unordered_map>
#include <vector>

#include "cfg.h"


/* ================================================================== */
/* ========================= DEF-USE CHAINS ========================= */
/* ================================================================== */

[[nodiscard]] std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> discover_store(const cfg& graph){
    std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> store = {};

    for (const auto& block_ptr : graph.blocks) {
        const cfg_block* block = block_ptr.get();
        const std::uint16_t bb_id = block->id;
        for (auto* o : block->instructions) {
            switch (o->type){
                case OPCODE_STORE_VARIABLE:
                case OPCODE_SUB_AND_STORE_VARIABLE:
                case OPCODE_ADD_AND_STORE_VARIABLE:
                case OPCODE_DIVIDE_AND_STORE_VARIABLE:
                case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                    if(o->op_store_var.to.kind != VARIABLE_GLOBAL)
                        store[o->op_store_var.to.index].push_back(bb_id);
                    break;
                default:
                    break;
            }
        }
    }
    return store;
}


[[nodiscard]] std::vector<def_use_map> discover_def_use(const cfg& graph, std::uint64_t& var_id){
    std::vector<def_use_map> map;
    const std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> store = discover_store(graph);

    for(const auto& block_ptr : graph.blocks){
        const cfg_block* block = block_ptr.get();
        const std::uint16_t bb_id = block->id;

        for (auto* o : block->instructions) {
            switch (o->type){
                case OPCODE_STORE_VARIABLE:
                case OPCODE_SUB_AND_STORE_VARIABLE:
                case OPCODE_ADD_AND_STORE_VARIABLE:
                case OPCODE_DIVIDE_AND_STORE_VARIABLE:
                case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                    if(store.find(o->op_store_var.from.index) != store.end())
                        map.push_back(def_use_map(o->op_store_var.from.index, USE, bb_id, NULL, o->type));
                    if(o->op_store_var.to.kind != VARIABLE_GLOBAL)
                        map.push_back(def_use_map(o->op_store_var.to.index, STORE, bb_id, var_id++, o->type));
                    break;
                case OPCODE_STORE_ACCESS_LIST:
                case OPCODE_SUB_AND_STORE_ACCESS_LIST:
                case OPCODE_ADD_AND_STORE_ACCESS_LIST:
                case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
                case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
                    if(store.find(o->op_store_access_list.from.index) != store.end())
                        map.push_back(def_use_map(o->op_store_access_list.from.index, USE, bb_id, NULL, o->type));
                    // TODO: Structs etc using Access Lists for now kept in Memory
                    break;
                case OPCODE_LOAD_ACCESS_LIST:
                    if(store.find(o->op_load_access_list.to.index) != store.end())
                        map.push_back(def_use_map(o->op_load_access_list.to.index, ASSIGN, bb_id, var_id++, o->type));
                    break;
                case OPCODE_VAR:
                    if(store.find(o->op_var.var.index) != store.end())
                        map.push_back(def_use_map(o->op_var.var.index, ASSIGN, bb_id, var_id++, o->type));
                    break;
                case OPCODE_NOT:
                    if(store.find(o->op_not.from.index) != store.end())
                        map.push_back(def_use_map(o->op_not.from.index, USE, bb_id, NULL, o->type));
                    if(store.find(o->op_not.to.index) != store.end())
                        map.push_back(def_use_map(o->op_not.to.index, ASSIGN, bb_id, var_id++, o->type));
                    break;
                case OPCODE_NEGATE:
                    if(store.find(o->op_negate.from.index) != store.end())
                        map.push_back(def_use_map(o->op_negate.from.index, USE, bb_id, NULL, o->type));
                    if(store.find(o->op_negate.to.index) != store.end())
                        map.push_back(def_use_map(o->op_negate.to.index, ASSIGN, bb_id, var_id++, o->type));
                    break;
                case OPCODE_RETURN:
                    if(store.find(o->op_return.var.index) != store.end()){
                        map.push_back(def_use_map(o->op_return.var.index, USE, bb_id, NULL, o->type));
                    }
                    break;
                case OPCODE_CALL:
                    for(int i = 0; i < o->op_call.parameters_size; ++i){
                        if(store.find(o->op_call.parameters[i].index) != store.end()){
                            map.push_back(def_use_map(o->op_call.parameters[i].index, USE, bb_id, NULL, o->type));
                        }
                    }
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
                    if(store.find(o->op_binary.left.index) != store.end()){
                        map.push_back(def_use_map(o->op_binary.left.index, USE, bb_id, NULL, o->type));
                    }
                    if(store.find(o->op_binary.right.index) != store.end()){
                        map.push_back(def_use_map(o->op_binary.right.index, USE, bb_id, NULL, o->type));
                    }
                    if(store.find(o->op_binary.result.index) != store.end()){
                        map.push_back(def_use_map(o->op_binary.result.index, ASSIGN, bb_id, var_id++, o->type));
                    }
                    break;
                case OPCODE_IF:
                    if(store.find(o->op_if.condition.index) != store.end()){
                        map.push_back(def_use_map(o->op_if.condition.index, USE, bb_id, NULL, o->type));
                    }
                    break;
                case OPCODE_WHILE_CONDITION:
                    if(store.find(o->op_while.condition.index) != store.end()){
                        map.push_back(def_use_map(o->op_while.condition.index, USE, bb_id, NULL, o->type));
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return map;
}
