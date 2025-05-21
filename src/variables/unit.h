#pragma once 

#include "../utils/any.h"
#include "../base/base.h"
#include "variable_base.h"

#include <string>

namespace wio
{
    class unit : public variable_base
    {
    public:
        unit(packed_bool flags = {});
        unit(const std::string& identifier, packed_bool flags = {});
        virtual variable_base_type get_base_type() const override;
        virtual variable_type get_type() const override;
        virtual ref<variable_base> clone() const override;

        void set_identifier(const std::string& identifier);
        const std::string& get_identifier();
    private:
        std::vector<symbol*> m_parents;
        std::string m_identifier;
    };
} // namespace wio