#pragma once

#include "../interpreter/scope.h"

namespace wio
{
    namespace builtin
    {
        class b_array
        {
        public:
            static ref<scope> load();
        };
    }
}