#pragma once

#include "../variables/variable.h"

#include "../base/base.h"
#include "token.h"

namespace wio
{
    enum class unary_operator_type { prefix, postfix };
    enum class lit_type { lt_null, lt_string, lt_character, lt_bool, lt_binary, lt_octal, lt_decimal, lt_hexadeximal, lt_float };
    enum class unit_declaration_type { none, exposed, hidden, shared };

    MakeFrenumInNamespace(wio, lit_type, lt_null, lt_string, lt_character, lt_bool, lt_binary, lt_octal, lt_decimal, lt_hexadeximal, lt_float)

    class ast_node;
    class expression;
    class statement;
    class program;
    class literal;
    class string_literal;
    class array_literal;
    class dictionary_literal;
    class lambda_literal;
    class null_expression;
    class identifier;
    class binary_expression;
    class unary_expression;
    class array_access_expression;
    class member_access_expression;
    class function_call;
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
    class function_definition;
    class function_declaration;
    class lambda_declaration;
    class realm_declaration;
    class omni_declaration;
    class enum_declaration;
    class parameter_declaration;
    class unit_declaration;
    class unit_member_declaration;

    class ast_node
    {
    public:
        virtual ~ast_node() = default;

        virtual location get_location() const = 0;
        virtual std::string to_string() const = 0;
    };

    class expression : public ast_node
    {
    public:
        virtual ~expression() = default;
        virtual bool is_ref() const = 0;
    };

    class statement : public ast_node
    {
    public:
        static constexpr bool is_declaration = false;

        virtual ~statement() = default;
    };

    class program : public ast_node
    {
    public:
        program(std::vector<ref<statement>> statements)
            : m_statements(statements) {
        }

        location get_location() const override;
        std::string to_string() const override;

        std::vector<ref<statement>> m_statements;
    };

    class literal : public expression
    {
    public:
        literal(token tok);

        location get_location() const override { return m_token.loc; }
        lit_type get_literal_type() const { return m_type; }
        bool is_ref() const override { return false; }
        std::string to_string() const override;

        token m_token;
        lit_type m_type;
    };

    class string_literal : public literal
    {
    public:
        string_literal(token tok) 
            : literal(tok)
        {
            m_type = lit_type::lt_string;
        }

        std::string to_string() const override;
    };

    class array_literal : public expression
    {
    public:
        array_literal(const std::vector<ref<expression>>& elements, location loc) : m_elements(elements), m_loc(loc) {}

        std::string to_string() const override;
        location get_location() const override { return m_loc; }
        bool is_ref() const override { return false; }

        std::vector<ref<expression>> m_elements;
        location m_loc;
    };

    class dictionary_literal : public expression
    {
    public:
        dictionary_literal(const std::vector<std::pair<ref<expression>, ref<expression>>>& pairs, location loc) : m_pairs(pairs), m_loc(loc) {}

        std::string to_string() const override;
        location get_location() const override { return m_loc; }
        bool is_ref() const override { return false; }

        std::vector<std::pair<ref<expression>, ref<expression>>> m_pairs;
        location m_loc;
    };

    class lambda_literal : public expression
    {
    public:
        lambda_literal(const std::vector<ref<parameter_declaration>>& params, ref<block_statement> body, location loc) 
            : m_params(params), m_body(body), m_loc(loc) {}

        std::string to_string() const override;
        location get_location() const override { return m_loc; }
        bool is_ref() const override { return false; }

        std::vector<ref<parameter_declaration>> m_params;
        ref<block_statement> m_body;
        location m_loc;
    };

    class null_expression : public expression
    {
    public:
        null_expression(token tok);

        virtual location get_location() const override { return m_token.loc; }
        virtual std::string to_string() const override;
        bool is_ref() const override { return false; }

        token m_token;
    };

    class identifier : public expression
    {
    public:
        identifier(token tok, bool is_ref = false, bool is_lhs = false);

        location get_location() const override { return m_token.loc; }
        std::string to_string() const override;
        bool is_ref() const override { return m_is_ref; }

        token m_token;
        variable_type m_type = variable_type::vt_null;
        bool m_is_ref;
        bool m_is_lhs;
    };

    class binary_expression : public expression
    {
    public:
        binary_expression(ref<expression> left, token op, ref<expression> right) 
            : m_left(left), m_operator(op), m_right(right) {}

        location get_location() const override { return m_operator.loc; }
        std::string to_string() const override;
        bool is_ref() const override { return m_left->is_ref(); }

        bool is_assignment = false;
        ref<expression> m_left;
        token m_operator;
        ref<expression> m_right;
    };

    class unary_expression : public expression
    {
    public:
        unary_expression(token op, ref<expression> operand, unary_operator_type op_type = unary_operator_type::prefix, bool is_ref = false)
            : m_operator(op), m_operand(operand), m_op_type(op_type), m_is_ref(is_ref) {}

