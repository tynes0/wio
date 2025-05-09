#pragma once

#include "../../interpreter/scope.h"

namespace wio
{
    namespace builtin
    {
        class b_vec3
        {
        public:
            static ref<symbol_table> load();
        };
    }
}