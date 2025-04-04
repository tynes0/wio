// evaluator.h
#pragma once

#include "ast.h"
#include "scope.h"
#include "../base/base.h"
#include "../utils/uuid.h"

#include <map>
#include <vector>

namespace wio 
{
    struct statement_stack
    {
        std::vector<ref<statement>>* list = nullptr;
        ref<statement_stack> parent;
    };

    class evaluator 
    {
    public:
        evaluator(packed_bool flags = {});

        void evaluate_program(ref<program> program_node);

        symbol_table_t& get_symbols();
    private:
        ref<variable_base> evaluate_expression(ref<expression> node);
        void evaluate_statement(ref<statement> node);

        ref<variable_base> evaluate_literal(ref<literal> node);
        ref<variable_base> evaluate_string_literal(ref<string_literal> node);
        ref<variable_base> evaluate_array_literal(ref<array_literal> node);
        ref<variable_base> evaluate_dictionary_literal(ref<dictionary_literal> node);
        ref<variable_base> evaluate_typeof_expression(ref<typeof_expression> node);
        ref<variable_base> evaluate_binary_expression(ref<binary_expression> node, ref<variable_base> object = nullptr, bool is_ref = false);
        ref<variable_base> evaluate_unary_expression(ref<unary_expression> node, ref<variable_base> object = nullptr, bool is_ref = false);
        ref<variable_base> evaluate_assignment_expression(ref<assignment_expression> node, ref<variable_base> object = nullptr, bool is_ref = false);
        ref<variable_base> evaluate_identifier(ref<identifier> node, ref<variable_base> object = nullptr, bool is_ref = false);
        ref<variable_base> evaluate_array_access_expression(ref<array_access_expression> node, ref<variable_base> object = nullptr, bool is_ref = false);
        ref<variable_base> evaluate_member_access_expression(ref<member_access_expression> node, ref<variable_base> object = nullptr, bool is_ref = false);
        ref<variable_base> evaluate_function_call(ref<function_call> node, ref<variable_base> object = nullptr, bool is_ref = false);

        ref<scope> evaluate_block_statement(ref<block_statement> node);

        void evaluate_expression_statement(ref<expression_statement> node);
        void evaluate_if_statement(ref<if_statement> node);
        void evaluate_for_statement(ref<for_statement> node);
        void evaluate_foreach_statement(ref<foreach_statement> node);
        void evaluate_while_statement(ref<while_statement> node);
        void evaluate_break_statement(ref<break_statement> node);
        void evaluate_continue_statement(ref<continue_statement> node);
        void evaluate_import_statement(ref<import_statement> node);
        void evaluate_return_statement(ref<return_statement> node);

        void evaluate_variable_declaration(ref<variable_declaration> node, bool is_parameter = false);
        void evaluate_array_declaration(ref<array_declaration> node);
        void evaluate_dictionary_declaration(ref<dictionary_declaration> node);
        void evaluate_function_declaration(ref<function_declaration> node);
        void evaluate_function_definition(ref<function_definition> node);
        void evaluate_realm_declaration(ref<realm_declaration> node);

        void evaluate_only_global_declarations();

        void enter_scope(scope_type type);
        ref<scope> exit_scope();
        void enter_statement_stack(std::vector<ref<statement>>* list);
        void exit_statement_stack();
        symbol* lookup_member(const std::string& name, ref<variable_base> object);
        symbol* lookup(const std::string& name);
        symbol* lookup_current_and_global(const std::string& name);
        ref<function_declaration> get_func_decl(const std::string& name, std::vector<function_param> real_parameters);
        ref<variable_base> get_null_var(variable_base_type vbt = variable_base_type::variable);
        ref<variable_base> get_value(ref<expression> node, ref<variable_base> object = nullptr, bool is_ref = false);

        ref<scope> m_current_scope;
        std::map<variable_type, ref<scope>> m_constant_object_members;

        ref<variable_base> m_last_return_value;
        ref<statement_stack> m_statement_stack;

        int m_loop_depth = 0;
        uint16_t m_func_body_counter = 0;

        packed_bool m_eval_flags{}; // 1- break, 2 - continue, 3 - return, 4 - only global decl, 5 - in func call
        packed_bool m_program_flags{}; // 1- single file 2- pure 3- realm 4- no builtin

    };

} // namespace wio