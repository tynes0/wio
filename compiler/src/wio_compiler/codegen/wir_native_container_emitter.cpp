#include "wio/codegen/wir_native_container_emitter.h"
#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/codegen/wir_native_pod_emitter.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace wio::codegen
{
    std::string WirNativeContainerEmitter::emit(const wir::lowered::Module& module,
                                                const WirModuleEmitter::TypeSpelling& cpp)
    {
        using namespace wir;
        std::set<TypeId> podTypes;
        bool podsCollected = false;
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
            if (type->kind == TypeKind::Dictionary)
            {
                if (type->arguments.size() != 2)
                    return false;
                const auto& key = module.types.get(type->arguments.front());
                const bool keySupported = (key.kind >= TypeKind::Bool && key.kind <= TypeKind::USize) ||
                                          (key.kind >= TypeKind::Byte && key.kind <= TypeKind::Text) ||
                                          (key.kind == TypeKind::Named && (key.nominalKind == NominalKind::Enum ||
                                                                           key.nominalKind == NominalKind::Flagset));
                return keySupported && self(self, type->arguments.back(), depth + 1);
            }
            if (type->kind != TypeKind::Named)
                return false;
            if (type->arguments.empty() &&
                (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset))
                return true;
            if (type->nominalRepresentation != NominalRepresentation::NativePod)
                return false;
            if (!podsCollected)
            {
                // Do not scan physical POD layouts for modules whose native
                // Any/container boundaries never need a POD adapter.
                const auto nativePods = WirNativePodEmitter::eligibleTypes(module);
                podTypes.insert(nativePods.begin(), nativePods.end());
                podsCollected = true;
            }
            return podTypes.contains(id);
        };
        std::set<TypeId> sequences;
        std::set<TypeId> dictionaries;
        std::set<TypeId> anyPayloads;
        TypeId anyType;
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
            if (type->kind == TypeKind::Any)
            {
                anyType = id;
                return;
            }
            if (!supported(supported, id, 0))
                return;
            if (type->kind == TypeKind::Array && sequences.insert(id).second)
                self(self, type->arguments.front(), depth + 1);
            else if (type->kind == TypeKind::Dictionary && dictionaries.insert(id).second)
                for (const auto argument : type->arguments)
                    self(self, argument, depth + 1);
        };
        for (const auto& function : module.functions)
        {
            if (!function.nativeBinding || !function.genericParameters.empty())
                continue;
            for (const auto& parameter : function.nativeBinding->parameters)
                collect(collect, parameter.type, 0);
            collect(collect, function.nativeBinding->result.type, 0);
        }
        if (anyType.isValid())
        {
            const auto objects = WirNativeObjectEmitter::eligibleTypes(module);
            const auto objectContracts = WirNativeObjectEmitter::hostedHierarchyTypes(module, objects);
            for (const auto& function : module.functions)
                for (const auto& block : function.blocks)
                    for (const auto& instruction : block.instructions)
                        if ((instruction.opcode == lowered::Opcode::AnyBox ||
                             instruction.opcode == lowered::Opcode::AnyCheckedCast ||
                             instruction.opcode == lowered::Opcode::AnyTypeTest) &&
                            (supported(supported, instruction.targetType, 0) ||
                             objectContracts.contains(instruction.targetType)) &&
                            module.types.get(instruction.targetType).kind != TypeKind::Nullable)
                        {
                            anyPayloads.insert(instruction.targetType);
                            collect(collect, instruction.targetType, 0);
                        }
        }
        if (sequences.empty() && dictionaries.empty() && !anyType.isValid())
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
        if (!dictionaries.empty())
            out << "#include <wir_dictionary_abi.h>\n";
        if (anyType.isValid())
            out << "#include <wir_any_abi.h>\n";
        for (const auto id : sequences)
        {
            auto element = module.types.get(id).arguments.front();
            if (module.types.get(element).kind == TypeKind::Nullable)
                element = module.types.get(element).arguments.front();
            out << "using _native_sequence_" << id.value() << " = wio::wir_backend::abi::SequenceAdapter<" << cpp(id)
                << ", " << identity(id) << "ULL, " << identity(element) << "ULL>;\n";
        }
        if (!sequences.empty())
        {
            out << "static const WioNativeSequenceDescriptor _native_sequences[] = {\n";
            for (const auto id : sequences)
            {
                const auto name = "_native_sequence_" + std::to_string(id.value());
                out << "    {" << identity(id) << "ULL, " << identity(module.types.get(id).arguments.front())
                    << "ULL, &" << name << "::pack, &" << name << "::count, &" << name << "::read},\n";
            }
            out << "};\nstatic const WioNativeSequenceApi _native_sequence_api{WIO_NATIVE_SEQUENCE_VERSION, "
                << sequences.size()
                << ", _native_sequences};\n"
                   "extern \"C\" WIO_WIR_EXPORT const WioNativeSequenceApi* WioGetNativeSequenceApi() noexcept { "
                   "return "
                   "&_native_sequence_api; }\n";
        }
        if (!dictionaries.empty())
        {
            for (const auto id : dictionaries)
            {
                const auto& arguments = module.types.get(id).arguments;
                auto value = arguments.back();
                if (module.types.get(value).kind == TypeKind::Nullable)
                    value = module.types.get(value).arguments.front();
                out << "using _native_dictionary_" << id.value() << " = wio::wir_backend::abi::DictionaryAdapter<"
                    << cpp(id) << ", " << identity(id) << "ULL, " << identity(arguments.front()) << "ULL, "
                    << identity(value) << "ULL>;\n";
            }
            out << "static const WioNativeDictionaryDescriptor _native_dictionaries[] = {\n";
            for (const auto id : dictionaries)
            {
                const auto& type = module.types.get(id);
                const auto name = "_native_dictionary_" + std::to_string(id.value());
                out << "    {" << identity(id) << "ULL, " << identity(type.arguments.front()) << "ULL, "
                    << identity(type.arguments.back()) << "ULL, "
                    << (type.name == "ordered" ? "WIO_NATIVE_DICTIONARY_ORDERED" : "0") << ", &" << name << "::pack, &"
                    << name << "::count, &" << name << "::read},\n";
            }
            out << "};\nstatic const WioNativeDictionaryApi _native_dictionary_api{WIO_NATIVE_DICTIONARY_VERSION, "
                << dictionaries.size()
                << ", _native_dictionaries};\n"
                   "extern \"C\" WIO_WIR_EXPORT const WioNativeDictionaryApi* WioGetNativeDictionaryApi() noexcept { "
                   "return "
                   "&_native_dictionary_api; }\n";
        }
        if (anyType.isValid())
        {
            for (const auto id : anyPayloads)
                out << "using _native_any_payload_" << id.value() << " = wio::wir_backend::abi::AnyPayloadAdapter<"
                    << cpp(id) << ", " << identity(anyType) << "ULL, " << identity(id) << "ULL>;\n";
            if (!anyPayloads.empty())
            {
                out << "static const WioNativeAnyPayloadDescriptor _native_any_payloads[] = {\n";
                for (const auto id : anyPayloads)
                {
                    const auto name = "_native_any_payload_" + std::to_string(id.value());
                    out << "    {" << identity(id) << "ULL, &" << name << "::pack, &" << name << "::read},\n";
                }
                out << "};\n";
            }
            const auto adapter = "wio::wir_backend::abi::AnyAdapter<" + std::to_string(identity(anyType)) + "ULL>";
            out << "static const WioNativeAnyApi _native_any_api{WIO_NATIVE_ANY_VERSION, " << anyPayloads.size() << ", "
                << identity(anyType) << "ULL, " << (anyPayloads.empty() ? "nullptr" : "_native_any_payloads") << ", &"
                << adapter << "::packNull, &" << adapter
                << "::isNull};\n"
                   "extern \"C\" WIO_WIR_EXPORT const WioNativeAnyApi* WioGetNativeAnyApi() noexcept { "
                   "return &_native_any_api; }\n";
        }
        return out.str();
    }
} // namespace wio::codegen
