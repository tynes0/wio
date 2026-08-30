#pragma once

#include "wio/common/smart_ptr.h"

#include <string>

namespace wio
{
    struct Program;

    namespace codegen
    {
        class CppGenerator;

        namespace detail
        {
            class ModuleApiEmitter final
            {
            public:
                explicit ModuleApiEmitter(CppGenerator& generator);

                void emit(const Ref<Program>& program);

            private:
                CppGenerator& generator_;

                void emit(const std::string& value);
                void emitLine(const std::string& value = {});
                void emitGeneratedDirective();
                void emitTabs();
                void indent();
                void dedent();
            };
        }
    }
}
