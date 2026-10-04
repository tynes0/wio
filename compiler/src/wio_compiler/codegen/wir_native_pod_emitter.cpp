#include "wio/codegen/wir_native_pod_emitter.h"
#include "wio/codegen/wir_cpp_text.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace wio::codegen
{
    std::string WirNativePodEmitter::emit(const wir::lowered::Module& module, const WirModuleEmitter::TypeSpelling& cpp)
    {
        using namespace wir;
        std::ostringstream out;
        const auto identity = [&](TypeId id)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == id)
                    return reflected.stableTypeId;
            return stableModuleHash(nativeAbiTypeKey(module.types, id));
        };
        const auto codec = [&](TypeId id)
        { return "wio::wir_backend::abi::Codec<" + cpp(id) + ", " + std::to_string(identity(id)) + "ULL>"; };
        const auto supported = [&](auto&& self, TypeId id, unsigned depth) -> bool
        {
            if (depth > 64)
                return false;
            const auto& type = module.types.get(id);
            if (type.kind >= TypeKind::Bool && type.kind <= TypeKind::Char)
                return true;
            if (type.kind != TypeKind::Named)
                return false;
            if (type.nominalKind == NominalKind::Enum || type.nominalKind == NominalKind::Flagset)
                return type.enumUnderlyingType && self(self, type.enumUnderlyingType, depth + 1);
            if (type.nominalRepresentation != NominalRepresentation::NativePod)
                return false;
            for (const auto argument : type.arguments)
                if (!self(self, argument, depth + 1))
                    return false;
            return std::ranges::all_of(type.fields,
                                       [&](const auto& field) { return self(self, field.type, depth + 1); });
        };
        std::vector<TypeId> types;
        for (const auto& reflected : module.contract.reflection)
        {
            const auto id = reflected.type;
            const auto& type = module.types.get(id);
            if (type.nominalRepresentation != NominalRepresentation::NativePod || !supported(supported, id, 0) ||
                std::ranges::find(types, id) != types.end())
                continue;
            types.push_back(id);
            const auto name = "_native_pod_" + std::to_string(id.value());
            out << "using " << name << " = " << cpp(id) << ";\n";
            const auto boundary = [&](const std::string& operation, const std::string& first)
            {
                out << "static WioNativeAbiStatus " << name << '_' << operation << '(' << first
                    << ", std::uint64_t index, WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept {\n"
                       "    if (failure) *failure = {};\n"
                       "    if (!result) return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
                       "    *result = {};\n"
                       "    try {\n";
            };
            const auto endBoundary = [&]
            {
                out << "        return WIO_NATIVE_ABI_OK;\n"
                       "    } catch (const wio::wir_backend::abi::Error& error) {\n"
                       "        return wio::wir_backend::abi::failure(failure, error.status, error.what());\n"
                       "    } catch (const std::exception& error) {\n"
                       "        return wio::wir_backend::abi::failure(failure, WIO_NATIVE_ABI_EXCEPTION, "
                       "error.what());\n"
                       "    } catch (...) {\n"
                       "        return wio::wir_backend::abi::failure(failure, WIO_NATIVE_ABI_EXCEPTION, \"native POD "
                       "adapter failed\");\n"
                       "    }\n}\n";
            };
            boundary("pack", "const WioNativeAbiValue* prototype, const WioNativeAbiValue* fields");
            out << "        if (index != " << type.fields.size()
                << " || (index && !fields)) return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
                   "        "
                << name << " value = prototype ? " << codec(id) << "::read(*prototype) : " << name << "{};\n";
            for (std::size_t index = 0; index < type.fields.size(); ++index)
                out << "        value." << type.fields[index].name << " = " << codec(type.fields[index].type)
                    << "::read(fields[" << index << "]);\n";
            out << "        *result = " << codec(id) << "::write(std::move(value));\n";
            endBoundary();
            boundary("read", "const WioNativeAbiValue* pod");
            out << "        if (!pod) return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
                   "        const auto value = "
                << codec(id)
                << "::read(*pod);\n"
                   "        switch (index) {\n";
            for (std::size_t index = 0; index < type.fields.size(); ++index)
                out << "        case " << index << ": *result = " << codec(type.fields[index].type) << "::write(value."
                    << type.fields[index].name << "); break;\n";
            out << "        default: return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n        }\n";
            endBoundary();
            if (!type.fields.empty())
            {
                out << "static const WioNativePodFieldDescriptor " << name << "_fields[] = {\n";
                for (const auto& field : type.fields)
                    out << "    {" << WirCppText::quote(field.name) << ", " << identity(field.type) << "ULL, offsetof("
                        << name << ", " << field.name << "), sizeof(" << name << "{}." << field.name << "), "
                        << static_cast<unsigned>(module.types.get(field.type).kind) << "},\n";
                out << "};\n";
            }
        }
        if (!types.empty())
        {
            out << "static const WioNativePodLayoutDescriptor _native_pod_layouts[] = {\n";
            for (const auto id : types)
            {
                const auto name = "_native_pod_" + std::to_string(id.value());
                const auto count = module.types.get(id).fields.size();
                out << "    {" << identity(id) << "ULL, sizeof(" << name << "), alignof(" << name << "), " << count
                    << ", " << (count ? name + "_fields" : "nullptr") << ", &" << name << "_pack, &" << name
                    << "_read},\n";
            }
            out << "};\n";
        }
        out << "static const WioNativePodLayoutApi _native_pod_api{WIO_NATIVE_POD_LAYOUT_VERSION, " << types.size()
            << ", " << (types.empty() ? "nullptr" : "_native_pod_layouts")
            << "};\n"
               "extern \"C\" WIO_WIR_EXPORT const WioNativePodLayoutApi* WioGetNativePodLayoutApi() noexcept {\n"
               "    return &_native_pod_api;\n}\n";
        return "#include <wio_native_pod.h>\n" + out.str();
    }
} // namespace wio::codegen
