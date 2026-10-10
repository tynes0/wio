#include "wio/codegen/wir_native_pod_emitter.h"
#include "wio/codegen/wir_cpp_text.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <vector>

namespace wio::codegen
{
    std::vector<wir::TypeId> WirNativePodEmitter::eligibleTypes(const wir::lowered::Module& module)
    {
        using namespace wir;
        const auto concreteArgument = [&](auto&& self, TypeId id, unsigned depth) -> bool
        {
            const auto* type = module.types.tryGet(id);
            if (!type || depth > 64 || type->extentParameter || type->kind == TypeKind::Invalid ||
                type->kind == TypeKind::GenericParameter || type->kind == TypeKind::ConstGenericParameter ||
                type->kind == TypeKind::GenericParameterPack || type->kind == TypeKind::TypePack ||
                type->kind == TypeKind::ValuePack)
                return false;
            // Arguments identify a template specialization; they are not
            // necessarily stored fields (e.g. an integer extent or an unused
            // string type). Field compatibility is checked separately below.
            return std::ranges::all_of(type->arguments,
                                       [&](TypeId argument) { return self(self, argument, depth + 1); });
        };
        const auto supported = [&](auto&& self, TypeId id, unsigned depth) -> bool
        {
            if (depth > 64)
                return false;
            const auto& type = module.types.get(id);
            if (type.kind >= TypeKind::Bool && type.kind <= TypeKind::Char)
                return true;
            if (type.kind == TypeKind::Opaque)
                return true;
            if (type.kind == TypeKind::Nullable)
                return type.arguments.size() == 1 && module.types.get(type.arguments.front()).kind == TypeKind::Opaque;
            if (type.kind == TypeKind::Array)
                return type.staticExtent.has_value() && !type.extentParameter && type.arguments.size() == 1 &&
                       self(self, type.arguments.front(), depth + 1);
            if (type.kind != TypeKind::Named)
                return false;
            if (type.nominalKind == NominalKind::Enum || type.nominalKind == NominalKind::Flagset)
                return type.enumUnderlyingType && self(self, type.enumUnderlyingType, depth + 1);
            if (type.nominalRepresentation != NominalRepresentation::NativePod)
                return false;
            for (const auto argument : type.arguments)
                if (!concreteArgument(concreteArgument, argument, depth + 1))
                    return false;
            return std::ranges::all_of(type.fields,
                                       [&](const auto& field) { return self(self, field.type, depth + 1); });
        };
        std::vector<TypeId> types;
        for (const auto& reflected : module.contract.reflection)
        {
            const auto id = reflected.type;
            const auto& type = module.types.get(id);
            if (type.nominalKind != NominalKind::Component ||
                type.nominalRepresentation != NominalRepresentation::NativePod || !supported(supported, id, 0) ||
                std::ranges::find(types, id) != types.end())
                continue;
            types.push_back(id);
        }
        return types;
    }

