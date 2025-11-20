#include "unit.h"

namespace wio
{
	unit::unit(packed_bool flags) : variable_base(flags), m_is_final(false)
	{

	}

	unit::unit(const std::string& identifier, const std::vector<ref<unit>>& parents, const std::vector<ref<unit>>& trust_list, bool is_final, packed_bool flags)
		: m_identifier(identifier), m_parents(parents), m_trust_list(trust_list), m_is_final(is_final), variable_base(flags)
	{
	}

	variable_base_type unit::get_base_type() const
	{
		return variable_base_type::unit;
	}

	variable_type unit::get_type() const
	{
		return variable_type::vt_unit;
	}

	ref<variable_base> unit::clone() const
	{
		return make_ref<unit>(*this);
	}

	void unit::set_identifier(const std::string& identifier)
	{
		m_identifier = identifier;
	}

	const std::string& unit::get_identifier()
	{
		return m_identifier;
	}

	void unit::add_data(const std::string& id, const ref<variable_base>& value)
	{
		unit_access_type ud_type{};

		if (value->is_exposed())
			ud_type = unit_access_type::exposed;
		else if (value->is_shared())
			ud_type = unit_access_type::shared;
		else
			ud_type = unit_access_type::hidden;

		m_data.insert(detail::unit_item{ id, value, ud_type, value->is_outer() });
	}

	unit_data& unit::get_all()
	{
		return m_data;
	}

	ref<overload_list> unit::get_ctor()
	{
		auto value = m_data.find_by<detail::ud_id_index>(token_constants::ctor_str);
		if (value)
			return std::dynamic_pointer_cast<overload_list>(value->data);
		return nullptr;
	}

	unit_access_type unit::access_type_of(const std::string& id) const
	{
		auto value = m_data.find_by<detail::ud_id_index>(id);
		if (value)
			return value->access_type;

		if (m_members)
		{
			for (auto& item : m_members->get_symbols())
			{
				if (item.first == id) // so this is an outer
					return unit_access_type::exposed; // always reachable
			}
		}

		return unit_access_type::none;
	}

	const std::vector<ref<unit>>& unit::get_parents() const
	{
		return m_parents;
	}

	const std::vector<ref<unit>>& unit::get_trust_list() const
	{
		return m_trust_list;
	}

	bool unit::is_final() const
	{
		return m_is_final;
	}

	bool unit::operator==(const unit& right) const noexcept
	{
		return m_identifier == right.m_identifier;
	}

	unit_instance::unit_instance(packed_bool flags) : variable_base(flags), m_source()
	{
	}

	unit_instance::unit_instance(ref<unit> source, packed_bool flags) : variable_base(flags), m_source(source)
	{
		unit_data& datas = source->get_all();

		ref<symbol_table> members = make_ref<symbol_table>();
		this->load_members(members);

		for (auto& item : datas)
		{
			if (item->id == token_constants::ctor_str || item->id == token_constants::dtor_str || item->is_outer /* Probably this is impossible...*/)
				continue;
			members->insert(item->id, symbol(item->data->clone()));
		}

		for (auto& parent : source->get_parents())
		{
			unit_data& parent_datas = parent->get_all();
		}

		ref<symbol_table> outer_members = source->get_members();
		if (outer_members)
		{
			for (auto& item : outer_members->get_symbols())
				members->insert(item.first, item.second);
		}
	}

	variable_base_type unit_instance::get_base_type() const
	{
		return variable_base_type::unit_instance;
	}

	variable_type unit_instance::get_type() const
	{
		return variable_type::vt_unit_instance;
	}

	ref<variable_base> unit_instance::clone() const
	{
		return make_ref<unit_instance>(*this);
	}

	void unit_instance::set_source(ref<unit> source)
	{
		m_source = source;
	}

	ref<unit> unit_instance::get_source()
	{
		return m_source;
	}

	unit_access_type unit_instance::access_type_of(const std::string& id) const
	{
		return m_source->access_type_of(id);
	}
}
