// evaluator.h
#pragma once

#include "ast.h"
#include "scope.h"  // Artýk scope.h'ý include ediyoruz
#include "base.h"   // ref için
#include <memory>
#include <set>

namespace wio {

    // Forward declarations (AST düðümleri) - Gerekli
    class ast_node;
    class expression;
    class statement;
    class literal;
    class identifier;
    class binary_expression;
    class unary_expression;
    class assignment_expression;
    class variable_declaration;
    class array_declaration;
    class dictionary_declaration;
    class function_declaration;
    class function_call;
    class if_statement;
    class for_statement;
    class foreach_statement;
    class while_statement;
    class break_statement;
    class continue_statement;
    class return_statement;
    class import_statement;
    class typeof_expression;
    class block_statement;
    class array_access_expression;
    class dictionary_access_expression;
    class member_access_expression;
    class program;
    class string_literal;

    class evaluator {
    public:
        evaluator();

        void evaluate_program(ref<program> program_node);

    private:
        void preprocess_declarations(ref<ast_node> node);

        ref<variable_base> evaluate_expression(ref<expression> node);
        void evaluate_statement(ref<statement> node);

        ref<variable_base> evaluate_literal(ref<literal> node);
        ref<variable_base> evaluate_string_literal(ref<string_literal> node);
        ref<variable_base> evaluate_identifier(ref<identifier> node);
        ref<variable_base> evaluate_binary_expression(ref<binary_expression> node);
        ref<variable_base> evaluate_unary_expression(ref<unary_expression> node);
        ref<variable_base> evaluate_assignment_expression(ref<assignment_expression> node);
        ref<variable_base> evaluate_typeof_expression(ref<typeof_expression> node);
        ref<variable_base> evaluate_array_access_expression(ref<array_access_expression> node) { return make_ref<variable>(any(), variable_type::vt_null); }
        ref<variable_base> evaluate_dictionary_access_expression(ref<dictionary_access_expression> node) { return make_ref<variable>(any(), variable_type::vt_null); }
        ref<variable_base> evaluate_member_access_expression(ref<member_access_expression> node) { return make_ref<variable>(any(), variable_type::vt_null); }
        ref<variable_base> evaluate_function_call(ref<function_call> node) { return make_ref<variable>(any(), variable_type::vt_null); }

        void evaluate_block_statement(ref<block_statement> node) {}
        void evaluate_expression_statement(ref<expression_statement> node);
        void evaluate_if_statement(ref<if_statement> node) {}
        void evaluate_for_statement(ref<for_statement> node){}
        void evaluate_foreach_statement(ref<foreach_statement> node){}
        void evaluate_while_statement(ref<while_statement> node){}
        void evaluate_break_statement(ref<break_statement> node){}
        void evaluate_continue_statement(ref<continue_statement> node){}
        void evaluate_return_statement(ref<return_statement> node){}
        void evaluate_import_statement(ref<import_statement> node); // Þimdilik boþ kalabilir

        void evaluate_variable_declaration(ref<variable_declaration> node);
        void evaluate_array_declaration(ref<array_declaration> node);
        void evaluate_dictionary_declaration(ref<dictionary_declaration> node);
        void evaluate_function_declaration(ref<function_declaration> node);

        void enter_scope(scope_type type);
        void exit_scope();
        symbol* lookup(const std::string& name);

        ref<scope> current_scope;
        ref<function_declaration> current_function = nullptr;
        int m_loop_depth = 0;
        std::set<std::string> m_imported_modules;

        ref<variable_base> get_value(ref<expression>);
        variable_type get_expression_type(ref<expression> node);
    };

} // namespace wio