#pragma once

#include "../utils/any.h"
#include "../base/base.h"
#include "variable_base.h"
#include "variable.h"

#include <vector>
#include <string>

namespace wio
{
    class var_enum : public variable_base
    {
    public:
        using container_t = std::vector<std::pair<string_t, ref<variable>>>;

        var_enum(packed_bool flags = {});
        var_enum(const std::vector<string_t>& name_list, packed_bool flags = {});
        virtual variable_base_type get_base_type() const override;
        virtual variable_type get_type() const override;
        virtual ref<variable_base> clone() const override;

        integer_t index_of(integer_t value) const;
        integer_t index_of(const string_t& item) const;
        integer_t value_of(const string_t& item) const;
        string_t to_string(integer_t value) const;
        string_t to_string_n(integer_t index) const;
        bool contains(integer_t value) const;
        bool contains(const string_t& item) const;
        integer_t size() const;
    private:
        std::vector<string_t> m_name_list;
    };
}