#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>

#include "token.h"
#include "base.h"
#include "variables/variable.h"
#include "variables/array.h"
#include "variables/dictionary.h"
#include "variables/function.h"


namespace wio
{
    class expression;
    class statement;
    class literal;
    class identifier;
    class program;
    class string_literal;
    class block_statement;
    class binary_expression;
    class unary_expression;
    class assignment_expression;
    class variable_declaration;
    class array_declaration;
    class dictionary_declaration;
    class function_declaration;
    class function_call;
    class expression_statement;
    class if_statement;
    class for_statement;
    class foreach_statement;
    class while_statement;
    class break_statement;
    class continue_statement;
    class return_statement;
    class import_statement;
    class typeof_expression;
    class array_access_expression;
    class dictionary_access_expression;
    class member_access_expression;

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
    };

    class statement : public ast_node
    {
    public:
        virtual ~statement() = default;
    };

    class literal : public expression
    {
    public:
        literal(token tok) : m_token(tok)
        {
            switch (tok.type)
            {
            case token_type::number:
                m_type = tok.value.find('.') != std::string::npos ? variable_type::vt_float : variable_type::vt_integer;
                break;
            case token_type::string:
                m_type = variable_type::vt_string;
                break;
            case token_type::character:
                m_type = variable_type::vt_character;
                break;
            case token_type::kw_true:
            case token_type::kw_false:
                m_type = variable_type::vt_bool;
                break;
            default:
                m_type = variable_type::vt_none;
            }
        }

        token_type get_type() const override { return m_token.type; }
        location get_location() const override { return m_token.loc; }
        variable_type get_expression_type() const override { return m_type; }

        std::string to_string() const override
        {
            return "Literal(" + m_token.value + ":" + frenum::to_string(m_type) + ")";
        }


        token m_token;
    private:
        variable_type m_type;
    };

    class identifier : public expression
    {
    public:
        identifier(token tok) : m_token(tok)
        {
            if (tok.type != token_type::identifier)
                throw unexpected_token_error("When creating identifier, the token type must be identifier!");
        }

        token_type get_type() const override { return token_type::identifier; }
        location get_location() const override { return m_token.loc; }
        variable_type get_expression_type() const override;

        std::string to_string() const override
        {
            return "Identifier(" + m_token.value + ")";
        }

        ref<variable_base> var_ref = nullptr;
        token m_token;
    };

    class program : public ast_node
    {
    public:
        program(std::vector<ref<statement>> statements) : m_statements(statements) {}

        token_type get_type() const override
        {
            return token_type::end_of_file;
        }
        location get_location() const override
        {
            if (m_statements.size() > 0)
                return m_statements[0]->get_location();

            return location{ 0, 0 };
        }
        std::string to_string() const override
        {
            std::string result;
            for (const auto& stmt : m_statements) {
                result += stmt->to_string() + "\n";
            }
            return result;
        }

        ref<program> accept() { return (std::static_pointer_cast<program>(shared_from_this())); }

        std::vector<ref<statement>> m_statements;
    };

    class string_literal : public literal
    {
    public:
        string_literal(token tok) : literal(tok) {}
        token_type get_type() const override {
            return token_type::string;
        }
        std::string to_string() const override {
            return m_token.value; // Doðrudan token deðerini döndür (týrnak iþaretleri dahil)
        }
    };

    class block_statement : public statement
    {
    public:
        block_statement(std::vector<ref<statement>> statements) : m_statements(statements) {}

        token_type get_type() const override { return token_type::left_curly_bracket; }
        location get_location() const override { return m_statements.empty() ? location{ 0, 0 } : m_statements.front()->get_location(); }

        std::string to_string() const override
        {
            std::string result = "{";
            for (const auto& stmt : m_statements)
                result += "\n    " + stmt->to_string();
            result += "\n}";
            return result;
        }

        std::vector<ref<statement>> m_statements;
    };

    class binary_expression : public expression
    {
    public:
        binary_expression(ref<expression> left, token op, ref<expression> right)
            : m_left(left), m_operator(op), m_right(right) {
        }

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;

        std::string to_string() const override
        {
            return "BinaryExpr(" + m_left->to_string() + " " + m_operator.value + " " + m_right->to_string() + ")";
        }

        ref<expression> m_left;
        token m_operator;
        ref<expression> m_right;
    };

    class unary_expression : public expression
    {
    public:
        unary_expression(token op, ref<expression> operand, unary_operator_type op_type = unary_operator_type::prefix)
            : m_operator(op), m_operand(operand), m_op_type(op_type) {
        }

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;

        std::string to_string() const override
        {
            if (m_op_type == unary_operator_type::prefix)
                return "UnaryExpr(" + m_operator.value + " " + m_operand->to_string() + ")";
            return "UnaryExpr(" + m_operand->to_string() + " " + m_operator.value + ")";
        }

        token m_operator;
        ref<expression> m_operand;
        unary_operator_type m_op_type;
    };

    class assignment_expression : public expression
    {
    public:
        assignment_expression(ref<identifier> target, token op, ref<expression> value)
            : m_target(target), m_operator(op), m_value(value) {
        }

        token_type get_type() const override { return m_operator.type; }
        location get_location() const override { return m_operator.loc; }
        variable_type get_expression_type() const override;

        std::string to_string() const override
        {
            return "AssignmentExpr(" + m_target->to_string() + " " + m_operator.value + " " + m_value->to_string() + ")";
        }

        ref<identifier> m_target;
        token m_operator;
        ref<expression> m_value;
    };

    class variable_declaration : public statement
    {
    public:
        variable_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global, variable_type type = variable_type::vt_none)
            : m_id(id), m_initializer(initializer), m_is_const(is_const), m_is_local(is_local), m_is_global(is_global), m_type(type) {
        }

        token_type get_type() const override
        {
            switch (m_type)
            {
            case wio::variable_type::vt_array:
                return token_type::kw_array;
            case wio::variable_type::vt_dictionary:
                return token_type::kw_dict;
            case wio::variable_type::vt_function:
                return token_type::kw_func;
            default:
                if (m_is_const)
                    return token_type::kw_const;
                return token_type::kw_var;
            }
        }
        location get_location() const override { return m_id->get_location(); }

        std::string to_string() const override
        {
            std::string result;
            if (m_is_local)
                result += "local ";
            else if (m_is_global)
                result += "global ";
            if (m_is_const)
                result += "const ";

            result += frenum::to_string(m_type) + " " + m_id->to_string();
            if (m_initializer)
                result += " = " + m_initializer->to_string();
            result += ";";
            return result;
        }

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
            : m_id(id), m_elements(elements), m_is_const(is_const), m_is_local(is_local), m_is_global(is_global) {
        }

        token_type get_type() const override { return token_type::kw_array; }
        location get_location() const override { return m_id->get_location(); }

        std::string to_string() const override
        {
            std::string result = (m_is_const ? "const " : "") + std::string("array ") + m_id->to_string() + " = [";
            for (size_t i = 0; i < m_elements.size(); ++i)
            {
                result += m_elements[i]->to_string();
                if (i < m_elements.size() - 1)
                    result += ", ";
            }
            result += "];";
            return result;
        }

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
            : m_id(id), m_pairs(pairs), m_is_const(is_const), m_is_local(is_local), m_is_global(is_global) {
        }

        token_type get_type() const override { return token_type::kw_dict; }
        location get_location() const override { return m_id->get_location(); }

        std::string to_string() const override
        {
            std::string result = (m_is_const ? "const " : "") + std::string("dict ") + m_id->to_string() + " = {";
            for (size_t i = 0; i < m_pairs.size(); ++i)
            {
                result += "[" + m_pairs[i].first->to_string() + ", ";
                result += m_pairs[i].second->to_string() + "]";
                if (i < m_pairs.size() - 1)
                    result += ", ";
            }
            result += "};";
            return result;
        }

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
            : m_id(id), m_params(params), m_body(body), m_return_type(return_type), m_is_local(is_local), m_is_global(is_global) {
        }

        std::string to_string() const override
        {
            std::string result = "func " + m_id->to_string() + "(";
            for (size_t i = 0; i < m_params.size(); ++i)
            {
                result += m_params[i]->to_string();

                if (i < m_params.size() - 1)
                    result += ", ";
            }
            result += ") " + m_body->to_string();
            return result;
        }

        token_type get_type() const override { return token_type::kw_func; }
        location get_location() const override { return m_id->get_location(); }
        variable_type get_return_type() const { return m_return_type; }

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
            : m_caller(caller), m_arguments(args) {
        }

        token_type get_type() const override { return token_type::identifier; }
        location get_location() const override { return m_caller->get_location(); }
        variable_type get_expression_type() const override;

        std::string to_string() const override {
            std::string result = m_caller->to_string() + "(";
            for (size_t i = 0; i < m_arguments.size(); ++i)
            {
                result += m_arguments[i]->to_string();
                if (i < m_arguments.size() - 1)
                    result += ", ";
            }
            result += ")";
            return result;
        }

        ref<expression> m_caller;
        std::vector<ref<expression>> m_arguments;
    };

    class expression_statement : public statement
    {
    public:
        expression_statement(ref<expression> expr) : m_expression(expr) {}

        token_type get_type() const override
        {
            return m_expression->get_type();
        }
        location get_location() const override { return m_expression->get_location(); }
        std::string to_string() const override {
            return m_expression->to_string() + ";";
        }

        ref<expression> m_expression;
    };

    class if_statement : public statement
    {
    public:
        if_statement(ref<expression> condition, ref<statement> then_branch, ref<statement> else_branch)
            : m_condition(condition), m_then_branch(then_branch), m_else_branch(else_branch) {
        }

        token_type get_type() const override { return token_type::kw_if; }
        location get_location() const override { return m_condition->get_location(); }

        std::string to_string() const override
        {
            std::string result = "if (" + m_condition->to_string() + ") " + m_then_branch->to_string();
            if (m_else_branch)
                result += " else " + m_else_branch->to_string();
            return result;
        }

        ref<expression> m_condition;
        ref<statement> m_then_branch;
        ref<statement> m_else_branch;
    };

    class for_statement : public statement
    {
    public:
        for_statement(ref<statement> initialization, ref<expression> condition, ref<expression> increment, ref<block_statement> body)
            : m_initialization(initialization), m_condition(condition), m_increment(increment), m_body(body) {
        }

        token_type get_type() const override { return token_type::kw_for; }
        location get_location() const override
        {
            if (m_initialization != nullptr)
                return m_initialization->get_location();
            else if (m_condition != nullptr)
                return m_condition->get_location();
            return m_body->get_location();
        }

        std::string to_string() const override
        {
            std::string result = "for (";
            if (m_initialization)
                result += m_initialization->to_string();
            result += "; ";
            if (m_condition)
                result += m_condition->to_string();
            result += "; ";
            if (m_increment)
                result += m_increment->to_string();
            result += ") " + m_body->to_string();
            return result;
        }

        ref<statement> m_initialization;
        ref<expression> m_condition;
        ref<expression> m_increment;
        ref<block_statement> m_body;
    };

    class foreach_statement : public statement
    {
    public:
        foreach_statement(ref<identifier> item, ref<expression> collection, ref<block_statement> body)
            :m_item(item), m_collection(collection), m_body(body)
        {
        }

        token_type get_type() const override { return token_type::kw_foreach; }
        location get_location() const override { return m_item->get_location(); }

        std::string to_string() const override
        {
            return "foreach (" + m_item->to_string() + " in " + m_collection->to_string() + ") " + m_body->to_string();
        }

        ref<identifier> m_item;
        ref<expression> m_collection;
        ref<block_statement> m_body;
    };

    class while_statement : public statement
    {
    public:
        while_statement(ref<expression> condition, ref<block_statement> body)
            : m_condition(condition), m_body(body) {
        }

        token_type get_type() const override { return token_type::kw_while; }
        location get_location() const override { return m_condition->get_location(); }

        std::string to_string() const override
        {
            return "while (" + m_condition->to_string() + ") " + m_body->to_string();
        }

        ref<expression> m_condition;
        ref<block_statement> m_body;
    };

    class break_statement : public statement
    {
    public:
        break_statement(location loc) : m_loc(loc) {}

        token_type get_type() const override { return token_type::kw_break; }
        location get_location() const override { return m_loc; }

        std::string to_string() const override
        {
            return "break;";
        }

        location m_loc;
    };

    class continue_statement : public statement
    {
    public:
        continue_statement(location loc) : m_loc(loc) {}

        token_type get_type() const override { return token_type::kw_continue; }
        location get_location() const override { return m_loc; }

        std::string to_string() const override
        {
            return "continue;";
        }

        location m_loc;
    };

    class return_statement : public statement
    {
    public:
        return_statement(location loc, ref<expression> value) : m_location(loc), m_value(value) {}

        token_type get_type() const override { return token_type::kw_return; }
        location get_location() const override { return m_location; }

        std::string to_string() const override
        {
            std::string result = "return";
            if (m_value)
                result += " " + m_value->to_string();
            result += ";";
            return result;
        }

        ref<expression> m_value;
        location m_location;
    };

    class import_statement : public statement
    {
    public:
        import_statement(location loc, std::string module_path) : m_location(loc), m_module_path(module_path) {}

        token_type get_type() const override { return token_type::kw_import; }
        location get_location() const override { return m_location; }

        std::string to_string() const override
        {
            return "import " + m_module_path + ";";
        }

        std::string m_module_path;
        location m_location;
    };

    class typeof_expression : public expression {
    public:
        typeof_expression(ref<expression> expr) : m_expr(expr) {}

        token_type get_type() const override { return token_type::kw_typeof; }
        location get_location() const override { return m_expr->get_location(); }
        variable_type get_expression_type() const override { return variable_type::vt_string; }

        std::string to_string() const override
        {
            return "typeof " + m_expr->to_string();
        }

        ref<expression> m_expr;
    };

    class array_access_expression : public expression
    {
    public:
        array_access_expression(ref<expression> array, ref<expression> index) : m_array(array), m_index(index) {}

        token_type get_type() const override { return token_type::left_bracket; }
        location get_location() const override { return m_array->get_location(); }
        variable_type get_expression_type() const override;

        std::string to_string() const override
        {
            return m_array->to_string() + "[" + m_index->to_string() + "]";
        }

        ref<expression> m_array;
        ref<expression> m_index;
    };

    class dictionary_access_expression : public expression
    {
    public:
        dictionary_access_expression(ref<expression> dictionary, ref<expression> key) : m_dictionary(dictionary), m_key(key) {}

        token_type get_type() const override { return token_type::left_bracket; }
        location get_location() const override { return m_dictionary->get_location(); }
        variable_type get_expression_type() const override;

        std::string to_string() const override
        {
            return m_dictionary->to_string() + "[" + m_key->to_string() + "]";
        }

        ref<expression> m_dictionary;
        ref<expression> m_key;
    };

    class member_access_expression : public expression
    {
    public:
        member_access_expression(ref<expression> object, ref<identifier> member)
            : m_object(object), m_member(member) {
        }

        token_type get_type() const override { return token_type::dot; } // "."
        location get_location() const override { return m_object->get_location(); }
        variable_type get_expression_type() const override;

        std::string to_string() const override {
            return m_object->to_string() + "." + m_member->to_string();
        }

        ref<expression> m_object;
        ref<identifier> m_member;
    };
} // namespace wio