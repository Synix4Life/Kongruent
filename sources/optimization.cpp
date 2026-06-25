#include "optimization.h"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "cfg.h"
#include "debugger.h"
#include "functions.h"



/* ------------------------------------------------------------------ */
/* ---------------------- FILE-PRIVATE METHODS ---------------------- */
/* ------------------------------------------------------------------ */

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


/* --------------------- CREAT & INSERT OPCODES --------------------- */

static void create_phi_opcode(uint64_t to, std::vector<uint64_t> preds){
    opcode new_phi = {
        .type = OPCODE_PHI,
        .op_phi = {
            .to = to,
            .preds = NULL,
            .preds_size = (uint8_t)preds.size()
        }
    };

    new_phi.op_phi.preds =
        (uint64_t*)malloc(sizeof(uint64_t) * preds.size());

    for(size_t i = 0; i < preds.size(); i++){
        new_phi.op_phi.preds[i] = preds[i];
    }

    new_phi.size = OP_SIZE(new_phi, op_phi);

    copy_opcode(&new_phi);
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

static void create_constant_int(variable to, int value){
    opcode code = {
        .type = OPCODE_LOAD_INT_CONSTANT,
        .op_load_int_constant = {
            .number = value,
            .to = to
        }
    };
    code.size = OP_SIZE(code, op_load_int_constant);
    copy_opcode(&code);
}

static void create_constant_float(variable to, float value){
    opcode code = {
        .type = OPCODE_LOAD_FLOAT_CONSTANT,
        .op_load_float_constant = {
            .number = value,
            .to = to
        }
    };
    code.size = OP_SIZE(code, op_load_float_constant);
    copy_opcode(&code);
}

static void create_constant_bool(variable to, bool value){
    opcode code = {
        .type = OPCODE_LOAD_BOOL_CONSTANT,
        .op_load_bool_constant = {
            .boolean = value,
            .to = to
        }
    };
    code.size = OP_SIZE(code, op_load_bool_constant);
    copy_opcode(&code);
}


/* -------------------- CONSTANT-FOLDING HELPER --------------------- */

/**
 * Check if two constant types are compatible
 * @param a Constant number one
 * @param b Constant number two
 * @return If they are compatible
 */
bool compatible(constant_var a, constant_var b){
    return a.type == b.type || ( a.type == INTEGER && b.type == FLOAT) || ( a.type == FLOAT && b.type == INTEGER);
}

/**
 * Calculate the folding results for two boolean variables
 * 
 * @param LHS Boolean LHS value
 * @param RHS Boolean RHS value
 * @param op The opcode
 * 
 * @return Result
 * 
 * @throws Runtime Exception if the operation is not possible give the data types
 */
bool calculate(const bool LHS, const bool RHS, const opcode_type op){
    switch(op) {
        case OPCODE_EQUALS: return LHS == RHS;
        case OPCODE_NOT_EQUALS: return LHS != RHS;
        case OPCODE_AND: return LHS && RHS;
        case OPCODE_OR: return LHS || RHS;
        default: {
            kong_log(LOG_LEVEL_ERROR, "bool calculate() calculation with %s not possible!", get_opcode_name(op));
            exit(1);
        }
    }
}

/**
 * Calculate the folding results for two integer variables
 * 
 * @param LHS Integer LHS value
 * @param RHS Integer RHS value
 * @param op The opcode
 * 
 * @return Result
 * 
 * @throws Runtime Exception if the operation is not possible give the data types
 */
int calculate(const int LHS, const int RHS, const opcode_type op){
    switch(op) {
        case OPCODE_ADD: return LHS + RHS;
        case OPCODE_SUB: return LHS - RHS;
        case OPCODE_MULTIPLY: return LHS * RHS;
        case OPCODE_DIVIDE: return LHS / RHS;
        case OPCODE_MOD: return LHS % RHS;
        case OPCODE_EQUALS: return LHS == RHS;
        case OPCODE_NOT_EQUALS: return LHS != RHS;
        case OPCODE_GREATER: return LHS > RHS;
        case OPCODE_GREATER_EQUAL: return LHS >= RHS;
        case OPCODE_LESS: return LHS < RHS;
        case OPCODE_LESS_EQUAL: return LHS <= RHS;
        case OPCODE_AND: return LHS && RHS;
        case OPCODE_OR: return LHS || RHS;
        case OPCODE_BITWISE_XOR: return LHS ^ RHS;
        case OPCODE_BITWISE_AND: return LHS & RHS;
        case OPCODE_BITWISE_OR: return LHS | RHS;
        case OPCODE_LEFT_SHIFT: return LHS << RHS;
        case OPCODE_RIGHT_SHIFT: return LHS >> RHS;
        default: {
            kong_log(LOG_LEVEL_ERROR, "int calculate() calculation with %s not possible!", get_opcode_name(op));
            exit(1);
        }
    }
}

/**
 * Calculate the folding results for two float variables
 * 
 * @param LHS Float LHS value
 * @param RHS Float RHS value
 * @param op The opcode
 * 
 * @return Result
 * 
 * @throws Runtime Exception if the operation is not possible give the data types
 */
float calculate(const float LHS, const float RHS, const opcode_type op){
    switch(op) {
        case OPCODE_ADD: return LHS + RHS;
        case OPCODE_SUB: return LHS - RHS;
        case OPCODE_MULTIPLY: return LHS * RHS;
        case OPCODE_DIVIDE: return LHS / RHS;
        case OPCODE_EQUALS: return LHS == RHS;
        case OPCODE_NOT_EQUALS: return LHS != RHS;
        case OPCODE_GREATER: return LHS > RHS;
        case OPCODE_GREATER_EQUAL: return LHS >= RHS;
        case OPCODE_LESS: return LHS < RHS;
        case OPCODE_LESS_EQUAL: return LHS <= RHS;
        case OPCODE_AND: return LHS && RHS;
        case OPCODE_OR: return LHS || RHS;
        default: {
            kong_log(LOG_LEVEL_ERROR, "float calculate() calculation with %s not possible!", get_opcode_name(op));
            exit(1);
        }
    }
}

/**
 * Calculate the folding results for integer-float variables
 * 
 * @param LHS Integer LHS value
 * @param RHS Float RHS value
 * @param op The opcode
 * 
 * @return Result
 * 
 * @throws Runtime Exception if the operation is not possible give the data types
 */
float calculate(const int LHS, const float RHS, const opcode_type op){ return calculate((float) LHS, RHS, op); }

/**
 * Calculate the folding results for integer-float variables
 * 
 * @param LHS Float LHS value
 * @param RHS Integer RHS value
 * @param op The opcode
 * 
 * @return Result
 * 
 * @throws Runtime Exception if the operation is not possible give the data types
 */
float calculate(const float LHS, const int RHS, const opcode_type op){ return calculate(LHS, (float) RHS, op); }





/* ================================================================== */
/* ========================= OPTIMIZATIONS ========================== */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* ----------------------- PHI-OPTIMIZATIONS ------------------------ */
/* ------------------------------------------------------------------ */

void remove_trivial_phi(function_id id){
    function *f = get_function(id);
    if (f->block == NULL) {
        return;
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

    new_opcode.size = 0;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
            case OPCODE_PHI: {
                std::unordered_set<uint64_t> phi_preds;
                for(int i = 0; i < o->op_phi.preds_size; i++){
                    phi_preds.insert(o->op_phi.preds[i]);
                }

                if(phi_preds.size() == 1){
                    create_assign_opcode(o->op_phi.to, *phi_preds.begin());
                    break;
                }
                else if(phi_preds.size() == 2 && phi_preds.count(o->op_phi.to)) {
                    uint64_t replacement = 0;
                    for (auto v : phi_preds) {
                        if (v != o->op_phi.to) replacement = v;
                    }
                    create_assign_opcode(o->op_phi.to, replacement);
                    break;
                }
                else {
                    create_phi_opcode(
                        o->op_phi.to, 
                        std::vector<uint64_t>(phi_preds.begin(), phi_preds.end())
                    );
                }
                break;
            }
            default:
                copy_opcode(o);
                break;
        }
		index += o->size;
    }

	f->code = new_opcode;
}





