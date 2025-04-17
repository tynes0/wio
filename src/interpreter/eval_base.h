#pragma once

#include <vector>
#include <map>

#include "scope.h"
#include "ast.h"
#include "../base/base.h"


namespace wio
{
	struct stmt_stack
	{
		std::vector<ref<statement>>* list = nullptr;
		ref<stmt_stack> parent;
	};

	class eval_base
	{
	public:
		static void initialize(packed_bool flags);
		static eval_base& get();

		void enter_scope(id_t id, scope_type type, id_t helper_id = 0);
		ref<scope> exit_scope(id_t id);

		void enter_statement_stack(std::vector<ref<statement>>* list);
		void exit_statement_stack();

		void enter_statement_stack_and_scope(id_t id, std::vector<ref<statement>>* list, scope_type type = scope_type::block);
		ref<scope> exit_statement_stack_and_scope(id_t id);

		symbol* lookup_member(id_t id, const std::string& name, ref<variable_base> object);
		symbol* lookup(id_t id, const std::string& name);
		symbol* lookup_current_and_global(id_t id, const std::string& name);

		void insert(id_t id, const std::string& name, const symbol& symbol);
		void insert_to_global(id_t id, const std::string& name, const symbol& symbol);

		ref<function_declaration> get_func_decl(const std::string& name, std::vector<function_param> real_parameters);

		ref<scope>& get_scope(id_t id) const;
		std::map<variable_type, ref<symbol_table>>& get_constant_object_members();
		ref<variable_base> get_last_return_value() const;
		ref<stmt_stack> get_statement_stack() const;

		void inc_loop_depth();
		void dec_loop_depth();
		int get_loop_depth() const;

		void inc_func_body_depth();
		void dec_func_body_depth();
		uint16_t get_func_body_depth() const;

		bool break_called() const;
		bool continue_called() const;
		bool return_called() const;
		bool is_only_global() const;
		bool in_func_call() const;

		bool is_sf() const;
		bool is_nb() const;

		void set_last_return_value(ref<variable_base> value);
		void set_loop_depth(int loop_depth);
		void set_func_body_depth(int fb_depth);
		void call_break(bool flag);
		void call_continue(bool flag);
		void call_return(bool flag);
		void set_only_global_state(bool flag);
		void set_func_call_state(bool flag);
	};
}