    std::string WirNativePodEmitter::emitProjectionAccess(const wir::lowered::Module& module,
                                                          const WirModuleEmitter::TypeSpelling& cpp)
    {
        using namespace wir;
        const auto pods = eligibleTypes(module);
        if (pods.empty())
            return {};
        const auto identity = [&](TypeId id)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == id)
                    return reflected.stableTypeId;
            return stableModuleHash(nativeAbiTypeKey(module.types, id));
        };
        std::vector<TypeId> arrays;
        const auto collect = [&](auto&& self, TypeId id) -> void
        {
            const auto& type = module.types.get(id);
            if (type.kind == TypeKind::Array && type.staticExtent && std::ranges::find(arrays, id) == arrays.end())
            {
                arrays.push_back(id);
                self(self, type.arguments.front());
            }
            for (const auto& field : type.fields)
                self(self, field.type);
        };
        for (const auto id : pods)
            collect(collect, id);
        // Multiple Wio declarations can expose different surfaces of one C++ POD.
        // Specialize by C++ type once, then dispatch by the stable Wio identity.
        std::map<std::string, std::vector<TypeId>> podGroups;
        std::map<std::string, std::vector<TypeId>> arrayGroups;
        for (const auto id : pods)
            podGroups[cpp(id)].push_back(id);
        for (const auto id : arrays)
            arrayGroups[cpp(id)].push_back(id);
        std::ostringstream out;
        out << "namespace wio::wir_backend::abi {\n";
        const std::string signature =
            "void* storage, std::uint64_t nodeId, std::uint64_t targetId, std::uint64_t offset, "
            "std::uint64_t size, const std::type_info& requested";
        const auto declarations = [&](const auto& groups, const char* trait)
        {
            for (const auto& [spelling, ids] : groups)
                out << "template<> struct " << trait << "<" << spelling << "> { static void* resolve(" << signature
                    << ") noexcept; };\n";
        };
        declarations(podGroups, "NativePodProjectionAccess");
        declarations(arrayGroups, "PodFieldProjectionAccess");
        for (const auto& [spelling, ids] : arrayGroups)
        {
            out << "inline void* PodFieldProjectionAccess<" << spelling << ">::resolve(" << signature
                << ") noexcept {\n"
                << "    using Array = " << spelling << ";\n"
                << "    using Element = typename Array::value_type;\n"
                << "    if (!storage) return nullptr;\n";
            for (const auto id : ids)
            {
                const auto& type = module.types.get(id);
                out << "    if (nodeId == " << identity(id) << "ULL) {\n"
                    << "        if (!offset && size == sizeof(Array) && targetId == nodeId && requested == "
                       "typeid(Array)) "
                       "return storage;\n"
                    << "        const auto index = offset / sizeof(Element);\n"
                    << "        if (index >= " << *type.staticExtent << "ULL) return nullptr;\n"
                    << "        return PodFieldProjectionAccess<Element>::resolve("
                       "std::addressof((*static_cast<Array*>(storage))[index]), "
                    << identity(type.arguments.front())
                    << "ULL, targetId, offset % sizeof(Element), size, requested);\n    }\n";
            }
            out << "    return nullptr;\n}\n";
        }
        for (const auto& [spelling, ids] : podGroups)
        {
            out << "inline void* NativePodProjectionAccess<" << spelling << ">::resolve(" << signature
                << ") noexcept {\n"
                << "    using Pod = " << spelling << ";\n"
                << "    static_assert(std::is_trivially_copyable_v<Pod> && std::is_standard_layout_v<Pod>);\n"
                << "    if (!storage) return nullptr;\n";
            for (const auto id : ids)
            {
                out << "    if (nodeId == " << identity(id) << "ULL) {\n"
                    << "        if (!offset && size == sizeof(Pod) && targetId == nodeId && requested == typeid(Pod)) "
                       "return storage;\n";
                for (const auto& field : module.types.get(id).fields)
                    out << "        if (offset >= offsetof(Pod, " << field.name << ") && offset - offsetof(Pod, "
                        << field.name << ") < sizeof(Pod{}." << field.name << "))\n"
                        << "            return PodFieldProjectionAccess<std::remove_cvref_t<decltype(Pod{}."
                        << field.name << ")>>::resolve(std::addressof(static_cast<Pod*>(storage)->" << field.name
                        << "), " << identity(field.type) << "ULL, targetId, offset - offsetof(Pod, " << field.name
                        << "), size, requested);\n";
                out << "        return nullptr;\n    }\n";
            }
            out << "    return nullptr;\n}\n";
        }
        out << "} // namespace wio::wir_backend::abi\n";
        return out.str();
    }

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
        const auto types = eligibleTypes(module);
        for (const auto id : types)
        {
            const auto& type = module.types.get(id);
            const auto name = "_native_pod_" + std::to_string(id.value());
            out << "using " << name << " = " << cpp(id) << ";\n";
            for (const auto& field : type.fields)
            {
                const auto& fieldType = module.types.get(field.type);
                if (fieldType.kind == TypeKind::Opaque || fieldType.kind == TypeKind::Nullable)
                    out << "static_assert(std::is_same_v<std::remove_cvref_t<decltype(" << name << "{}." << field.name
                        << ")>, void*>, \"native POD opaque field must have void* storage\");\n";
                if (fieldType.kind == TypeKind::Array)
                    out << "static_assert(sizeof(" << name << "{}." << field.name << ") == "
                        << (*fieldType.staticExtent ? "sizeof(" + cpp(fieldType.arguments.front()) + ") * " +
                                                          std::to_string(*fieldType.staticExtent)
                                                    : "sizeof(" + cpp(field.type) + ")")
                        << ", \"native POD fixed-array field layout is unsupported\");\n";
            }
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
            out << "static WioNativeAbiStatus " << name
                << "_default(WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept {\n"
                   "    if (failure) *failure = {};\n"
                   "    if (!result) return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
                   "    *result = {};\n"
                   "    try {\n"
                   "        *result = "
                << codec(id) << "::write(" << name << "{});\n";
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
            out << "static const WioNativePodDefaultDescriptor _native_pod_defaults[] = {\n";
            for (const auto id : types)
                out << "    {" << identity(id) << "ULL, &_native_pod_" << id.value() << "_default},\n";
            out << "};\n";
        }
        out << "static const WioNativePodLayoutApi _native_pod_api{WIO_NATIVE_POD_LAYOUT_VERSION, " << types.size()
            << ", " << (types.empty() ? "nullptr" : "_native_pod_layouts")
            << "};\n"
               "extern \"C\" WIO_WIR_EXPORT const WioNativePodLayoutApi* WioGetNativePodLayoutApi() noexcept {\n"
               "    return &_native_pod_api;\n}\n";
        out << "static const WioNativePodDefaultApi _native_pod_default_api{WIO_NATIVE_POD_DEFAULT_VERSION, "
            << types.size() << ", " << (types.empty() ? "nullptr" : "_native_pod_defaults")
            << "};\n"
               "extern \"C\" WIO_WIR_EXPORT const WioNativePodDefaultApi* WioGetNativePodDefaultApi() noexcept {\n"
               "    return &_native_pod_default_api;\n}\n";
        return "#include <wio_native_pod.h>\n" + out.str();
    }
} // namespace wio::codegen
