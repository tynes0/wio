#include "eval_base.h"

#include "../builtin/builtin_manager.h"
#include "../builtin/internal.h"
#include "main_table.h"

#include <stack>

namespace wio
{
	struct eval_data
	{
		std::map<variable_type, ref<symbol_table>> constant_object_members;
		std::stack<id_t> func_eval_ids;
		std::vector<function_param> last_parameters;
		ref<variable_base> last_return_value;
		ref<variable_base>* last_left_value = nullptr;
		ref<stmt_stack> statement_stack;
		int loop_depth = 0;
		uint16_t func_body_depth = 0;
		packed_bool eval_flags{}; // 1- break, 2 - continue, 3 - return, 4 - ref error active, 5 - in func call 6- search only function
		packed_bool program_flags{}; // 1- single file 2- no builtin
	};

	static eval_base s_base;
	static eval_data s_data;

	void eval_base::initialize(packed_bool flags)
	{
		builtin_manager::load(s_data.constant_object_members);
		builtin::internal::load();

		s_data.program_flags = flags;
		s_data.eval_flags.b4 = true;
	}

	eval_base& eval_base::get()
	{
		return s_base;
	}

	void eval_base::enter_scope(id_t id, scope_type type, id_t helper_id)
	{
		if (type == scope_type::function_body)
		{
			++s_data.func_body_depth;
			set_func_call_state(true);
			s_data.func_eval_ids.push(helper_id);
		}

		main_table::get().enter_scope(id, type);
	}

	ref<scope> eval_base::exit_scope(id_t id)
	{
		if (main_table::get().get_scope(id)->get_type() == scope_type::global)
			return nullptr;
		if (main_table::get().get_scope(id)->get_type() == scope_type::function_body)
		{
			if (s_data.func_body_depth)
			{
				s_data.func_eval_ids.pop();
				s_data.func_body_depth--;
				if (!s_data.func_body_depth)
					set_func_call_state(false);
			}
			else
			{
				throw exception("Unexpected error!");
			}
		}

		return main_table::get().exit_scope(id);
	}

	void eval_base::enter_statement_stack(std::vector<ref<statement>>* list)
	{
		s_data.statement_stack = make_ref<stmt_stack>(list, s_data.statement_stack);
	}

	void eval_base::exit_statement_stack()
	{
		if (s_data.statement_stack->parent)
			s_data.statement_stack = s_data.statement_stack->parent;
	}

	void eval_base::enter_statement_stack_and_scope(id_t id, std::vector<ref<statement>>* list, scope_type type)
	{
		enter_scope(id, type);
		enter_statement_stack(list);
	}

	ref<scope> eval_base::exit_statement_stack_and_scope(id_t id)
	{
		exit_statement_stack();
		return exit_scope(id);
	}

	symbol* eval_base::search_member(id_t id, const std::string& name, ref<variable_base> object) const
	{
		if (should_search_only_func())
			return search_member_function(id, name, object, get_last_parameters());
		symbol* sym = nullptr;
		if (object)
		{
			if (s_data.constant_object_members[object->get_type()])
				sym = s_data.constant_object_members[object->get_type()]->lookup(name);

			if (!sym)
			{
				ref<symbol_table> member_scope = object->get_members();

				if (!member_scope)
					return nullptr;

				return member_scope->lookup(name);
			}
			else
				return sym;
		}
		return search(id, name);
	}

	symbol* eval_base::search_member_function(id_t id, const std::string& name, ref<variable_base> object, const std::vector<function_param>& parameters) const
	{
		symbol* sym = nullptr;
		if (object)
		{
			if (s_data.constant_object_members[object->get_type()])
				sym = s_data.constant_object_members[object->get_type()]->lookup(name);

			if (!sym)
			{
				ref<symbol_table> member_scope = object->get_members();

				if (!member_scope)
					return nullptr;

				return member_scope->lookup(name);
			}
			else
				return sym;
		}
		return search_function(id, name, parameters);
	}

	symbol* eval_base::search(id_t id, const std::string& name) const
	{
		if (should_search_only_func())
			return search_function(id, name, get_last_parameters());
		id_t pass_id = in_func_call() ? s_data.func_eval_ids.top() : 0;
		return main_table::get().search(id, name, pass_id);
	}

	symbol* eval_base::search_current(id_t id, const std::string& name) const
	{
		id_t pass_id = in_func_call() ? s_data.func_eval_ids.top() : 0;
		return main_table::get().search_current(id, name, pass_id);
	}

	symbol* eval_base::search_current_function(id_t id, const std::string& name, const std::vector<function_param>& parameters) const
	{
		return main_table::get().search_current_function(id, name, parameters);
	}

	symbol* eval_base::search_function(id_t id, const std::string& name, const std::vector<function_param>& parameters) const
	{
		id_t pass_id = in_func_call() ? s_data.func_eval_ids.top() : 0;
		return main_table::get().search_function(id, name, parameters, pass_id);
	}

