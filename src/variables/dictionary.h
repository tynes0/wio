#pragma once

#include "variable_base.h"
#include "../base/base.h"

#include <map>

namespace wio
{
	class var_dictionary : public variable_base
	{
	public:
		using map_t = std::map<std::string, ref<variable_base>>; // std::map could be change

		var_dictionary(const map_t& data, packed_bool flags = {});

		virtual variable_base_type get_base_type() const override;
		virtual variable_type get_type() const override;
		virtual ref<variable_base> clone() const override;

		static std::string as_key(ref<variable_base> value);

		// TODO: Member functions

		map_t& get_data();
		const map_t& get_data() const;
		ref<variable_base> get_element(ref<variable_base> key);

		void set_data(const map_t& data);
		void set_element(ref<variable_base> key, ref<variable_base> value);
		size_t size() const;
	private:
		bool check_existance(const std::string& key);

		 map_t m_data;
	};
}