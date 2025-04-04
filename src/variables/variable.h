#pragma once 

#include "../utils/any.h"
#include "../base/base.h"
#include <string>
#include "variable_base.h"

namespace wio 
{
    class variable : public null_var 
    {
    public:
        variable(const any& data, variable_type type, packed_bool flags = {});
        virtual variable_base_type get_base_type() const override;
        virtual variable_type get_type() const override;
        virtual ref<variable_base> clone() const override;

        any& get_data();
        const any& get_data() const;

        template <class T>
        T& get_data_as() { return any_cast<T&>(m_data); }
        template <class T>
        const T& get_data_as() const { return any_cast<const T&>(m_data); }

        void set_data(const any& new_data);
        void set_type(variable_type type);
    private:
        any m_data;
        variable_type m_type;
    };

} // namespace wio