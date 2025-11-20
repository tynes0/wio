#pragma once

#include <vector>
#include <map>

#include "scope.h"
#include "../variables/function_param.h"
#include "../variables/unit.h"
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
		void enter_this_scope(id_t id, ref<scope> this_scope);
		ref<scope> exit_scope(id_t id);

		void enter_statement_stack(std::vector<ref<statement>>* list);
		void exit_statement_stack();

		void enter_statement_stack_and_scope(id_t id, std::vector<ref<statement>>* list, scope_type type = scope_type::block);
		ref<scope> exit_statement_stack_and_scope(id_t id);

		symbol* search_member(id_t id, const std::string& name, ref<variable_base> object) const;
		symbol* search(id_t id, const std::string& name) const;
		symbol* search_current(id_t id, const std::string& name) const;

		std::pair<bool, symbol*> is_function_valid(id_t id, const std::string name, const std::vector<function_param>& parameters) const;

		void insert(id_t id, const std::string& name, const symbol& symbol);
		void insert_to_global(id_t id, const std::string& name, const symbol& symbol);

		ref<function_definition> get_func_definition(const std::string& name, std::vector<function_param> real_parameters);

		ref<scope>& get_scope(id_t id) const;
		std::map<variable_type, ref<symbol_table>>& get_constant_object_members();
		const std::vector<function_param>& get_last_parameters() const;
		ref<variable_base> get_last_return_value() const;
		ref<variable_base>* get_last_left_value() const;
		ref<stmt_stack> get_statement_stack() const;

		ref<unit> active_unit();
		void push_unit(ref<unit> p_unit);
		ref<unit> pop_unit();

		ref<unit_instance> last_unit_whose_member_function_is_called();
		void push_unit_whose_member_function_is_called(ref<unit_instance> p_unit_instance);
		ref<unit_instance> pop_unit_whose_member_function_is_called();

		void inc_loop_depth();
		void dec_loop_depth();
		int get_loop_depth() const;

		void inc_func_body_depth();
		void dec_func_body_depth();
		uint16_t get_func_body_depth() const;

		bool break_called() const;
		bool continue_called() const;
		bool return_called() const;
		bool is_ref_error_active() const;
		bool in_func_call() const;
		bool in_object() const;
		bool is_table_blocked() const;

		bool is_sf() const;
		bool is_nb() const;

		bool is_in_unit_decl() const;

		void set_last_return_value(ref<variable_base> value);
		void set_last_left_value(ref<variable_base>* value);
		void set_last_parameters(const std::vector<function_param>& parameters);
		void set_loop_depth(int loop_depth);
		void set_func_body_depth(int fb_depth);
		void call_break(bool flag);
		void call_continue(bool flag);
		void call_return(bool flag);
		void set_ref_error_activity(bool flag);
		void set_func_call_state(bool flag);
		void set_object_state(bool flag);
		void block_table(bool flag);
	};
}