        location get_location() const override { return m_operator.loc; }
        std::string to_string() const override;
        bool is_ref() const override { return m_is_ref; }

        token m_operator;
        ref<expression> m_operand;
        unary_operator_type m_op_type;
        bool m_is_ref;
    };

    class typeof_expression : public expression
    {
    public:
        typeof_expression(ref<expression> expr) 
            : m_expr(expr) {}

        location get_location() const override { return m_expr->get_location(); }
        std::string to_string() const override;
        bool is_ref() const override { return false; }

        ref<expression> m_expr;
    };

    class array_access_expression : public expression
    {
    public:
        array_access_expression(ref<expression> array, ref<expression> index, bool is_ref = false, bool is_lhs = false)
            : m_array(array), m_key_or_index(index), m_is_ref(is_ref), m_is_lhs(is_lhs) {}

        location get_location() const override { return m_array->get_location(); }
        std::string to_string() const override;
        bool is_ref() const override { return m_is_ref; }

        ref<expression> m_array;
        ref<expression> m_key_or_index;
        bool m_is_ref;
        bool m_is_lhs;
    };

    class member_access_expression : public expression
    {
    public:
        member_access_expression(ref<expression> object, ref<expression> member, bool is_ref = false, bool is_lhs = false)
            : m_object(object), m_member(member), m_is_ref(is_ref), m_is_lhs(is_lhs) {}

        location get_location() const override { return m_object->get_location(); }
        std::string to_string() const override;
        bool is_ref() const override { return m_is_ref; }

        ref<expression> m_object;
        ref<expression> m_member;
        bool m_is_ref;
        bool m_is_lhs;
    };

    class function_call : public expression
    {
    public:
        function_call(ref<expression> caller, std::vector<ref<expression>> args)
            : m_caller(caller), m_arguments(args) {
        }

        location get_location() const override { return m_caller->get_location(); }
        std::string to_string() const override;
        bool is_ref() const override { return false; }

        ref<expression> m_caller;
        std::vector<ref<expression>> m_arguments;
    };

    class block_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        block_statement(std::vector<ref<statement>> statements) 
            : m_statements(statements) {}

        location get_location() const override { return m_statements.empty() ? location{ 0, 0 } : m_statements.front()->get_location(); }
        std::string to_string() const override;