	std::pair<bool, symbol*> eval_base::is_function_valid(id_t id, const std::string name, const std::vector<function_param>& parameters) const
	{
		id_t pass_id = in_func_call() ? s_data.func_eval_ids.top() : 0;
		return main_table::get().is_function_valid(id, name, parameters, pass_id);
	}

	void eval_base::insert(id_t id, const std::string& name, const symbol& symbol)
	{
		main_table::get().insert(id, name, symbol);
	}

	void eval_base::insert_to_global(id_t id, const std::string& name, const symbol& symbol)
	{
		main_table::get().insert_to_global(id, name, symbol);
	}

	ref<function_definition> eval_base::get_func_definition(const std::string& name, std::vector<function_param> real_parameters)
	{
		for (auto& stmt : *s_data.statement_stack->list)
		{
			if (auto func_decl = std::dynamic_pointer_cast<function_definition>(stmt))
			{
				if (func_decl->m_id->m_token.value == name)
				{
					std::vector<function_param> parameters;
					for (auto& param : func_decl->m_params)
						parameters.emplace_back(param->m_id->m_token.value, param->m_type, (bool)param->m_is_ref);

					if (std::equal(parameters.begin(), parameters.end(), real_parameters.begin(), real_parameters.end()))
						return func_decl;
				}
			}
		}

		return nullptr;
	}

	ref<scope>& eval_base::get_scope(id_t id) const
	{
		return main_table::get().get_scope(id);
	}

	std::map<variable_type, ref<symbol_table>>& eval_base::get_constant_object_members()
	{
		return s_data.constant_object_members;
	}

	const std::vector<function_param>& eval_base::get_last_parameters() const
	{
		return s_data.last_parameters;
	}

	ref<variable_base> eval_base::get_last_return_value() const
	{
		return s_data.last_return_value;
	}

	ref<variable_base>* eval_base::get_last_left_value() const
	{
		return s_data.last_left_value;
	}

	ref<stmt_stack> eval_base::get_statement_stack() const
	{
		return s_data.statement_stack;
	}

	void eval_base::inc_loop_depth()
	{
		s_data.loop_depth++;
	}

	void eval_base::dec_loop_depth()
	{
		s_data.loop_depth--;
	}

	int eval_base::get_loop_depth() const
	{
		return s_data.loop_depth;
	}

	void eval_base::inc_func_body_depth()
	{
		s_data.func_body_depth++;
	}

	void eval_base::dec_func_body_depth()
	{
		s_data.func_body_depth--;
	}

	uint16_t eval_base::get_func_body_depth() const
	{
		return s_data.func_body_depth;
	}

	bool eval_base::break_called() const
	{
		return s_data.eval_flags.b1;
	}

	bool eval_base::continue_called() const
	{
		return s_data.eval_flags.b2;
	}

	bool eval_base::return_called() const
	{
		return s_data.eval_flags.b3;
	}

	bool eval_base::is_ref_error_active() const
	{
		return s_data.eval_flags.b4;
	}

	bool eval_base::in_func_call() const
	{
		return s_data.eval_flags.b5;
	}

	bool eval_base::should_search_only_func() const
	{
		return s_data.eval_flags.b6;
	}

	bool eval_base::is_sf() const
	{
		return s_data.program_flags.b1;
	}

	bool eval_base::is_nb() const
	{
		return s_data.program_flags.b2;
	}

	void eval_base::set_last_return_value(ref<variable_base> value)
	{
		if (value == nullptr)
			s_data.last_return_value = create_null_variable();
		else
			s_data.last_return_value = value;
	}

	void eval_base::set_last_left_value(ref<variable_base>* value)
	{
		s_data.last_left_value = value;
	}

	void eval_base::set_last_parameters(const std::vector<function_param>& parameters)
	{
		s_data.last_parameters = parameters;
	}

	void eval_base::set_loop_depth(int loop_depth)
	{
		s_data.loop_depth = loop_depth;
	}

	void eval_base::set_func_body_depth(int fb_depth)
	{
		s_data.func_body_depth = fb_depth;
	}

	void eval_base::call_break(bool flag)
	{
		s_data.eval_flags.b1 = flag;
	}

	void eval_base::call_continue(bool flag)
	{
		s_data.eval_flags.b2 = flag;
	}

	void eval_base::call_return(bool flag)
	{
		s_data.eval_flags.b3 = flag;
	}

	void eval_base::set_ref_error_activity(bool flag)
	{
		s_data.eval_flags.b4 = flag;
	}

	void eval_base::set_func_call_state(bool flag)
	{
		s_data.eval_flags.b5 = flag;
	}

	void eval_base::set_search_only_func_state(bool flag)
	{
		s_data.eval_flags.b6 = flag;
	}
}