/* ------------------------------------------------------------------ */
/* --------------------- FOLDING-OPTIMIZATIONS ---------------------- */
/* ------------------------------------------------------------------ */

std::tuple<
    std::unordered_map<uint64_t, uint64_t>,
    std::unordered_map<uint64_t, constant_var>
> discover(function_id id)
{
    function *f = get_function(id);
    if (f->block == NULL) {
        kong_log(LOG_LEVEL_ERROR, "Trying to hit discover in NULL-block function");
        exit(1);
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

    std::unordered_map<uint64_t, uint64_t> replacement_map;
    std::unordered_map<uint64_t, constant_var> constants;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
            case OPCODE_ASSIGN:
                if(replacement_map.find(o->op_assign.from) != replacement_map.end()){
                    replacement_map[o->op_assign.to] = replacement_map[o->op_assign.from];
                }
                else {
                    replacement_map[o->op_assign.to] = o->op_assign.from;
                }
                break;
            case OPCODE_STORE_VARIABLE:
                if(replacement_map.find(o->op_store_var.from.index) != replacement_map.end()){
                    replacement_map[o->op_store_var.to.index] = replacement_map[o->op_store_var.from.index];
                }
                else {
                    replacement_map[o->op_store_var.to.index] = o->op_store_var.from.index;
                }
                break;
            case OPCODE_LOAD_INT_CONSTANT:
                constants[o->op_load_int_constant.to.index] = { .type = INTEGER, .integer = { .val = o->op_load_int_constant.number }};
                break;
            case OPCODE_LOAD_BOOL_CONSTANT:
                constants[o->op_load_bool_constant.to.index] = { .type = BOOLEAN, .boolean = { .val = o->op_load_bool_constant.boolean }};
                break;
            case OPCODE_LOAD_FLOAT_CONSTANT:
                constants[o->op_load_float_constant.to.index] = { .type = FLOAT, .floating = { .val = o->op_load_float_constant.number }};
                break;
            default:
                break;
        }
		index += o->size;
    }

    return {replacement_map, constants};
}


