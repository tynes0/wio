#pragma once

#include "../variables/variable.h"

#include "base.h"
#include "token.h"

namespace wio
{
    enum class unary_operator_type { prefix, postfix };
    enum class access_ref_type { value, lvalue };

    class ast_node;
    class expression;
    class statement;
    class literal;
    class string_literal;
    class array_literal;
    class dictionary_literal;
    class null_expression;
    class identifier;
    class program;
    class binary_expression;
    class unary_expression;
    class assignment_expression;
    class array_access_expression;
    class member_access_expression;
    class block_statement;
    class if_statement;
    class for_statement;
    class while_statement;
    class foreach_statement;
    class break_statement;
    class continue_statement;
    class return_statement;
    class import_statement;
    class typeof_expression;
    class variable_declaration;
    class array_declaration;
    class dictionary_declaration;
    class function_declaration;
    class function_call;
    class function_definition;

    class ast_node
    {
    public:
        virtual ~ast_node() = default;

        virtual token_type get_type() const = 0;
        virtual location get_location() const = 0;
        virtual std::string to_string() const = 0;
    };

    class expression : public ast_node
    {
    public:
        virtual ~expression() = default;

        virtual variable_type get_expression_type() const = 0;
        variable_type m_expression_type = variable_type::vt_null;
    };

    class statement : public ast_node
    {
    public:
        virtual ~statement() = default;
    };

    class literal : public expression
    {
    public:
        literal(token tok);

        token_type get_type() const override { return m_token.type; }
        location get_location() const override { return m_token.loc; }
        variable_type get_expression_type() const override { return m_type; }
        std::string to_string() const override;

        token m_token;
        variable_type m_type;
    };

    class string_literal : public literal
    {
    public:
        string_literal(token tok) 
            : literal(tok)
        {
            m_expression_type = variable_type::vt_string;
        }

        token_type get_type() const override { return token_type::string; }
        std::string to_string() const override;
    };

    class array_literal : public expression
    {
    public:
        array_literal(const std::vector<ref<expression>>& elements, location loc) : m_elements(elements), m_loc(loc) {}

        token_type get_type() const override { return token_type::kw_array; }
        std::string to_string() const override;
        variable_type get_expression_type() const override { return variable_type::vt_array; }
        location get_location() const override { return m_loc; }

        std::vector<ref<expression>> m_elements;
        location m_loc;
    };

    class dictionary_literal : public expression
    {
    public:
        dictionary_literal(const std::vector<std::pair<ref<expression>, ref<expression>>>& pairs, location loc) : m_pairs(pairs), m_loc(loc) {}

        token_type get_type() const override { return token_type::kw_dict; }
        std::string to_string() const override;
        variable_type get_expression_type() const override { return variable_type::vt_dictionary; }
        location get_location() const override { return m_loc; }

        std::vector<std::pair<ref<expression>, ref<expression>>> m_pairs;
        location m_loc;
    };

    class null_expression : public expression
    {
    public:
        null_expression(token tok);

        virtual token_type get_type() const override { return m_token.type; }
        virtual location get_location() const override { return m_token.loc; }
        variable_type get_expression_type() const override;
        virtual std::string to_string() const override;

        token m_token;
    };

    class identifier : public expression
    {
    public:
        identifier(token tok, bool is_ref = false, bool is_lhs = false);

        token_type get_type() const override { return token_type::identifier; }
        location get_location() const override { return m_token.loc; }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        token m_token;
        //ref<variable_base> var_ref = nullptr;
        variable_type m_type = variable_type::vt_null;
        bool m_is_ref;
        bool m_is_lhs;
    };

    class program : public ast_node
    {
    public:
        program(std::vector<ref<statement>> statements)
            : m_statements(statements) {
        }

        token_type get_type() const override { return token_type::end_of_file; }
        location get_location() const override;
        std::string to_string() const override;

        std::vector<ref<statement>> m_statements;
    };

    class binary_expression : public expression
    {
    public:
        binary_expression(ref<expression> left, token op, ref<expression> right) 
            : m_left(left), m_operator(op), m_right(right) {}

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_left;
        token m_operator;
        ref<expression> m_right;
    };

