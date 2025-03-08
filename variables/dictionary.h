#pragma once

#include "variable.h"
#include "array.h"

#include <vector>
#include <memory>

namespace wio
{
    class var_dictionary : public detail::templated_var_array<std::pair<ref<variable_base>, ref<variable_base>>>, public variable_base
    {
    public:
        using base = detail::templated_var_array<std::pair<ref<variable_base>, ref<variable_base>>>;

        var_dictionary() : base() { }

        var_dictionary(const std::string& id, bool is_constant = false)
            : base(id, is_constant) { }

        var_dictionary(const std::string& id, bool is_constant, const std::initializer_list<value_type>& ilist)
            : base(id, is_constant, ilist) { }

        virtual variable_base_type get_base_type() const override
        {
            return variable_base_type::dictionary;
        }
    };

    /*var_dictionary::value_type dict_v(ref<variable_base> left_var, ref<variable_base> right_var)
    {
        return make_ref<std::pair<ref<variable_base>, ref<variable_base>>>(std::make_pair<>(left_var, right_var));
    }*/
}