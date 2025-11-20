// evaluator.h
#pragma once

#include "ast.h"
#include "scope.h"
#include "../base/base.h"

#include <map>
#include <vector>

namespace wio 
{
    class evaluator 
    {
    public:
        static void evaluate_program(ref<program> program_node, uint32_t id, packed_bool flags = {});
        static ref<variable_base> evaluate_single_expr(ref<expression> node);
    private:
        static ref<variable_base> evaluate_expression(ref<expression> node);
        static void evaluate_statement(ref<statement> node);
        static ref<variable_base> evaluate_declaration_statement(ref<statement> node);

        static ref<variable_base> evaluate_literal(ref<literal> node);
        static ref<variable_base> evaluate_string_literal(ref<string_literal> node);
        static ref<variable_base> evaluate_array_literal(ref<array_literal> node);
        static ref<variable_base> evaluate_dictionary_literal(ref<dictionary_literal> node);
        static ref<variable_base> evaluate_lambda_literal(ref<lambda_literal> node);
        static ref<variable_base> evaluate_typeof_expression(ref<typeof_expression> node);
        static ref<variable_base> evaluate_binary_expression(ref<binary_expression> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
        static ref<variable_base> evaluate_unary_expression(ref<unary_expression> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
        static ref<variable_base> evaluate_identifier(ref<identifier> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
        static ref<variable_base> evaluate_array_access_expression(ref<array_access_expression> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
        static ref<variable_base> evaluate_member_access_expression(ref<member_access_expression> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
        static ref<variable_base> evaluate_function_call(ref<function_call> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
        static ref<variable_base> evaluate_new_unit_instance_call(ref<new_unit_instance_call> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);

        static ref<scope> evaluate_block_statement(ref<block_statement> node);

        static void evaluate_expression_statement(ref<expression_statement> node);
        static void evaluate_if_statement(ref<if_statement> node);
        static void evaluate_for_statement(ref<for_statement> node);
        static void evaluate_foreach_statement(ref<foreach_statement> node);
        static void evaluate_while_statement(ref<while_statement> node);
        static void evaluate_break_statement(ref<break_statement> node);
        static void evaluate_continue_statement(ref<continue_statement> node);
        static void evaluate_import_statement(ref<import_statement> node);
        static void evaluate_return_statement(ref<return_statement> node);

        static ref<variable_base> evaluate_variable_declaration(ref<variable_declaration> node);
        static ref<variable_base> evaluate_array_declaration(ref<array_declaration> node);
        static ref<variable_base> evaluate_dictionary_declaration(ref<dictionary_declaration> node);
        static ref<variable_base> evaluate_function_definition(ref<function_definition> node);
        static ref<variable_base> evaluate_function_declaration(ref<function_declaration> node);
        static ref<variable_base> evaluate_lambda_declaration(ref<lambda_declaration> node);
        static ref<variable_base> evaluate_enum_declaration(ref<enum_declaration> node);
        static ref<variable_base> evaluate_realm_declaration(ref<realm_declaration> node);
        static ref<variable_base> evaluate_omni_declaration(ref<omni_declaration> node);
        static ref<variable_base> evaluate_parameter_declaration(ref<parameter_declaration> node);
        static ref<variable_base> evaluate_unit_declaration(ref<unit_declaration> node);
        static ref<variable_base> evaluate_unit_member_declaration(ref<unit_member_declaration> node);

        static ref<variable_base> as_value(ref<expression> node, ref<variable_base> object = nullptr, bool is_ref = false, uint32_t access_level = 0);
    };

} // namespace wio