    class unary_expression : public expression
    {
    public:
        unary_expression(token op, ref<expression> operand, unary_operator_type op_type = unary_operator_type::prefix)
            : m_operator(op), m_operand(operand), m_op_type(op_type) {}

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        token m_operator;
        ref<expression> m_operand;
        unary_operator_type m_op_type;
    };

    class assignment_expression : public expression
    {
    public:
        assignment_expression(ref<expression> target, token op, ref<expression> value)
            : m_target(target), m_operator(op), m_value(value) {}

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_target;
        token m_operator;
        ref<expression> m_value;
    };

    class typeof_expression : public expression
    {
    public:
        typeof_expression(ref<expression> expr) 
            : m_expr(expr) {}

        token_type get_type() const override { return token_type::kw_typeof; }
        location get_location() const override { return m_expr->get_location(); }
        variable_type get_expression_type() const override { return variable_type::vt_string; }
        std::string to_string() const override;

        ref<expression> m_expr;
    };

    class array_access_expression : public expression
    {
    public:
        array_access_expression(ref<expression> array, ref<expression> index, bool is_ref = false, bool is_lhs = false)
            : m_array(array), m_key_or_index(index), m_is_ref(is_ref), m_is_lhs(is_lhs) {}

        token_type get_type() const override { return token_type::left_bracket; }
        location get_location() const override { return m_array->get_location(); }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_array;
        ref<expression> m_key_or_index;
        bool m_is_ref;
        bool m_is_lhs;
    };

    class member_access_expression : public expression
    {
    public:
        member_access_expression(ref<expression> object, ref<expression> member)
            : m_object(object), m_member(member) {}

        token_type get_type() const override { return token_type::dot; } // "."
        location get_location() const override { return m_object->get_location(); }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_object;
        ref<expression> m_member;
    };

    class block_statement : public statement
    {
    public:
        block_statement(std::vector<ref<statement>> statements) 
            : m_statements(statements) {}

        token_type get_type() const override { return token_type::left_curly_bracket; }
        location get_location() const override { return m_statements.empty() ? location{ 0, 0 } : m_statements.front()->get_location(); }
        std::string to_string() const override;

        std::vector<ref<statement>> m_statements;
    };

    class expression_statement : public statement
    {
    public:
        expression_statement(ref<expression> expr) 
            : m_expression(expr) {}

        token_type get_type() const override { return m_expression->get_type(); }
        location get_location() const override { return m_expression->get_location(); }
        std::string to_string() const override;

        ref<expression> m_expression;
    };

    class if_statement : public statement
    {
    public:
        if_statement(ref<expression> condition, ref<statement> then_branch, ref<statement> else_branch)
            : m_condition(condition), m_then_branch(then_branch), m_else_branch(else_branch) {}

        token_type get_type() const override { return token_type::kw_if; }
        location get_location() const override { return m_condition->get_location(); }
        std::string to_string() const override;

        ref<expression> m_condition;
        ref<statement> m_then_branch;
        ref<statement> m_else_branch;
    };

    class for_statement : public statement
    {
    public:
        for_statement(ref<statement> initialization, ref<expression> condition, ref<expression> increment, ref<block_statement> body)
            : m_initialization(initialization), m_condition(condition), m_increment(increment), m_body(body) {}

        token_type get_type() const override { return token_type::kw_for; }
        location get_location() const override;
        std::string to_string() const override;

        ref<statement> m_initialization;
        ref<expression> m_condition;
        ref<expression> m_increment;
        ref<block_statement> m_body;
    };

    class foreach_statement : public statement
    {
    public:
        foreach_statement(ref<identifier> item, ref<expression> collection, ref<block_statement> body)
            :m_item(item), m_collection(collection), m_body(body) {}

        token_type get_type() const override { return token_type::kw_foreach; }
        location get_location() const override { return m_item->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_item;
        ref<expression> m_collection;
        ref<block_statement> m_body;
    };

    class while_statement : public statement
    {
    public:
        while_statement(ref<expression> condition, ref<block_statement> body)
            : m_condition(condition), m_body(body) {}

        token_type get_type() const override { return token_type::kw_while; }
        location get_location() const override { return m_condition->get_location(); }
        std::string to_string() const override;

        ref<expression> m_condition;
        ref<block_statement> m_body;
    };

    class break_statement : public statement
    {
    public:
        break_statement(location loc) 
            : m_loc(loc) {}

