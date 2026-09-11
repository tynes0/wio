#include "wio/codegen/wir_cpp_text.h"
#include "wio/codegen/wir_reflection_emitter.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <set>

namespace wio::codegen {
std::string WirReflectionEmitter::traits(const wir::lowered::Module& m, const WirModuleEmitter::TypeSpelling& cpp) {
    using namespace wir;
    std::ostringstream out;
    std::function<bool(TypeId)> concrete = [&](TypeId t) {
        const auto& v = m.types.get(t);
        if (v.kind == TypeKind::GenericParameter || v.kind == TypeKind::ConstGenericParameter ||
            v.kind == TypeKind::GenericParameterPack || v.kind == TypeKind::TypePack || v.kind == TypeKind::ValuePack)
            return false;
        return std::ranges::all_of(v.arguments, concrete);
    };
    auto name = [&](TypeId id) {
        const auto& t = m.types.get(id);
        return t.name.empty() ? std::string(typeKindName(t.kind)) : t.name;
    };
    auto strings = [&](const char* field, const std::vector<std::string>& values) {
        out << "    static constexpr std::array<std::string_view, " << values.size() << "> " << field << "{";
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index > 0)
                out << ", ";
            out << WirCppText::quote(values[index]);
        }
        out << "};\n";
    };
    std::set<std::string> seen;
    for (const auto& r : m.contract.reflection) {
        if (!r.runtimeVisible || !concrete(r.type) || !seen.insert(cpp(r.type)).second)
            continue;
        const auto& layout = m.types.get(r.type);
        const char* kind = r.nominalKind == NominalKind::Component   ? "component_type"
                           : r.nominalKind == NominalKind::Object    ? "object_type"
                           : r.nominalKind == NominalKind::Interface ? "interface_type"
                           : r.nominalKind == NominalKind::Enum      ? "enum_type"
                                                                     : "flagset_type";
        out << "template <> struct wio::runtime::TypeReflection<" << cpp(r.type)
            << "> {\n"
               "    static constexpr std::string_view Name = "
            << WirCppText::quote(r.logicalName)
            << ";\n"
               "    static constexpr auto Kind = wio::runtime::ReflectedTypeKind::"
            << kind << ";\n";
        std::vector<std::string> names, types, access, methods, signatures, methodAccess, bases, typeAttrs, attrNames,
            attrRetentions, attrOrigins, attrArgNames, attrArgTypes, attrArgValues, fieldAttrs, behaviorAttrs,
            behaviorTypes, behaviorPhases, behaviorHooks, behaviorModes;
        std::vector<std::uint64_t> attrIds;
        std::vector<std::uint8_t> attrDefaults;
        std::vector<std::size_t> offsets{0}, attrArgOffsets{0}, behaviorOffsets{0};
        for (const auto& f : r.fields) {
            names.push_back(f.name);
            types.push_back(name(f.type));
            access.emplace_back(fieldVisibilityName(f.visibility));
            for (auto id : f.attributes)
                for (const auto& a : m.contract.attributes)
                    if (a.stableId == id && a.runtimeRetained) {
                        std::string s = a.canonicalName;
                        if (!a.arguments.empty()) {
                            s += '(';
                            for (std::size_t i = 0; i < a.arguments.size(); ++i) {
                                if (i)
                                    s += ", ";
                                s += a.arguments[i].sourceText;
                            }
                            s += ')';
                        }
                        fieldAttrs.push_back(s);
                    }
            offsets.push_back(fieldAttrs.size());
        }
        for (const auto& f : r.methods)
            if (f.name != "OnConstruct") {
                methods.push_back(f.name);
                methodAccess.emplace_back(fieldVisibilityName(f.visibility));
                std::string s = "fn(";
                for (std::size_t i = 0; i < f.parameterTypes.size(); ++i) {
                    if (i)
                        s += ", ";
                    s += name(f.parameterTypes[i]);
                }
                signatures.push_back(s + ") -> " + name(f.returnType));
                for (auto attributeId : f.attributes)
                    for (const auto& a : m.contract.attributes)
                        if (a.stableId == attributeId && a.runtimeRetained)
                            for (const auto& processor : a.processors)
                                if (processor.phase == AttributeProcessorPhase::Pre ||
                                    processor.phase == AttributeProcessorPhase::Post ||
                                    processor.phase == AttributeProcessorPhase::Finally ||
                                    processor.phase == AttributeProcessorPhase::Around) {
                                    behaviorAttrs.push_back(a.canonicalName);
                                    behaviorTypes.push_back(processor.canonicalTypeName);
                                    behaviorPhases.emplace_back(attributeProcessorPhaseName(processor.phase));
                                    behaviorHooks.push_back(processor.phase == AttributeProcessorPhase::Pre ? "Before"
                                                            : processor.phase == AttributeProcessorPhase::Post ? "After"
                                                            : processor.phase == AttributeProcessorPhase::Finally
                                                                ? "Finally"
                                                                : "Around");
                                    behaviorModes.push_back(processor.hookMode);
                                }
                behaviorOffsets.push_back(behaviorPhases.size());
            }
        for (auto base : layout.baseTypes)
            bases.push_back(name(base));
        for (auto attributeId : r.attributes)
            for (const auto& a : m.contract.attributes)
                if (a.stableId == attributeId && a.runtimeRetained) {
                    attrNames.push_back(a.canonicalName);
                    attrIds.push_back(a.stableId);
                    attrRetentions.push_back("runtime");
                    attrOrigins.emplace_back(attributeOriginKindName(a.origin));
                    std::string s = a.canonicalName;
                    if (!a.arguments.empty()) {
                        s += '(';
                        for (std::size_t i = 0; i < a.arguments.size(); ++i) {
                            if (i)
                                s += ", ";
                            s += a.arguments[i].sourceText;
                        }
                        s += ')';
                    }
                    typeAttrs.push_back(s);
                    for (const auto& argument : a.arguments) {
                        attrArgNames.push_back(argument.name);
                        attrArgTypes.push_back(argument.type ? name(argument.type) : "unknown");
                        attrArgValues.push_back(argument.sourceText);
                        attrDefaults.push_back(argument.usedDefault ? 1 : 0);
                    }
                    attrArgOffsets.push_back(attrArgValues.size());
                }
        strings("FieldNames", names);
        strings("FieldTypes", types);
        strings("FieldAccess", access);
        strings("MethodNames", methods);
        strings("MethodSignatures", signatures);
        strings("MethodAccess", methodAccess);
        strings("BaseTypes", bases);
        strings("TypeAttributes", typeAttrs);
        strings("TypeAttributeNames", attrNames);
        strings("FieldAttributes", fieldAttrs);
        strings("TypeAttributeRetentions", attrRetentions);
        strings("TypeAttributeOrigins", attrOrigins);
        strings("TypeAttributeArgumentNames", attrArgNames);
        strings("TypeAttributeArgumentTypes", attrArgTypes);
        strings("TypeAttributeArgumentValues", attrArgValues);
        strings("MethodBehaviorAttributeNames", behaviorAttrs);
        strings("MethodBehaviorProcessorTypes", behaviorTypes);
        strings("MethodBehaviorPhases", behaviorPhases);
        strings("MethodBehaviorHooks", behaviorHooks);
        strings("MethodBehaviorModes", behaviorModes);
        out << "    static constexpr std::array<std::uint64_t, " << attrIds.size() << "> TypeAttributeStableIds{";
        for (std::size_t index = 0; index < attrIds.size(); ++index) {
            if (index > 0)
                out << ", ";
            out << attrIds[index] << "ULL";
        }
        out << "};\n";
        out << "    static constexpr std::array<std::uint8_t, " << attrDefaults.size()
            << "> TypeAttributeArgumentUsedDefaults{";
        for (std::size_t index = 0; index < attrDefaults.size(); ++index) {
            if (index > 0)
                out << ", ";
            out << unsigned(attrDefaults[index]);
        }
        out << "};\n";
        out << "    static constexpr std::array<std::size_t, " << attrArgOffsets.size()
            << "> TypeAttributeArgumentOffsets{";
        for (std::size_t index = 0; index < attrArgOffsets.size(); ++index) {
            if (index > 0)
                out << ", ";
            out << attrArgOffsets[index];
        }
        out << "};\n";
        out << "    static constexpr std::array<std::size_t, " << offsets.size() << "> FieldAttributeOffsets{";
        for (std::size_t index = 0; index < offsets.size(); ++index) {
            if (index > 0)
                out << ", ";
            out << offsets[index];
        }
        out << "};\n";
        out << "    static constexpr std::array<std::size_t, " << behaviorOffsets.size() << "> MethodBehaviorOffsets{";
        for (std::size_t index = 0; index < behaviorOffsets.size(); ++index) {
            if (index > 0)
                out << ", ";
            out << behaviorOffsets[index];
        }
        out << "};\n};\n";
    }
    return out.str();
}

