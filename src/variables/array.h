#pragma once

#include "variable_base.h"
#include "../base/base.h"

#include <vector>

namespace wio
{
	class var_array : public variable_base
	{
	public:
		var_array(const std::vector<ref<variable_base>>& data, packed_bool flags = {});

		virtual variable_base_type get_base_type() const override;
		virtual variable_type get_type() const override;
		virtual ref<variable_base> clone() const override;

		const std::vector<ref<variable_base>>& get_data() const;
		ref<variable_base> get_element(long long index);
		size_t size() const;

		void push(ref<variable_base> data);
		ref<variable_base> pop();
		void insert(long long idx, ref<variable_base> data);
		ref<variable_base> erase(long long idx);
		void clear();

		void set_data(const std::vector<ref<variable_base>>& data);
		void set_element(long long index, ref<variable_base> value);
	private:
		long long normalize_index(long long index) const;

		std::vector<ref<variable_base>> m_data; // std::vector could be change
	};
}