#pragma once 

#include "../utils/any.h"
#include "../base/base.h"
#include <string>
#include "variable_base.h"

namespace wio
{
    class realm : public variable_base
    {
    public:
        realm(packed_bool flags = {});
        virtual variable_base_type get_base_type() const override;
        virtual variable_type get_type() const override;
        virtual ref<variable_base> clone() const override;
    };

} // namespace wio