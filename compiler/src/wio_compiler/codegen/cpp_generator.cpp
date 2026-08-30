#include "wio/codegen/cpp_generator.h"
#include "detail/module_api_emitter.h"

#include "wio/ast/attribute_contract.h"
#include "wio/ast/attribute_queries.h"
#include "wio/ast/declaration_queries.h"
#include "wio/codegen/cpp_identifier.h"
#include "wio/common/filesystem/filesystem.h"
#include "wio/common/operator_overload.h"
#include "wio/common/utility.h"
#include "compiler.h"
#include "wio/common/logger.h"
#include "wio/sema/symbol.h"
#include "wio/sema/scope_lookup.h"
#include "wio/sema/constant_evaluator.h"
#include "wio/sema/generic_support.h"
#include "wio/sema/type_queries.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <unordered_set>
#include <utility>

#define EMIT_TABS() do { for (int _____I_____ = 0; _____I_____ < indentationLevel_; ++_____I_____) buffer_ << "    "; } while(false)

namespace wio::codegen
{
    namespace 
    {
        #include "detail/generator_support.inl"
    }

    #include "detail/generator_core.inl"

    void CppGenerator::emitModuleApiTable(const Ref<Program>& program)
    {
        detail::ModuleApiEmitter(*this).emit(program);
    }
    #include "detail/emission_pipeline.inl"

    #include "detail/expression_visitors.inl"

    #include "detail/declaration_visitors.inl"

    #include "detail/control_statement_visitors.inl"

}