std::string WirReflectionEmitter::emit(const wir::lowered::Module& m, const WirModuleEmitter::TypeSpelling& cpp,
                                       const ThunkEmitter& thunk) {
    using namespace wir;
    std::ostringstream out;
    auto id = [&](TypeId t) {
        for (const auto& r : m.contract.reflection)
            if (r.type == t)
                return r.stableTypeId;
        return stableModuleHash(nativeAbiTypeKey(m.types, t));
    };
    auto codec = [&](TypeId t) {
        return "wio::wir_backend::abi::Codec<" + cpp(t) + ", " + std::to_string(id(t)) + "ULL>";
    };
    std::function<bool(TypeId)> concrete = [&](TypeId t) {
        const auto& v = m.types.get(t);
        if (v.kind == TypeKind::GenericParameter || v.kind == TypeKind::ConstGenericParameter ||
            v.kind == TypeKind::GenericParameterPack || v.kind == TypeKind::TypePack || v.kind == TypeKind::ValuePack)
            return false;
        return std::ranges::all_of(v.arguments, concrete);
    };
    auto begin = [&](const std::string& name, std::size_t count) {
        out << "static WioNativeAbiStatus " << name
            << "(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result, "
               "WioNativeAbiFailure* error) noexcept {\n"
               "    if (error)\n"
               "        *error = {};\n"
               "    if (count != "
            << count
            << " || (count && !args) || !result)\n"
               "        return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
               "    *result = {};\n"
               "    wio::wir_backend::abi::Frame frame;\n"
               "    try {\n";
    };
    auto end = [&] {
        out << "        frame.commit();\n"
               "        return WIO_NATIVE_ABI_OK;\n"
               "    } catch (const wio::wir_backend::abi::Error& exception) {\n"
               "        return wio::wir_backend::abi::failure(error, exception.status, exception.what());\n"
               "    } catch (const std::exception& exception) {\n"
               "        return wio::wir_backend::abi::failure(error, WIO_NATIVE_ABI_EXCEPTION, exception.what());\n"
               "    } catch (...) {\n"
               "        return WIO_NATIVE_ABI_EXCEPTION;\n"
               "    }\n"
               "}\n";
    };
    auto params = [&](const std::string& name, const std::vector<TypeId>& types) {
        if (!types.empty()) {
            out << "static const std::uint64_t " << name << "[] = {";
            for (std::size_t index = 0; index < types.size(); ++index) {
                if (index > 0)
                    out << ", ";
                out << id(types[index]) << "ULL";
            }
            out << "};\n";
        }
        return types.empty() ? std::string("nullptr") : name;
    };
    struct Record {
        const ReflectionDescriptor* r;
        std::string prefix;
        std::size_t methods = 0, ctors = 0;
    };
    std::vector<Record> records;
    for (const auto& r : m.contract.reflection) {
        if (!r.runtimeVisible || !concrete(r.type))
            continue;
        const auto& layout = m.types.get(r.type);
        const bool object = layout.nominalKind == NominalKind::Object || layout.nominalKind == NominalKind::Interface;
        Record record{&r, "_reflect_" + std::to_string(r.type.value())};
        const auto& p = record.prefix;
        std::vector<std::string> getters, setters;
        for (std::size_t i = 0; i < r.fields.size(); ++i) {
            const auto& f = r.fields[i];
            const auto found = std::ranges::find(layout.fields, f.name, &FieldLayout::name);
            const bool exposed =
                r.isExported && f.visibility == FieldVisibility::Public && found != layout.fields.end();
            const std::string access =
                (object ? "value->" : "value.") + (layout.nominalRepresentation == NominalRepresentation::NativePod
                                                       ? f.name
                                                       : "_f" + std::to_string(found - layout.fields.begin()));
            const auto get = p + "_get" + std::to_string(i), set = p + "_set" + std::to_string(i);
            getters.push_back(exposed ? "&" + get : "nullptr");
            setters.push_back(exposed && f.isMutable ? "&" + set : "nullptr");
            if (!exposed)
                continue;
            begin(get, 1);
            out << "        auto value = " << codec(r.type) << "::read(args[0]);\n";
            if (object)
                out << "        wio::wir_backend::abi::require(bool(value));\n";
            out << "        *result = " << codec(f.type) << "::write(" << access << ");\n";
            end();
            if (f.isMutable) {
                begin(set, 2);
                out << "        auto& value = frame.borrow<" << cpp(r.type) << ", " << id(r.type)
                    << "ULL>(args[0], true);\n";
                if (object)
                    out << "        wio::wir_backend::abi::require(bool(value));\n";
                out << "        " << access << " = " << codec(f.type) << "::read(args[1]);\n";
                end();
            }
        }
        if (!r.fields.empty()) {
            out << "static const WioNativeFieldDescriptor " << p << "_fields[] = {\n";
            for (std::size_t i = 0; i < r.fields.size(); ++i) {
                const auto& f = r.fields[i];
                out << "    {" << f.stableId << "ULL, " << WirCppText::quote(f.name) << ", " << id(f.type) << "ULL, "
                    << static_cast<int>(f.visibility) << ", " << f.isMutable << ", " << getters[i] << ", " << setters[i]
                    << "},\n";
            }
            out << "};\n";
        }
        std::vector<std::string> methodRows, ctorRows;
        for (const auto& method : r.methods) {
            const auto fn = std::ranges::find(m.functions, method.function, &lowered::Function::id);
            if (fn == m.functions.end() || !concrete(fn->callableType))
                continue;
            const bool ctor = method.name == "OnConstruct";
            const auto name = p + "_call" + std::to_string(method.stableId);
            const auto signature = params(name + "_params", method.parameterTypes);
            const bool allowed = r.isExported && method.visibility == FieldVisibility::Public && !fn->isAbstract &&
                                 m.types.get(fn->returnType).kind != TypeKind::Reference;
            std::ostringstream row;
            if (ctor) {
                if (!allowed || layout.nominalKind == NominalKind::Interface)
                    continue;
                begin(name, method.parameterTypes.size());
                out << "        auto value = " << cpp(r.type)
                    << (object ? "::Create(wio::wir_backend::SkipConstructor{})" : "{}") << ";\n";
                out << "        _wio_f" << fn->id.value() << '(' << cpp(fn->parameters.front().type)
                    << "::borrow(value)";
                for (std::size_t i = 0; i < method.parameterTypes.size(); ++i)
                    out << ", " << codec(method.parameterTypes[i]) << "::read(args[" << i << "])";
                out << ");\n        *result = " << codec(r.type) << "::write(std::move(value));\n";
                end();
                row << '{' << method.stableId << "ULL, " << method.parameterTypes.size() << ", " << signature << ", &"
                    << name << '}';
                ctorRows.push_back(row.str());
            } else {
                // Borrowed self is safe for synchronous calls only. Async owner
                // retention is handled by explicit exported owning functions.
                const bool callable = allowed && !method.isAsync;
                if (callable)
                    thunk(*fn, name);
                row << '{' << method.stableId << "ULL, " << WirCppText::quote(method.name) << ", "
                    << id(method.returnType) << "ULL, " << method.parameterTypes.size() << ", " << signature << ", "
                    << method.slot << ", " << method.isAsync << ", " << static_cast<int>(method.visibility) << ", "
                    << (callable ? "&" + name : "nullptr") << '}';
                methodRows.push_back(row.str());
            }
        }
        // Implicit default construction is available only when no user constructor exists.
        if (r.isExported && !layout.hasConstructor && layout.nominalKind == NominalKind::Component) {
            const auto name = p + "_default";
            begin(name, 0);
            out << "        *result = " << codec(r.type) << "::write(" << cpp(r.type) << "{});\n";
            end();
            ctorRows.push_back("{" + std::to_string(stableModuleHash(std::to_string(r.stableTypeId) + ":default")) +
                               "ULL, 0, nullptr, &" + name + "}");
        }
        record.methods = methodRows.size();
        record.ctors = ctorRows.size();
        if (!methodRows.empty()) {
            out << "static const WioNativeMethodDescriptor " << p << "_methods[] = {\n";
            for (const auto& row : methodRows)
                out << "    " << row << ",\n";
            out << "};\n";
        }
        if (!ctorRows.empty()) {
            out << "static const WioNativeConstructorDescriptor " << p << "_ctors[] = {\n";
            for (const auto& row : ctorRows)
                out << "    " << row << ",\n";
            out << "};\n";
        }
        if (!r.cases.empty()) {
            out << "static const WioNativeCaseDescriptor " << p << "_cases[] = {\n";
            for (const auto& c : r.cases) {
                auto found = std::ranges::find(layout.enumCases, c.name, &EnumCaseLayout::name);
                out << "    {" << c.stableId << "ULL, " << WirCppText::quote(c.name) << ", "
                    << (found == layout.enumCases.end() ? 0 : found->rawValue) << "ULL},\n";
            }
            out << "};\n";
        }
        records.push_back(record);
    }
    if (!records.empty()) {
        out << "static const WioNativeTypeDescriptor _reflect_types[] = {\n";
        for (const auto& rec : records) {
            const auto& r = *rec.r;
            const auto& p = rec.prefix;
            out << "    {" << r.stableTypeId << "ULL, " << WirCppText::quote(r.logicalName) << ", "
                << static_cast<int>(r.nominalKind) << ", " << r.isExported << ", sizeof(" << cpp(r.type)
                << "), alignof(" << cpp(r.type) << "), " << r.fields.size() << ", "
                << (r.fields.empty() ? "nullptr" : p + "_fields") << ", " << rec.methods << ", "
                << (rec.methods ? p + "_methods" : "nullptr") << ", " << r.cases.size() << ", "
                << (r.cases.empty() ? "nullptr" : p + "_cases") << ", " << rec.ctors << ", "
                << (rec.ctors ? p + "_ctors" : "nullptr") << "},\n";
        }
        out << "};\n";
    }
    std::vector<const AttributeApplicationDescriptor*> attributes;
    for (const auto& a : m.contract.attributes)
        if (a.runtimeRetained) {
            auto p = "_attr_" + std::to_string(a.stableId);
            if (!a.arguments.empty()) {
                out << "static const WioNativeAttributeArgument " << p << "_args[] = {\n";
                for (const auto& v : a.arguments)
                    out << "    {" << WirCppText::quote(v.name) << ", " << WirCppText::quote(v.sourceText) << ", "
                        << (v.type ? id(v.type) : 0) << "ULL, " << v.usedDefault << "},\n";
                out << "};\n";
            }
            if (!a.processors.empty()) {
                out << "static const WioNativeAttributeProcessor " << p << "_processors[] = {\n";
                for (const auto& v : a.processors)
                    out << "    {" << v.stableId << "ULL, " << WirCppText::quote(v.canonicalTypeName) << ", "
                        << WirCppText::quote(v.hookName) << ", " << WirCppText::quote(v.hookMode) << ", "
                        << static_cast<int>(v.phase) << "},\n";
                out << "};\n";
            }
            attributes.push_back(&a);
        }
    if (!attributes.empty()) {
        out << "static const WioNativeAttributeDescriptor _reflect_attributes[] = {\n";
        for (auto* a : attributes) {
            auto p = "_attr_" + std::to_string(a->stableId);
            out << "    {" << a->stableId << "ULL, " << WirCppText::quote(a->canonicalName) << ", " << a->targetStableId
                << "ULL, " << static_cast<int>(a->targetKind) << ", " << static_cast<int>(a->origin) << ", "
                << WirCppText::quote(a->originParent) << ", " << WirCppText::quote(a->selector) << ", "
                << a->parameterIndex << ", " << a->sourceOrder << ", " << a->arguments.size() << ", "
                << (a->arguments.empty() ? "nullptr" : p + "_args") << ", " << a->processors.size() << ", "
                << (a->processors.empty() ? "nullptr" : p + "_processors") << "},\n";
        }
        out << "};\n";
    }
    out << "static const WioNativeReflectionApi _reflect_api{WIO_NATIVE_REFLECTION_VERSION, " << records.size() << ", "
        << (records.empty() ? "nullptr" : "_reflect_types") << ", " << attributes.size() << ", "
        << (attributes.empty() ? "nullptr" : "_reflect_attributes")
        << "};\n"
           "extern \"C\" WIO_WIR_EXPORT const WioNativeReflectionApi* WioGetNativeReflectionApi() noexcept {\n"
           "    return &_reflect_api;\n"
           "}\n";
    return out.str();
}
} // namespace wio::codegen
