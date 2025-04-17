#pragma once

#include "../../interpreter/scope.h"

namespace wio
{
    namespace builtin
    {
        class b_dictionary
        {
        public:
            static ref<symbol_table> load();
        };
    }
}