void constant_folding(function_id id, std::unordered_map<uint64_t, constant_var>& constants){
    function *f = get_function(id);
    if (f->block == NULL) {
        return;
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

    new_opcode.size = 0;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
            case OPCODE_NOT:
                if( constants.find(o->op_not.from.index) != constants.end() ){
                    if( constants[o->op_not.from.index].type == BOOLEAN ){
                        create_constant_bool(o->op_not.to, !constants[o->op_not.from.index].boolean.val);
                        constants[o->op_not.to.index] = { .type = BOOLEAN, .boolean = { .val = !constants[o->op_not.from.index].boolean.val }};
                    }
                    else if( constants[o->op_not.from.index].type == INTEGER && (
                                constants[o->op_not.from.index].integer.val == 1 ||
                                constants[o->op_not.from.index].integer.val == 0 ) 
                            )
                    {
                        create_constant_bool(o->op_not.to, constants[o->op_not.from.index].integer.val == 1 ? false : true);
                        constants[o->op_not.to.index] = { .type = BOOLEAN, .boolean = { .val = constants[o->op_not.from.index].integer.val == 1 ? false : true }};
                    }
                    else { copy_opcode(o); }
                } 
                else { copy_opcode(o); }
                break;
            case OPCODE_NEGATE:
                if( constants.find(o->op_negate.from.index) != constants.end() ){
                    if( constants[o->op_negate.from.index].type == INTEGER ){
                        create_constant_int(o->op_negate.to, -constants[o->op_not.from.index].integer.val);
                        constants[o->op_negate.to.index] = { .type = INTEGER, .integer = { .val = -constants[o->op_not.from.index].integer.val }};
                    }
                    else if( constants[o->op_negate.from.index].type == FLOAT ){
                        create_constant_float(o->op_negate.to, -constants[o->op_not.from.index].floating.val);
                        constants[o->op_negate.to.index] = { .type = FLOAT, .floating = { .val = -constants[o->op_not.from.index].floating.val }};
                    } 
                    else { copy_opcode(o); }
                } 
                else { copy_opcode(o); }
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
                if( constants.find(o->op_binary.left.index) != constants.end() &&
                    constants.find(o->op_binary.right.index) != constants.end() )
                {
                    if(compatible(constants[o->op_binary.left.index], constants[o->op_binary.right.index])){
                        if(constants[o->op_binary.left.index].type == BOOLEAN){
                            bool boolean_result = calculate(constants[o->op_binary.left.index].boolean.val, constants[o->op_binary.right.index].boolean.val, o->type);
                            create_constant_bool(o->op_binary.result, boolean_result);
                            constants[o->op_binary.result.index] = { .type = BOOLEAN, .boolean = { .val = boolean_result }};
                            break;
                        }
                        else if(constants[o->op_binary.left.index].type == INTEGER && constants[o->op_binary.right.index].type == INTEGER){
                            int integer_result = calculate(constants[o->op_binary.left.index].integer.val, constants[o->op_binary.right.index].integer.val, o->type);
                            create_constant_int(o->op_binary.result, integer_result);
                            constants[o->op_binary.result.index] = { .type = INTEGER, .integer = { .val = integer_result }};
                            break;
                        }
                        else{
                            float floating_result;
                            if(constants[o->op_binary.left.index].type == FLOAT && constants[o->op_binary.right.index].type == FLOAT)
                                floating_result = calculate(constants[o->op_binary.left.index].floating.val, constants[o->op_binary.right.index].floating.val, o->type);
                            if(constants[o->op_binary.left.index].type == INTEGER && constants[o->op_binary.right.index].type == FLOAT)
                                floating_result = calculate(constants[o->op_binary.left.index].integer.val, constants[o->op_binary.right.index].floating.val, o->type);
                            if(constants[o->op_binary.left.index].type == FLOAT && constants[o->op_binary.right.index].type == INTEGER)
                                floating_result = calculate(constants[o->op_binary.left.index].floating.val, constants[o->op_binary.right.index].integer.val, o->type);
                            create_constant_float(o->op_binary.result, floating_result);
                            constants[o->op_binary.result.index] = { .type = FLOAT, .floating = { .val = floating_result }};
                            break;
                        }
                    } 
                    else { copy_opcode(o); }
                }
                else { copy_opcode(o); }
                break;
            default:
                copy_opcode(o);
                break;
        }
		index += o->size;
    }

    f->code = new_opcode;
}


