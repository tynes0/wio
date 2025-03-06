#pragma once

#include "variable.h"
#include <vector>
#include <memory>

namespace wio
{
    namespace detail
    {
        template <class T>
        class templated_var_array
        {
        public:
            using value_type = ref<T>;

            templated_var_array() {}

            templated_var_array(const std::string& id, bool is_constant)
                : m_id(id), m_is_constant(is_constant), m_list() {
            }

            templated_var_array(const std::string& id, bool is_constant, const std::initializer_list<value_type>& ilist)
                : m_id(id), m_is_constant(is_constant), m_list(ilist) {
            }

            void set_id(const std::string& id)
            {
                m_id = id;
            }

            const std::string& get_id() const
            {
                return m_id;
            }

            void add(value_type var)
            {
                m_list.push_back(var);
            }

            void add(T var)
            {
                m_list.push_back(make_ref<T>(var));
            }

            void remove(size_t index)
            {
                m_list.erase(m_list.begin() + index);
            }

            value_type at(size_t index)
            {
                return m_list[index];
            }

            void set_as_const()
            {
                m_is_constant = true;
            }

            bool is_constant() const
            {
                return m_is_constant;
            }

            bool empty() const
            {
                return m_list.empty();
            }

            size_t size() const
            {
                return m_list.size();
            }

            void clear()
            {
                m_list.clear();
            }

        private:
            std::string m_id;
            std::vector<value_type> m_list;
            bool m_is_constant = false;
        };
    }
    
    class var_array : public detail::templated_var_array<variable_base>, public variable_base
    {
    public:
        using base = detail::templated_var_array<variable_base>;

        var_array() : base() {}

        var_array(const std::string& id, bool is_constant = false)
            : base(id, is_constant) {}

        var_array(const std::string& id, bool is_constant, const std::initializer_list<value_type>& ilist)
            : base(id, is_constant, ilist) {}

        virtual variable_base_type get_base_type() const override
        {
            return variable_base_type::array;
        }
    };

}

