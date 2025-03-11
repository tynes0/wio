#pragma once

#include "variables/variable.h"
#include "variables/function.h"

#include "base.h"
#include "token.h"

namespace wio
{
    enum class unary_operator_type
    {
        prefix,
        postfix
    };

    class ast_node : public std::enable_shared_from_this<ast_node>
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
        identifier(token tok);

        token_type get_type() const override { return token_type::identifier; }
        location get_location() const override { return m_token.loc; }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<variable_base> var_ref = nullptr;
        token m_token;
        variable_type m_type = variable_type::vt_null;
    };

    class program : public ast_node
    {
    public:
        program(std::vector<ref<statement>> statements) 
            : m_statements(statements) {}

        token_type get_type() const override { return token_type::end_of_file; }
        location get_location() const override;
        std::string to_string() const override;

        std::vector<ref<statement>> m_statements;
    };

    class string_literal : public literal
    {
    public:
        string_literal(token tok) 
            : literal(tok) {}

        token_type get_type() const override { return token_type::string; }
        std::string to_string() const override;
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
        assignment_expression(ref<identifier> target, token op, ref<expression> value)
            : m_target(target), m_operator(op), m_value(value) {}

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<identifier> m_target;
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
        array_access_expression(ref<expression> array, ref<expression> index) 
            : m_array(array), m_index(index) {}

        token_type get_type() const override { return token_type::left_bracket; }
        location get_location() const override { return m_array->get_location(); }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_array;
        ref<expression> m_index;
    };

    class dictionary_access_expression : public expression
    {
    public:
        dictionary_access_expression(ref<expression> dictionary, ref<expression> key) 
            : m_dictionary(dictionary), m_key(key) {}

        token_type get_type() const override { return token_type::left_bracket; }
        location get_location() const override { return m_dictionary->get_location(); }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_dictionary;
        ref<expression> m_key;
    };

    class member_access_expression : public expression
    {
    public:
        member_access_expression(ref<expression> object, ref<identifier> member)
            : m_object(object), m_member(member) {}

        token_type get_type() const override { return token_type::dot; } // "."
        location get_location() const override { return m_object->get_location(); }
        variable_type get_expression_type() const override;
        std::string to_string() const override;

        ref<expression> m_object;
        ref<identifier> m_member;
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
        variable_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global, variable_type type = variable_type::vt_null)
            : m_id(id), m_initializer(initializer), m_is_const(is_const), m_is_local(is_local), m_is_global(is_global),
            m_type(initializer && type == variable_type::vt_null ? initializer->get_expression_type() : type) {} // TODO

        token_type get_type() const override;
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        bool m_is_const;
        bool m_is_local;
        bool m_is_global;
        variable_type m_type;
    };

    class array_declaration : public statement
    {
    public:
        array_declaration(ref<identifier> id, std::vector<ref<expression>> elements, bool is_const, bool is_local, bool is_global)
            : m_id(id), m_elements(elements), m_is_const(is_const), m_is_local(is_local), m_is_global(is_global) {}

        token_type get_type() const override { return token_type::kw_array; }
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<ref<expression>> m_elements;
        bool m_is_const;
        bool m_is_local;
        bool m_is_global;
    };

    class dictionary_declaration : public statement
    {
    public:
        dictionary_declaration(ref<identifier> id, std::vector<std::pair<ref<expression>, ref<expression>>> pairs, bool is_const, bool is_local, bool is_global)
            : m_id(id), m_pairs(pairs), m_is_const(is_const), m_is_local(is_local), m_is_global(is_global) {}

        token_type get_type() const override { return token_type::kw_dict; }
        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<std::pair<ref<expression>, ref<expression>>> m_pairs;
        bool m_is_const;
        bool m_is_local;
        bool m_is_global;
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
} // namespace wio