void copy_propagation(function_id id, std::unordered_map<uint64_t, uint64_t>& replacement_map){
    function *f = get_function(id);
    if (f->block == NULL) {
        return;
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

    new_opcode.size = 0;

	size_t index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
            case OPCODE_VAR:
            case OPCODE_ASSIGN:
                // DO NOT COPY -> Same as OPCODE_STORE_VARIABLE INTERNAl
                break;
            case OPCODE_STORE_VARIABLE:
                // VARIABLE_INTERNAL always at internal, LOCAL and GLOBAL for args/ outside vars possible, therefore choosing this
                if(replacement_map.find(o->op_store_var.from.index) != replacement_map.end()){
                    o->op_store_var.from.index = replacement_map[o->op_store_var.from.index];
                }
                // INTERNAL shall be removed, since they will be replaced during copy-propagation anyways
                if(o->op_store_var.from.kind != VARIABLE_INTERNAL) {
                    copy_opcode(o);
                }
                break;
            case OPCODE_LOAD_ACCESS_LIST:
                // TODO: Structs etc using Access Lists for now kept in Memory
                copy_opcode(o);
                break;
            case OPCODE_STORE_ACCESS_LIST:
            case OPCODE_SUB_AND_STORE_ACCESS_LIST:
            case OPCODE_ADD_AND_STORE_ACCESS_LIST:
            case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
                if(replacement_map.find(o->op_store_access_list.from.index) != replacement_map.end()){
                    o->op_store_access_list.from.index = replacement_map[o->op_store_access_list.from.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_NOT:
                if(replacement_map.find(o->op_not.from.index) != replacement_map.end()){
                    o->op_not.from.index = replacement_map[o->op_not.from.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_NEGATE:
                if(replacement_map.find(o->op_negate.from.index) != replacement_map.end()){
                    o->op_negate.from.index = replacement_map[o->op_negate.from.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_RETURN:
                if(replacement_map.find(o->op_return.var.index) != replacement_map.end()){
                    o->op_return.var.index = replacement_map[o->op_return.var.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_CALL:
                for(int i = 0; i < o->op_call.parameters_size; ++i){
                    if(replacement_map.find(o->op_call.parameters[i].index) != replacement_map.end()){
                        o->op_call.parameters[i].index = replacement_map[o->op_call.parameters[i].index];
                    }
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
                if(replacement_map.find(o->op_binary.left.index) != replacement_map.end()){
                    o->op_binary.left.index = replacement_map[o->op_binary.left.index];
                }
                if(replacement_map.find(o->op_binary.right.index) != replacement_map.end()){
                    o->op_binary.right.index = replacement_map[o->op_binary.right.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_IF:
                if(replacement_map.find(o->op_if.condition.index) != replacement_map.end()){
                    o->op_if.condition.index = replacement_map[o->op_if.condition.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_WHILE_CONDITION:
                if(replacement_map.find(o->op_while.condition.index) != replacement_map.end()){
                    o->op_while.condition.index = replacement_map[o->op_while.condition.index];
                }
                copy_opcode(o);
                break;
            case OPCODE_PHI:
                for(int i = 0; i < o->op_phi.preds_size; ++i){
                    if(replacement_map.find(o->op_phi.preds[i]) != replacement_map.end()){
                        o->op_phi.preds[i] = replacement_map[o->op_phi.preds[i]];
                    }
                }
                copy_opcode(o);
                break;
            default:
                copy_opcode(o);
                break;
        }
		index += o->size;
    }

    f->code = new_opcode;
}





/* ------------------------------------------------------------------ */
/* --------------------- DEAD CODE ELIMINATION ---------------------- */
/* ------------------------------------------------------------------ */

void local_dead_code_elimination(function_id id){
    function *f = get_function(id);
    if (f->block == NULL) {
        return;
	}

	uint8_t *data = f->code.o;
	size_t   size = f->code.size;

    /* --------------- PASS 1 : DEAD CODE DISCOVERY ---------------  */

    std::unordered_set<uint64_t> dead_variables;
    size_t index;

    for(int i = 0; i < 2 ; i++){
        size_t index = 0;
        while (index < size) {
            opcode *o = (opcode *)&data[index];
            switch (o->type) {
                case OPCODE_ASSIGN:
                    if(i == 0) dead_variables.insert(o->op_assign.to);
                    else dead_variables.erase(o->op_assign.from);
                    break;
                case OPCODE_STORE_VARIABLE:
                case OPCODE_SUB_AND_STORE_VARIABLE:
                case OPCODE_ADD_AND_STORE_VARIABLE:
                case OPCODE_DIVIDE_AND_STORE_VARIABLE:
                case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                    if(i == 0) dead_variables.insert(o->op_store_var.to.index);
                    else dead_variables.erase(o->op_store_var.from.index);
                    break;
                case OPCODE_STORE_ACCESS_LIST:
                case OPCODE_SUB_AND_STORE_ACCESS_LIST:
                case OPCODE_ADD_AND_STORE_ACCESS_LIST:
                case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
                case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
                    if(i == 0){} // TODO: Structs etc using Access Lists for now kept in Memory
                    else dead_variables.erase(o->op_store_access_list.from.index);
                    break;
                case OPCODE_LOAD_ACCESS_LIST:
                    if(i == 0) dead_variables.insert(o->op_load_access_list.to.index);
                    else if(o->op_load_access_list.from.kind != VARIABLE_GLOBAL) dead_variables.erase(o->op_load_access_list.from.index);
                    break;
                case OPCODE_NOT:
                    if(i == 0) dead_variables.insert(o->op_not.to.index);
                    else dead_variables.erase(o->op_not.from.index);
                    break;
                case OPCODE_NEGATE:
                    if(i == 0) dead_variables.insert(o->op_negate.to.kind);
                    else dead_variables.erase(o->op_negate.from.index);
                    break;
                case OPCODE_CALL:
                    if(i == 0) dead_variables.insert(o->op_call.var.index);
                    else {
                        for(int p = 0; p < o->op_call.parameters_size; ++p) dead_variables.erase(o->op_call.parameters[p].index);
                    }                    
                    break;
                case OPCODE_RETURN:
                    if(i == 1) dead_variables.erase(o->op_return.var.index);
                    break;
                case OPCODE_LOAD_INT_CONSTANT:
                    if(i == 0) dead_variables.insert(o->op_load_int_constant.to.index);
                    break;
                case OPCODE_LOAD_FLOAT_CONSTANT:
                    if(i == 0) dead_variables.insert(o->op_load_float_constant.to.index);
                    break;
                case OPCODE_LOAD_BOOL_CONSTANT:
                    if(i == 0) dead_variables.insert(o->op_load_bool_constant.to.index);
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
                    if(i == 0) dead_variables.insert(o->op_binary.result.index);
                    else{
                        dead_variables.erase(o->op_binary.left.index);
                        dead_variables.erase(o->op_binary.right.index);
                    }
                    break;
                case OPCODE_IF:
                    if(i == 1) dead_variables.erase(o->op_if.condition.index);
                    break;
                case OPCODE_WHILE_CONDITION:
                    if(i == 1) dead_variables.erase(o->op_while.condition.index);
                    break;
                case OPCODE_PHI:
                    if(i == 0) dead_variables.insert(o->op_phi.to);
                    else {
                        for(int i = 0; i < o->op_phi.preds_size; ++i){
                            dead_variables.erase(o->op_phi.preds[i]);
                        }
                    }
                    break;
                default:
                    break;
            }
            index += o->size;
        }
    }

    /* --------------- PASS 2 : DEAD CODE ELIMINATION ---------------  */

    new_opcode.size = 0;

	index = 0;
	while (index < size) {
		opcode *o = (opcode *)&data[index];
		switch (o->type) {
            case OPCODE_VAR: break;
            case OPCODE_ASSIGN:
                if(dead_variables.find(o->op_assign.to) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_STORE_VARIABLE:
            case OPCODE_SUB_AND_STORE_VARIABLE:
            case OPCODE_ADD_AND_STORE_VARIABLE:
            case OPCODE_DIVIDE_AND_STORE_VARIABLE:
            case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
                if(dead_variables.find(o->op_store_var.to.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_STORE_ACCESS_LIST:
            case OPCODE_SUB_AND_STORE_ACCESS_LIST:
            case OPCODE_ADD_AND_STORE_ACCESS_LIST:
            case OPCODE_DIVIDE_AND_STORE_ACCESS_LIST:
            case OPCODE_MULTIPLY_AND_STORE_ACCESS_LIST:
                // TODO: Structs etc using Access Lists for now kept in Memory
                copy_opcode(o);
                break; 
            case OPCODE_LOAD_ACCESS_LIST:
                if(dead_variables.find(o->op_load_access_list.to.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_NOT:
                if(dead_variables.find(o->op_not.to.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_NEGATE:
                if(dead_variables.find(o->op_negate.to.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_CALL:
                if(dead_variables.find(o->op_call.var.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_LOAD_INT_CONSTANT:
                if(dead_variables.find(o->op_load_int_constant.to.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_LOAD_FLOAT_CONSTANT:
                if(dead_variables.find(o->op_load_float_constant.to.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_LOAD_BOOL_CONSTANT:
                if(dead_variables.find(o->op_load_bool_constant.to.index) == dead_variables.end()){
                    copy_opcode(o);
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
                if(dead_variables.find(o->op_binary.result.index) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            case OPCODE_PHI:
                if(dead_variables.find(o->op_phi.to) == dead_variables.end()){
                    copy_opcode(o);
                }
                break;
            default:
                copy_opcode(o);
                break;
        }
		index += o->size;
    }

    f->code = new_opcode;
}