        std::vector<ref<statement>> m_statements;
    };

    class expression_statement : public statement
    {
    public:
        expression_statement(ref<expression> expr) 
            : m_expression(expr) {}

        location get_location() const override { return m_expression->get_location(); }
        std::string to_string() const override;

        ref<expression> m_expression;
    };

    class if_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        if_statement(ref<expression> condition, ref<statement> then_branch, ref<statement> else_branch)
            : m_condition(condition), m_then_branch(then_branch), m_else_branch(else_branch) {}

        location get_location() const override { return m_condition->get_location(); }
        std::string to_string() const override;

        ref<expression> m_condition;
        ref<statement> m_then_branch;
        ref<statement> m_else_branch;
    };

    class for_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        for_statement(ref<statement> initialization, ref<expression> condition, ref<expression> increment, ref<block_statement> body)
            : m_initialization(initialization), m_condition(condition), m_increment(increment), m_body(body) {}

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
        static constexpr bool is_declaration = false;

        foreach_statement(ref<identifier> item, ref<expression> collection, ref<block_statement> body)
            :m_item(item), m_collection(collection), m_body(body) {}

        location get_location() const override { return m_item->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_item;
        ref<expression> m_collection;
        ref<block_statement> m_body;
    };

    class while_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        while_statement(ref<expression> condition, ref<block_statement> body)
            : m_condition(condition), m_body(body) {}

        location get_location() const override { return m_condition->get_location(); }
        std::string to_string() const override;

        ref<expression> m_condition;
        ref<block_statement> m_body;
    };

    class break_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        break_statement(location loc) 
            : m_loc(loc) {}

        location get_location() const override { return m_loc; }
        std::string to_string() const override;

        location m_loc;
    };

    class continue_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        continue_statement(location loc) 
            : m_loc(loc) {}

        location get_location() const override { return m_loc; }
        std::string to_string() const override;

        location m_loc;
    };

    class return_statement : public statement
    {
    public:
        return_statement(location loc, ref<expression> value) 
            : m_location(loc), m_value(value) {}

        location get_location() const override { return m_location; }
        std::string to_string() const override;

        ref<expression> m_value;
        location m_location;
    };

    class import_statement : public statement
    {
    public:
        static constexpr bool is_declaration = false;

        import_statement(location loc, std::string module_path, bool is_pure = false, bool is_realm = false, ref<identifier> realm_id = nullptr)
            : m_location(loc), m_module_path(module_path), m_flags({ is_pure, is_realm }), m_realm_id(realm_id) {}

        location get_location() const override { return m_location; }
        std::string to_string() const override;

        std::string m_module_path;
        location m_location;
        ref<identifier> m_realm_id;
        packed_bool m_flags; // 1- is_pure 2-is_realm
    };

    class variable_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        variable_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global)
            : m_id(id), m_initializer(initializer), m_flags({is_const, is_local, is_global}) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        packed_bool m_flags; // 1- const 2-local 3- global
    };

    class array_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        array_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global)
            : m_id(id), m_initializer(initializer), m_flags({is_const, is_local, is_global}) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        packed_bool m_flags; // 1- const 2-local 3- global
    };

    class dictionary_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        dictionary_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global)
            : m_id(id), m_initializer(initializer), m_flags({is_const, is_local, is_global}) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        packed_bool m_flags; // 1- const 2-local 3- global
    };

    class function_definition : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        function_definition(ref<identifier> id, std::vector<ref<parameter_declaration>> params, ref<block_statement> body, bool is_local, bool is_global)
            : m_id(id), m_params(params), m_body(body), m_is_local(is_local), m_is_global(is_global) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<ref<parameter_declaration>> m_params;
        ref<block_statement> m_body;
        bool m_is_local;
        bool m_is_global;
    };

    class function_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        function_declaration(ref<identifier> id, std::vector<ref<parameter_declaration>> params, bool is_local, bool is_global)
            : m_id(id), m_params(params), m_is_local(is_local), m_is_global(is_global) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<ref<parameter_declaration>> m_params;
        bool m_is_local;
        bool m_is_global;
    };

    class lambda_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        lambda_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global, variable_type type = variable_type::vt_null)
            : m_id(id), m_initializer(initializer), m_flags({ is_const, is_local, is_global }) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        packed_bool m_flags; // 1- const 2-local 3- global
    };

    class realm_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        realm_declaration(ref<identifier> id, ref<block_statement> body, bool is_local, bool is_global)
            :m_id(id), m_body(body), m_is_local(is_local), m_is_global(is_global) {
        }

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<block_statement> m_body;
        bool m_is_local;
        bool m_is_global;
    };

    class omni_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        omni_declaration(ref<identifier> id, ref<expression> initializer, bool is_const, bool is_local, bool is_global)
            : m_id(id), m_initializer(initializer), m_flags({ is_const, is_local, is_global }) {
        }

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<expression> m_initializer;
        packed_bool m_flags; // 1- const 2-local 3- global
    };

    class enum_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        enum_declaration(ref<identifier> id, const std::vector<std::pair<ref<identifier>, ref<expression>>>& items, bool is_local, bool is_global)
            : m_id(id), m_items(items), m_is_local(is_local), m_is_global(is_global) {}

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        std::vector<std::pair<ref<identifier>, ref<expression>>> m_items;
        bool m_is_local;
        bool m_is_global;
    };

    class parameter_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = false; // why am i calling parameter_declaration then?

        parameter_declaration(ref<identifier> id, bool is_ref, variable_base_type type)
            : m_id(id), m_is_ref(is_ref), m_type(type) {
        }

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        variable_base_type m_type;
        bool m_is_ref;
    };

    class unit_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = true;

        unit_declaration(ref<identifier> id, ref<block_statement> body, bool is_local, bool is_global, const std::vector<ref<identifier>>& parent_list,
            const std::vector<ref<identifier>>& trust_list, bool is_final = false, unit_declaration_type default_decl = unit_declaration_type::hidden)
            : m_id(id), m_body(body), m_parent_list(parent_list), m_trust_list(trust_list), m_default_decl(default_decl), m_flags({ is_final, is_local, is_global }) {
        }

        location get_location() const override { return m_id->get_location(); }
        std::string to_string() const override;

        ref<identifier> m_id;
        ref<block_statement> m_body;
        std::vector<ref<identifier>> m_parent_list;
        std::vector<ref<identifier>> m_trust_list;
        unit_declaration_type m_default_decl;
        packed_bool m_flags; // 1- final 2- local 3- global
    };

    class unit_member_declaration : public statement
    {
    public:
        static constexpr bool is_declaration = false; // bcs m_decl_statement could be decl not unit_member_declaration!

        unit_member_declaration(ref<statement> decl_statement, unit_declaration_type decl_type, bool is_outer)
            : m_decl_statement(decl_statement), m_decl_type(decl_type), m_is_outer(is_outer) {
        }

        location get_location() const override { return m_decl_statement->get_location(); }
        std::string to_string() const override;

        ref<statement> m_decl_statement;
        unit_declaration_type m_decl_type;
        bool m_is_outer;
    };

} // namespace wio