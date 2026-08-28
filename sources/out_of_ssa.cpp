#include "out_of_ssa.h"


#include <cstdint>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>


#include "compiler.h"


namespace ssa {
namespace recompilation {



/** ---------------------------------------------------------------
 *  ---------------------------- UTIL -----------------------------
 *  --------------------------------------------------------------- */

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

static void create_assign_opcode(uint64_t to, uint64_t from){
    opcode new_assign = {
        .type = OPCODE_ASSIGN,
        .op_assign = {
            .to = to,
            .from = from
        }
    };

    new_assign.size = OP_SIZE(new_assign, op_assign);

    copy_opcode(&new_assign);
}



/** ---------------------------------------------------------------
 *  ------------------------ SREEDHAR UTIL ------------------------
 *  --------------------------------------------------------------- */

using copy = candidate_resource;
using copies = std::set<copy>;


/** "Insert" the required copies.
 * Note that because of the bytecode design, the copies aren't actually inserted here, 
 *  but scheduled and deferred until a later bytecode pass
 */
void insert_copy(
    uint64_t& sequence_id,
    candidate_resource& resource,
    live_sets& live_vars,
    congruence_class& classes,
    interference_graph& i_graph, 
    cfg& f,
    opcode*& op,
    std::unique_ptr<cfg_block>& phi_bb,
    copies& c
){
    if(resource.type == SOURCE){
        // For now/ By design: Only one BB per variable
        auto new_ = sequence_id ++;
        //c.insert(copy(new_, resource.target, resource.type, resource.bb_for_copy)); // Proxy for "Insert a copy inst: xnew_i = xi at the end of Lk;"
        c.insert(copy(resource.target, new_, resource.type, resource.bb_for_copy)); // Proxy for "Insert a copy inst: xnew_i = xi at the end of Lk;"

        // Replace xi with xnew_i in phiInst;
        for(int i=0; i<phi_bb.get()->pred.size(); ++i){
            if(phi_bb.get()->pred[i]->id == resource.bb_for_copy){
                op->op_phi.preds[i] = new_;
                break;
            }
        }

        classes[new_].insert(new_); // Add xnew_i in phiCongruenceClass[xnew_i]
        live_vars.LiveOut[resource.bb_for_copy].insert(new_); // LiveOut[Lk] += xnew_i;
        live_vars.LiveOut[resource.bb_for_copy].erase(resource.target); // LiveOut[Lk] -= xi;
        link_var(i_graph, live_vars.LiveOut[resource.bb_for_copy], new_); // Build interference edges between xnew_i and LiveOut[Lk];
    } 
    else { // resource.type == TARGET
        auto new_ = sequence_id ++;
        //c.insert(copy(new_, resource.target, resource.type, resource.bb_for_copy)); // Proxy for "Insert a copy inst: x0 = xnew_0 at the beginning of L0;"
        c.insert(copy(resource.target, new_, resource.type, resource.bb_for_copy)); // Proxy for "Insert a copy inst: xnew_i = xi at the end of Lk;"
        op->op_phi.to = new_; // Replace x0 with xnew_0 as the target in phiInst;
        classes[new_].insert(new_); // Add xnew_0 in phiCongruenceClass[xnew_0]
        live_vars.LiveIn[resource.bb_for_copy].erase(resource.target); // LiveIn[L0] -= x0;
        live_vars.LiveIn[resource.bb_for_copy].insert(new_); // LiveIn[L0] += xnew_0;
        link_var(i_graph, live_vars.LiveIn[resource.bb_for_copy], new_); // Build interference edges between xnew_0 and LiveIn[L0];
    }
}


void insert_into_stream(cfg& graph, copies& _copies){
    function *f = get_function(graph.fun_idx);
    if (f->block == NULL) {
        return;
	}

	new_opcode.size = 0;

    //auto is_terminator = [&](opcode* last_instruction) {
    //    auto& t = last_instruction->type;
    //    return t == OPCODE_WHILE_CONDITION || t == OPCODE_WHILE_END || t == OPCODE_IF || t == OPCODE_BLOCK_END ;
    //};

    for(auto& bb : graph.blocks){
        auto* block = bb.get();
        const std::uint16_t bb_id = block->id;
        const size_t num_instructions = block->instructions.size();

        size_t i = 0;

        // Process the PHI instructions
        while(i < num_instructions && block->instructions[i]->type == OPCODE_PHI){
            copy_opcode(block->instructions[i]);
            ++i;
        }
        
        // Target copies (after PHIs)
        for (auto it = _copies.begin(); it != _copies.end(); ) {
            if (it->bb_for_copy == bb_id && it->type == TARGET) {
                create_assign_opcode(it->target, it->new_target);
                it = _copies.erase(it);
            } else {
                ++it;
            }
        }
        
        // Process instructions except final terminator/ instruction
        while (i + 1 < num_instructions){
            copy_opcode(block->instructions[i]);
            ++i;
        }

        // Source copies
        for (auto it = _copies.begin(); it != _copies.end(); ) {
            if (it->bb_for_copy == bb_id && it->type == SOURCE) {
                create_assign_opcode(it->new_target, it->target);
                it = _copies.erase(it); // Safe erasure
            } else {
                ++it;
            }
        }

        if (i < num_instructions){
            copy_opcode(block->instructions[i]);
        }        
    }

    f->code = new_opcode;
}


/**
 * Check if two variables xi and xj interfere. 
 * Return true if they interfere, return false if they don't
 * 
 * @param i The interference graph
 * @param xi Primary variable
 * @param xj Secondary variable
 * 
 * @return If they interfere
 * @throw Internal Error if the interference graph is malformed
 *  (i[xi].has(xj)!=i[xj].has(xi))
 */
bool interfere(interference_graph& i, std::uint64_t xi, std::uint64_t xj){
    const bool a = i[xi].find(xj) != i[xi].end(); // Worst case O(n)
    const bool b = i[xj].find(xi) != i[xj].end(); // Average O(1)

    if (a != b){
        kong_log(LOG_LEVEL_ERROR, "[ INTERNAL ERROR ] Interference graph contains invalid data");
        exit(1);
    }

    return a;
}


/**
 * Check if the intersection of a class and a LiveOut set of a specific block
 * 
 * @param classes The Congruence classes
 * @param LiveOut_L The LiveOut set for the specific block L (live_sets.LiveOut[L])
 * @param x The variable to check if the class interferes
 * 
 * @return If the intersection is empty (return true if empty)
 */
bool intersect_class_set(
    congruence_class& classes, 
    std::unordered_set<std::uint64_t> LiveOut_L,
    std::uint64_t x
){
    for (const auto value : classes[x]) {
        if (LiveOut_L.find(value) != LiveOut_L.end()) {
            return false;
        }
    }
    return true;
}



/** ---------------------------------------------------------------
 *  ----------------------- SREEDHAR ET AL. -----------------------
 *  --------------------------------------------------------------- */

void resolve_interferences(
    std::unique_ptr<cfg_block>& bb,
    candidate_resource_set& candidateResourceSet,
    unresolved_neighbor_map& map,
    congruence_class& classes,
    interference_graph& i_graph,
    live_sets& live_vars,
    opcode*& op
){
    // Make a resource set that has all resources
    std::vector<uint64_t> resources;
    resources.push_back(op->op_phi.to);

    for (int i = 0; i < op->op_phi.preds_size; i++)
        resources.push_back(op->op_phi.preds[i]);

    // TODO: For keeping the unresolved resource information
    // With changes to the phi function, can be discarded in the future
    std::unordered_map<std::uint64_t, candidate_resource> unresolved_track;

    // Iterate over them to find possible interferences
    for (size_t i = 0; i < resources.size(); i++) {
        for (size_t j = i + 1; j < resources.size(); j++) {

            // " For each pair of resources xi:Li and xj:Lj in phiInst, where 0 <= i, j <= n and xi != xj, 
            // such that there exists yi in phiCongruenceClass[xi], yj in phiCongruenceClass[xj],
            // and yi and yj interfere with each other " -> Sreedhar et al. - 1999 - Translating out of static single assignment form

            auto xi = resources[i];
            auto xj = resources[j];

            bool interference = false;

            for (auto yi : classes[xi]) {
                for (auto yj : classes[xj]) {
                    if (interfere(i_graph, yi, yj)) {
                        interference = true;
                    }
                }
            }

            // If they interfere, find out which of the four cases apply
            if(interference){
                // Get the bb_ids for the variables, i.e. as the resources are phi-sources/ targets,
                // find out where they come from (based on the assumption that the ith variable belongs to the ith predecessor)
                // Subtract from that one to register that the target sits at position one (being the current bb)
                auto Li = i == 0 ? bb.get()->id : bb.get()->pred[i-1]->id;
                auto Lj = bb.get()->pred[j-1]->id; // j can never be 0 because of the loop design, i.e. it's always a SOURCE variable => pred

                // See which fo the intersections regarding the 4-case system presented in the paper exist
                bool xi_Lj_intersect = intersect_class_set(classes, live_vars.LiveOut[Lj], xi);
                bool xj_Li_intersect = intersect_class_set(classes, live_vars.LiveOut[Li], xj);

                // Process the actual cases
                if(!xi_Lj_intersect && xj_Li_intersect){ // Case 1
                    candidateResourceSet.push(
                        candidate_resource(xi, i == 0 ? TARGET : SOURCE , Li)
                    );
                }
                else if(xi_Lj_intersect && !xj_Li_intersect){ // Case 2
                    candidateResourceSet.push(
                        candidate_resource(xj, SOURCE , Lj)
                    );
                }
                else if(!xi_Lj_intersect && !xj_Li_intersect){ // Case 3
                    candidateResourceSet.push(
                        candidate_resource(xi, i == 0 ? TARGET : SOURCE , Li)
                    );
                    candidateResourceSet.push(
                        candidate_resource(xj, SOURCE , Lj)
                    );
                }
                else if(xi_Lj_intersect && xj_Li_intersect){ // Case 4 -> Deferred
                    map[xi].insert(xj);
                    unresolved_track.emplace(
                        xi,
                        candidate_resource(xi, i == 0 ? TARGET : SOURCE, Li)
                    );
                    map[xj].insert(xi);
                    unresolved_track.emplace(
                        xj,
                        candidate_resource(xj, SOURCE, Lj)
                    );
                }
            }
            
        }
    }

    // Process unresolved copies
    std::unordered_set<std::uint64_t> already_resolved;

    std::priority_queue<std::pair<std::size_t, std::uint64_t>> queue;

    for (const auto& [resource, neighbors] : map) {
        queue.emplace(neighbors.size(), resource);
    }

    // Resolve the neighbors by TODO:
    while(!queue.empty()){
        auto [_, resource] = queue.top();
        queue.pop();

        std::size_t unresolved_count = 0;

        for (const auto neighbor : map.at(resource)) {
            if (already_resolved.find(neighbor) == already_resolved.end()) {
                ++unresolved_count;
            }
        }

        if (unresolved_count == 0) {
            continue;
        }

        candidateResourceSet.push(unresolved_track.at(resource));
        already_resolved.insert(resource);
    }
}


congruence_class eliminate_phi_resource_interference(
    uint64_t& sequence_id,
    cfg& f, 
    interference_graph& i_graph, 
    live_sets& live_vars
){
    congruence_class classes;
    
    /* --------------- Initial congruence class construction --------------- */

    for(auto& block: f.blocks){ for(auto& op: block.get()->instructions){
        if(op->type == OPCODE_PHI){
            classes[op->op_phi.to].insert(op->op_phi.to);
            for(int i = 0; i < op->op_phi.preds_size; ++i){
                classes[op->op_phi.preds[i]].insert(op->op_phi.preds[i]);
            }
        }
    } }


    copies c;

    /* --------------- Process phi's --------------- */
    for(auto& block: f.blocks){ for(auto& op: block.get()->instructions){
        if(op->type == OPCODE_PHI){

            unresolved_neighbor_map map;
            candidate_resource_set candidateResourceSet;

            // resolve the interferences and build candidateResourceSet
            resolve_interferences(
                block,
                candidateResourceSet,
                map,
                classes,
                i_graph,
                live_vars,
                op
            );

            // insert copy
            while (!candidateResourceSet.empty()) {
                auto& resource = candidateResourceSet.front();

                kong_log(LOG_LEVEL_INFO, "Inserting copy: BB[%d], %d = %d, Type(%s)", resource.bb_for_copy, resource.new_target, resource.target, resource.type == SOURCE ? "SOURCE" : "TARGET");

                insert_copy(
                    sequence_id,
                    resource,
                    live_vars,
                    classes,
                    i_graph,
                    f,
                    op,
                    block,
                    c
                );

                candidateResourceSet.pop();
            }

            // merge congruence classes
            congruence_set current;

            std::vector<uint64_t> resources;
            resources.push_back(op->op_phi.to);

            for (int i = 0; i < op->op_phi.preds_size; i++)
                resources.push_back(op->op_phi.preds[i]);
            
            for(auto& resource : resources){
                current.insert(classes[resource].begin(), classes[resource].end());
            }
            for(auto& resource : resources){
                classes[resource] = current;
            }
        }
    } }

    /* ------------- Bytecode pass  ------------- */
    // To actually insert the copies
    insert_into_stream(f, c);


    /* --------------- Finalize --------------- */

    for (auto it = classes.begin(); it != classes.end(); ) {
        if (it->second.size() == 1)
            it = classes.erase(it);
        else
            ++it;
    }

    return classes;

}



/** ---------------------------------------------------------------
 *  ------------------------- COALESCING --------------------------
 *  --------------------------------------------------------------- */

/**
 * Create the replacement map, to correctly merge the variables into others
 * @param classes The congruence classes
 * @param sequence_id Sequence id
 */
std::unordered_map<std::uint64_t, std::uint64_t> create_replacements(congruence_class& classes, uint64_t& sequence_id){
    std::unordered_map<std::uint64_t, std::uint64_t> replacement_map;
    for (auto& [target, class_set] : classes) {
        if (!replacement_map.count(target)) {
            auto curr = sequence_id ++;
            for (auto entry : class_set)
                replacement_map[entry] = curr;
        }
    }
    return replacement_map;
}


void clean_phi_functions(uint64_t& sequence_id, cfg& graph, congruence_class& classes){
    auto replacement = create_replacements(classes, sequence_id);

    function *f = get_function(graph.fun_idx);
    if (f->block == NULL) {
        return;
	}

	new_opcode.size = 0;

    for(auto& bb: graph.blocks){
        const std::uint16_t bb_id = bb.get()->id;
        for(auto& instr : bb.get()->instructions){
            switch (instr->type){
                case OPCODE_STORE_VARIABLE:
                case OPCODE_SUB_AND_STORE_VARIABLE:
                case OPCODE_ADD_AND_STORE_VARIABLE:
                case OPCODE_DIVIDE_AND_STORE_VARIABLE:
                case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                    if(replacement.find(instr->op_store_var.from.index) != replacement.end()){
                        instr->op_store_var.from.index = replacement[instr->op_store_var.from.index];
                    }
                    if(replacement.find(instr->op_store_var.to.index) != replacement.end()){
                        instr->op_store_var.to.index = replacement[instr->op_store_var.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_LOAD_ACCESS_LIST:
                    if(replacement.find(instr->op_load_access_list.from.index) != replacement.end()){
                        instr->op_load_access_list.from.index = replacement[instr->op_load_access_list.from.index];
                    }
                    if(replacement.find(instr->op_load_access_list.to.index) != replacement.end()){
                        instr->op_load_access_list.to.index = replacement[instr->op_load_access_list.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_STORE_ACCESS_LIST:
                case OPCODE_SUB_AND_STORE_ACCESS_LIST:
                case OPCODE_ADD_AND_STORE_ACCESS_LIST:
                case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
                case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
                    // TODO: Structs etc using Access Lists for now kept in Memory
                    if(replacement.find(instr->op_store_access_list.from.index) != replacement.end()){
                        instr->op_store_access_list.from.index = replacement[instr->op_store_access_list.from.index];
                    }
                    if(replacement.find(instr->op_store_access_list.to.index) != replacement.end()){
                        instr->op_store_access_list.to.index = replacement[instr->op_store_access_list.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_VAR:
                    if(replacement.find(instr->op_var.var.index) != replacement.end()){
                        instr->op_var.var.index = replacement[instr->op_var.var.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_NOT:
                    if(replacement.find(instr->op_not.from.index) != replacement.end()){
                        instr->op_not.from.index = replacement[instr->op_not.from.index];
                    }
                    if(replacement.find(instr->op_not.to.index) != replacement.end()){
                        instr->op_not.to.index = replacement[instr->op_not.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_NEGATE:
                    if(replacement.find(instr->op_negate.from.index) != replacement.end()){
                        instr->op_negate.from.index = replacement[instr->op_negate.from.index];
                    }
                    if(replacement.find(instr->op_negate.to.index) != replacement.end()){
                        instr->op_negate.to.index = replacement[instr->op_negate.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_RETURN:
                    if(replacement.find(instr->op_return.var.index) != replacement.end()){
                        instr->op_return.var.index = replacement[instr->op_return.var.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_CALL:
                    for(int i = 0; i < instr->op_call.parameters_size; ++i){
                        if(replacement.find(instr->op_call.parameters[i].index) != replacement.end()){
                            instr->op_call.parameters[i].index = replacement[instr->op_call.parameters[i].index];
                        }
                    }
                    if(replacement.find(instr->op_call.var.index) != replacement.end()){
                        instr->op_call.var.index = replacement[instr->op_call.var.index];
                    }
                    copy_opcode(instr);
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
                    if(replacement.find(instr->op_binary.left.index) != replacement.end()){
                        instr->op_binary.left.index = replacement[instr->op_binary.left.index];
                    }
                    if(replacement.find(instr->op_binary.right.index) != replacement.end()){
                        instr->op_binary.right.index = replacement[instr->op_binary.right.index];
                    }
                    if(replacement.find(instr->op_binary.result.index) != replacement.end()){
                        instr->op_binary.result.index = replacement[instr->op_binary.result.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_LOAD_FLOAT_CONSTANT:
                    if(replacement.find(instr->op_load_float_constant.to.index) != replacement.end()){
                        instr->op_load_float_constant.to.index = replacement[instr->op_load_float_constant.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_LOAD_INT_CONSTANT:
                    if(replacement.find(instr->op_load_int_constant.to.index) != replacement.end()){
                        instr->op_load_int_constant.to.index = replacement[instr->op_load_int_constant.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_LOAD_BOOL_CONSTANT:
                    if(replacement.find(instr->op_load_bool_constant.to.index) != replacement.end()){
                        instr->op_load_bool_constant.to.index = replacement[instr->op_load_bool_constant.to.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_IF:
                    if(replacement.find(instr->op_if.condition.index) != replacement.end()){
                        instr->op_if.condition.index = replacement[instr->op_if.condition.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_WHILE_CONDITION:
                    if(replacement.find(instr->op_while.condition.index) != replacement.end()){
                        instr->op_while.condition.index = replacement[instr->op_while.condition.index];
                    }
                    copy_opcode(instr);
                    break;
                case OPCODE_PHI:
                    // Discard -> Do not keep in the coalescing pass
                    break;
                case OPCODE_ASSIGN:
                    if(replacement.find(instr->op_assign.from) != replacement.end()){
                        instr->op_assign.from = replacement[instr->op_assign.from];
                    }
                    if(replacement.find(instr->op_assign.to) != replacement.end()){
                        instr->op_assign.to = replacement[instr->op_assign.to];
                    }
                    copy_opcode(instr);
                    break;
                default:
                    // Not specifically treated: 
                    //   DISCARD → NiL , BLOCK_* → Just indices
                    copy_opcode(instr);
                    break;
            }
        }
    }

    f->code = new_opcode;
}

} // namespace recompilation
} // namespace SSA
