#include "wio/codegen/wir_native_sequence_emitter.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace wio::codegen
{
    std::string WirNativeSequenceEmitter::emit(const wir::lowered::Module& module,
                                               const WirModuleEmitter::TypeSpelling& cpp)
    {
        using namespace wir;
        const auto supported = [&](auto&& self, TypeId id, unsigned depth) -> bool
        {
            const auto* type = module.types.tryGet(id);
            if (!type || depth > 64)
                return false;
            if (type->kind >= TypeKind::Bool && type->kind <= TypeKind::Text)
                return true;
            if (type->kind == TypeKind::Opaque)
                return true;
            if (type->kind == TypeKind::Nullable)
                return type->arguments.size() == 1 &&
                       module.types.get(type->arguments.front()).kind == TypeKind::Opaque;
            if (type->kind == TypeKind::Array)
                return !type->staticExtent && !type->extentParameter && type->arguments.size() == 1 &&
                       self(self, type->arguments.front(), depth + 1);
            return type->kind == TypeKind::Named && type->arguments.empty() &&
                   (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset ||
                    type->nominalRepresentation == NominalRepresentation::NativePod);
        };
        std::set<TypeId> sequences;
        const auto collect = [&](auto&& self, TypeId id, unsigned depth) -> void
        {
            const auto* type = module.types.tryGet(id);
            if (!type || depth > 64)
                return;
            if ((type->kind == TypeKind::Reference || type->kind == TypeKind::AsyncTask) && type->arguments.size() == 1)
            {
                self(self, type->arguments.front(), depth + 1);
                return;
            }
            if (type->kind != TypeKind::Array || !supported(supported, id, 0) || !sequences.insert(id).second)
                return;
            self(self, type->arguments.front(), depth + 1);
        };
        for (const auto& function : module.functions)
        {
            if (!function.nativeBinding || !function.genericParameters.empty())
                continue;
            for (const auto& parameter : function.nativeBinding->parameters)
                collect(collect, parameter.type, 0);
            collect(collect, function.nativeBinding->result.type, 0);
        }
        if (sequences.empty())
            return {};
        const auto identity = [&](TypeId id)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == id)
                    return reflected.stableTypeId;
            return stableModuleHash(nativeAbiTypeKey(module.types, id));
        };
        std::ostringstream out;
        out << "#include <wir_sequence_abi.h>\n";
        for (const auto id : sequences)
        {
            auto element = module.types.get(id).arguments.front();
            if (module.types.get(element).kind == TypeKind::Nullable)
                element = module.types.get(element).arguments.front();
            out << "using _native_sequence_" << id.value() << " = wio::wir_backend::abi::SequenceAdapter<" << cpp(id)
                << ", " << identity(id) << "ULL, " << identity(element) << "ULL>;\n";
        }
        out << "static const WioNativeSequenceDescriptor _native_sequences[] = {\n";
        for (const auto id : sequences)
        {
            const auto name = "_native_sequence_" + std::to_string(id.value());
            out << "    {" << identity(id) << "ULL, " << identity(module.types.get(id).arguments.front()) << "ULL, &"
                << name << "::pack, &" << name << "::count, &" << name << "::read},\n";
        }
        out << "};\nstatic const WioNativeSequenceApi _native_sequence_api{WIO_NATIVE_SEQUENCE_VERSION, "
            << sequences.size()
            << ", _native_sequences};\n"
               "extern \"C\" WIO_WIR_EXPORT const WioNativeSequenceApi* WioGetNativeSequenceApi() noexcept { return "
               "&_native_sequence_api; }\n";
        return out.str();
    }
} // namespace wio::codegen