        token_type get_type() const override { return token_type::kw_break; }
        location get_location() const override { return m_loc; }
        std::string to_string() const override;

        location m_loc;
    };

    class continue_statement : public statement
    {
    public:
        continue_statement(location loc) 
            : m_loc(loc) {}

        token_type get_type() const override { return token_type::kw_continue; }
        location get_location() const override { return m_loc; }
        std::string to_string() const override;

        location m_loc;
    };

    class return_statement : public statement
    {
    public:
        return_statement(location loc, ref<expression> value) 
            : m_location(loc), m_value(value) {}

        token_type get_type() const override { return token_type::kw_return; }
        location get_location() const override { return m_location; }
        std::string to_string() const override;

        ref<expression> m_value;
        location m_location;
    };

    class import_statement : public statement
    {
    public:
        import_statement(location loc, std::string module_path) 
            : m_location(loc), m_module_path(module_path) {}

        token_type get_type() const override { return token_type::kw_import; }
        location get_location() const override { return m_location; }
        std::string to_string() const override;

        std::string m_module_path;
        location m_location;
    };

    class variable_declaration : public statement
    {
    public:
        variable_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global, bool is_ref, variable_type type = variable_type::vt_null)
            : m_id(id), m_initializer(initializer), m_flags({is_const, is_local, is_global, is_ref}),
            m_type(initializer && type == variable_type::vt_null ? initializer->get_expression_type() : type) {} // TODO

        token_type get_type() const override;
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        packed_bool m_flags; // 1- const 2-local 3- global 4-ref
        variable_type m_type;
    };

    class array_declaration : public statement
    {
    public:
        array_declaration(ref<identifier> id, ref<expression> initializer, const std::vector<ref<expression>>& elements, bool is_const, bool is_local, bool is_global, bool is_element_initializer)
            : m_id(id), m_initializer(initializer), m_elements(elements), m_flags({is_const, is_local, is_global, is_element_initializer}) { }

        token_type get_type() const override { return token_type::kw_array; }
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        std::vector<ref<expression>> m_elements;
        packed_bool m_flags; // 1- const 2-local 3- global 4- is element initializer
    };

    class dictionary_declaration : public statement
    {
    public:
        dictionary_declaration(ref<identifier> id, ref<expression> initializer, const std::vector<std::pair<ref<expression>, ref<expression>>>& pairs, bool is_const, bool is_local, bool is_global, bool is_element_initializer)
            : m_id(id), m_initializer(initializer), m_pairs(pairs), m_flags({is_const, is_local, is_global, is_element_initializer}) { }

        token_type get_type() const override { return token_type::kw_dict; }
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        std::vector<std::pair<ref<expression>, ref<expression>>> m_pairs;
        packed_bool m_flags; // 1- const 2-local 3- global 4- is element initializer
    };

    class function_declaration : public statement
    {
    public:
        function_declaration(ref<identifier> id, std::vector<ref<variable_declaration>> params, ref<block_statement> body, variable_type return_type, bool is_local, bool is_global)
            : m_id(id), m_params(params), m_body(body), m_return_type(return_type), m_is_local(is_local), m_is_global(is_global) {}

        variable_type get_return_type() const { return m_return_type; }
        token_type get_type() const override { return token_type::kw_func; }
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<ref<variable_declaration>> m_params;
        ref<block_statement> m_body;
        variable_type m_return_type;
        bool m_is_local;
        bool m_is_global;
    };

    class function_call : public expression
    {
    public:
        function_call(ref<expression> caller, std::vector<ref<expression>> args)
            : m_caller(caller), m_arguments(args) {}

        token_type get_type() const override { return token_type::identifier; }
        location get_location() const override { return m_caller->get_location(); }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_caller;
        std::vector<ref<expression>> m_arguments;
    };

    class function_definition : public statement
    {
    public:
        function_definition(ref<identifier> id, std::vector<ref<variable_declaration>> params, bool is_local, bool is_global)
            : m_id(id), m_params(params), m_is_local(is_local), m_is_global(is_global) {
        }

        token_type get_type() const override { return token_type::kw_func; }
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<ref<variable_declaration>> m_params;
        bool m_is_local;
        bool m_is_global;
    };
} // namespace wio