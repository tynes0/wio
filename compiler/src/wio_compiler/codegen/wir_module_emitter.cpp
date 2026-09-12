#include "wio/codegen/wir_module_emitter.h"
#include "wio/codegen/wir_application_emitter.h"
#include "wio/codegen/wir_reflection_emitter.h"
#include "wio/wir/native_abi_types.h"
#include <iomanip>
#include <map>
#include <sstream>
#include <set>

namespace wio::codegen {
namespace {
using namespace wir;
std::string quoted(const std::string& value) {
    std::ostringstream s;
    s << std::quoted(value);
    return s.str();
}
std::string functionName(FunctionId id) {
    return "_wio_f" + std::to_string(id.value());
}
std::string legacyKind(const Type& t) {
    switch (t.kind) {
    case TypeKind::Void:
        return "VOID";
    case TypeKind::Bool:
        return "BOOL";
    case TypeKind::I8:
        return "I8";
    case TypeKind::I16:
        return "I16";
    case TypeKind::I32:
        return "I32";
    case TypeKind::I64:
        return "I64";
    case TypeKind::U8:
        return "U8";
    case TypeKind::U16:
        return "U16";
    case TypeKind::U32:
        return "U32";
    case TypeKind::U64:
        return "U64";
    case TypeKind::ISize:
        return "ISIZE";
    case TypeKind::USize:
        return "USIZE";
    case TypeKind::F32:
        return "F32";
    case TypeKind::F64:
        return "F64";
    case TypeKind::Char:
        return "CHAR";
    case TypeKind::Byte:
        return "BYTE";
    default:
        return "UNKNOWN";
    }
}
std::string legacyField(std::string kind) {
    for (char& c : kind)
        c = static_cast<char>(std::tolower(c));
    return "v_" + kind;
}
} // namespace
std::string WirModuleEmitter::emit(const wir::lowered::Module& module, const TypeSpelling& type) {
    std::ostringstream out;
    std::map<FunctionId, const lowered::Function*> functions;
    for (const auto& f : module.functions)
        functions[f.id] = &f;
    const auto wireId = [&](TypeId id) {
        for (const auto& reflected : module.contract.reflection)
            if (reflected.type == id)
                return reflected.stableTypeId;
        return stableModuleHash(nativeAbiTypeKey(module.types, id));
    };
    const auto codec = [&](TypeId id) {
        return "wio::wir_backend::abi::Codec<" + type(id) + ", " + std::to_string(wireId(id)) + "ULL>";
    };
    std::function<bool(TypeId)> concrete = [&](TypeId id) {
        const auto& t = module.types.get(id);
        if (t.kind == TypeKind::GenericParameter || t.kind == TypeKind::ConstGenericParameter ||
            t.kind == TypeKind::GenericParameterPack || t.kind == TypeKind::TypePack || t.kind == TypeKind::ValuePack)
            return false;
        for (auto a : t.arguments)
            if (!concrete(a))
                return false;
        return true;
    };
    out << "#if defined(_WIN32)\n#define WIO_WIR_EXPORT __declspec(dllexport)\n#else\n#define WIO_WIR_EXPORT "
           "__attribute__((visibility(\"default\")))\n#endif\n";
    const auto thunk = [&](const lowered::Function& f, const std::string& name) {
        out << "static WioNativeAbiStatus " << name
            << "(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result, WioNativeAbiFailure* "
               "failure) noexcept {\n"
               "  if (failure) *failure = {};\n  if (count != "
            << f.parameters.size()
            << " || (count && !args) || !result) return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
               "  *result = {}; wio::wir_backend::abi::Frame frame;\n  try {\n";
        for (std::size_t i = 0; i < f.parameters.size(); ++i) {
            auto id = f.parameters[i].type;
            const auto& t = module.types.get(id);
            out << "    auto p" << i << " = ";
            if (t.kind == TypeKind::Reference) {
                auto base = t.arguments.front();
                out << type(id) << (t.isMutable ? "::borrow(" : "::borrowView(") << "frame.borrow<" << type(base)
                    << ", " << wireId(base) << "ULL>(args[" << i << "], " << (t.isMutable ? "true" : "false") << "))";
            } else
                out << codec(id) << "::read(args[" << i << "])";
            out << ";\n";
        }
        const auto& r = module.types.get(f.returnType);
        out << "    ";
        if (r.kind != TypeKind::Void)
            out << "auto value = ";
        out << functionName(f.id) << '(';
        for (std::size_t i = 0; i < f.parameters.size(); ++i) {
            if (i)
                out << ',';
            out << "p" << i;
        }
        out << ");\n    frame.commit();\n";
        if (r.kind == TypeKind::AsyncTask)
            out << "    wio::wir_backend::abi::Tasks::get().track(value);\n";
        if (r.kind != TypeKind::Void && r.kind != TypeKind::Reference)
            out << "    *result = " << codec(f.returnType) << "::write(std::move(value));\n";
        else if (r.kind != TypeKind::Void)
            out << "    return WIO_NATIVE_ABI_TYPE_MISMATCH;\n";
        out << "    return WIO_NATIVE_ABI_OK;\n"
               "  } catch (const wio::wir_backend::abi::Error& e) {\n"
               "    try { frame.commit(); } catch (...) {} return "
               "wio::wir_backend::abi::failure(failure,e.status,e.what());\n"
               "  } catch (const std::exception& e) {\n"
               "    try { frame.commit(); } catch (...) {} return "
               "wio::wir_backend::abi::failure(failure,WIO_NATIVE_ABI_EXCEPTION,e.what());\n"
               "  } catch (...) { try { frame.commit(); } catch (...) {} return "
               "wio::wir_backend::abi::failure(failure,WIO_NATIVE_ABI_EXCEPTION,\"unknown native failure\"); }\n}\n";
    };
    std::vector<const lowered::Function*> natives;
    std::set<std::string> nativeKeys;
    for (const auto& f : module.functions)
        if (f.nativeBinding && f.genericParameters.empty() && concrete(f.callableType)) {
            // Multiple Wio aliases may name the same concrete C++ binding.
            if (!nativeKeys.insert(f.nativeBinding->stableKey).second)
                continue;
            thunk(f, f.nativeBinding->thunkSymbol);
            natives.push_back(&f);
        }
    if (!natives.empty()) {
        out << "static const WioNativeAbiFunctionDescriptor _native_functions[] = {\n";
        for (auto* f : natives) {
            const auto& b = *f->nativeBinding;
            out << "{WIO_NATIVE_ABI_VERSION,0," << stableModuleHash(b.stableKey) << "ULL," << quoted(b.stableKey) << ','
                << quoted(b.symbol) << ',' << quoted(b.thunkSymbol) << ",&" << b.thunkSymbol << "},\n";
        }
        out << "};\n";
    }
    out << "static const WioNativeAbiRegistry _native_registry{WIO_NATIVE_ABI_VERSION," << natives.size() << ','
        << (natives.empty() ? "nullptr" : "_native_functions")
        << "};\n"
           "extern \"C\" WIO_WIR_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry() noexcept { return "
           "&_native_registry; }\n";
    out << WirApplicationEmitter::emit(module, type);
    out << WirReflectionEmitter::emit(module, type, thunk);
    if (module.contract.kind != ModuleKind::WioLibrary)
        return out.str();
    std::set<TypeId> taskTypes;
    for (const auto& f : module.functions)
        if (module.types.get(f.returnType).kind == TypeKind::AsyncTask && concrete(f.returnType))
            taskTypes.insert(f.returnType);
    for (const std::string action : {"ready", "cancel", "read"}) {
        out << "static WioNativeAbiStatus _task_" << action << "(const WioNativeAbiValue* task"
            << (action == "ready"  ? ",bool* ready"
                : action == "read" ? ",WioNativeAbiValue* result,WioNativeAbiFailure* error"
                                   : "")
            << ") noexcept {\n"
               " if (!task"
            << (action == "ready"  ? " || !ready"
                : action == "read" ? " || !result"
                                   : "")
            << ") return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n try {\n";
        for (auto id : taskTypes) {
            const auto payload = module.types.get(id).arguments.front();
            out << " if (task->typeId==" << wireId(id) << "ULL) { auto value=" << codec(id) << "::read(*task); ";
            if (action == "ready")
                out << "*ready=value.IsReady();";
            else if (action == "cancel")
                out << "value.Cancel();";
            else {
                out << "*result={}; if (!value.IsReady()) return WIO_NATIVE_ABI_NOT_READY; ";
                if (module.types.get(payload).kind == TypeKind::Void)
                    out << "value.Get();";
                else
                    out << "*result=" << codec(payload) << "::write(value.Get());";
            }
            out << " return WIO_NATIVE_ABI_OK; }\n";
        }
        out << " return WIO_NATIVE_ABI_TYPE_MISMATCH;\n } catch (const std::exception& e) { return "
               "wio::wir_backend::abi::failure("
            << (action == "read" ? "error" : "nullptr")
            << ",WIO_NATIVE_ABI_EXCEPTION,e.what()); } catch (...) { return WIO_NATIVE_ABI_EXCEPTION; } }\n";
    }
    out << "static const WioNativeTaskApi _task_api{WIO_NATIVE_ABI_VERSION,&_task_ready,&_task_cancel,&_task_read,"
           "+[]() noexcept { try { wio::runtime::BindAsyncMainExecutor(); } catch (...) {} },"
           "+[]() noexcept -> std::uint64_t { try { return wio::runtime::DrainAsyncMainExecutor(); } catch (...) { "
           "return 0; } },"
           "+[]() noexcept { wio::wir_backend::abi::Tasks::get().shutdown(); }};\n"
           "extern \"C\" WIO_WIR_EXPORT const WioNativeTaskApi* WioGetNativeTaskApi() noexcept { return &_task_api; "
           "}\n";
    const auto& exports = module.contract.exports;
    std::vector<const ModuleExport*> legacy;
    for (std::size_t i = 0; i < exports.size(); ++i) {
        const auto& e = exports[i];
        if (e.function)
            thunk(*functions.at(e.function), "_sdk_native" + std::to_string(i));
        out << "static std::int32_t _sdk_call" << i
            << "(const void* args, std::uint32_t count, void* result, void*) noexcept { ";
        if (e.function)
            out << "return _sdk_native" << i
                << "(static_cast<const "
                   "WioNativeAbiValue*>(args),count,static_cast<WioNativeAbiValue*>(result),nullptr);";
        else
            out << "return WIO_NATIVE_ABI_TYPE_MISMATCH;";
        out << " }\n";
        if (!e.function || e.isAsync || legacyKind(module.types.get(e.returnType)) == "UNKNOWN")
            continue;
        bool compatible = true;
        for (auto p : e.parameterTypes)
            compatible &= legacyKind(module.types.get(p)) != "UNKNOWN";
        if (!compatible)
            continue;
        legacy.push_back(&e);
        out << "static std::int32_t _legacy" << i
            << "(const WioValue* args, std::uint32_t count, WioValue* result) noexcept {\n"
               " if (count != "
            << e.parameterTypes.size() << " || (count && !args) || !result) return WIO_INVOKE_BAD_ARGUMENTS;\n try {\n";
        for (std::size_t p = 0; p < e.parameterTypes.size(); ++p)
            out << " if (args[" << p << "].type != WIO_ABI_" << legacyKind(module.types.get(e.parameterTypes[p]))
                << ") return WIO_INVOKE_TYPE_MISMATCH;\n";
        const auto resultKind = legacyKind(module.types.get(e.returnType));
        out << " result->type = WIO_ABI_" << resultKind << "; ";
        if (resultKind != "VOID")
            out << "result->value." << legacyField(resultKind) << " = ";
        out << functionName(e.function) << '(';
        for (std::size_t p = 0; p < e.parameterTypes.size(); ++p) {
            if (p)
                out << ',';
            out << "args[" << p << "].value." << legacyField(legacyKind(module.types.get(e.parameterTypes[p])));
        }
        out << "); return WIO_INVOKE_OK; } catch (...) { return WIO_INVOKE_NOT_CALLABLE; } }\n";
        if (!e.parameterTypes.empty()) {
            out << "static const WioAbiType _legacy_params" << i << "[] = {";
            for (auto p : e.parameterTypes)
                out << "WIO_ABI_" << legacyKind(module.types.get(p)) << ',';
            out << "};\n";
        }
    }
    if (!exports.empty()) {
        out << "static const WioSdkExportDescriptor _sdk_exports[] = {\n";
        for (const auto& e : exports)
            out << '{' << e.stableId << "ULL," << quoted(e.stableKey) << ',' << quoted(e.logicalName) << ','
                << quoted(e.symbolName) << ',' << quoted(e.roleName) << ',' << e.callTableSlot
                << ",static_cast<WioSdkExportKind>(" << static_cast<int>(e.kind) << "),static_cast<WioSdkExportRole>("
                << static_cast<int>(e.role) << "),WIO_SDK_CALL_NATIVE_ABI_V2},\n";
        out << "};\nstatic const WioSdkCallEntry _sdk_calls[] = {\n";
        for (std::size_t i = 0; i < exports.size(); ++i)
            out << '{' << exports[i].stableId << "ULL,&_sdk_call" << i << ",nullptr},\n";
        out << "};\n";
    }
    if (!module.contract.reflection.empty()) {
        out << "static const WioSdkReflectionDescriptor _sdk_reflection[] = {\n";
        for (const auto& r : module.contract.reflection)
            out << '{' << r.stableTypeId << "ULL," << quoted(r.logicalName) << ',' << static_cast<int>(r.nominalKind)
                << ',' << (r.isExported ? 1 : 0) << "},\n";
        out << "};\n";
    }
    if (!legacy.empty()) {
        out << "static const WioModuleExport _legacy_exports[] = {\n";
        for (auto* e : legacy)
            out << '{' << quoted(e->logicalName) << ',' << quoted(e->symbolName) << ",WIO_ABI_"
                << legacyKind(module.types.get(e->returnType)) << ',' << e->parameterTypes.size() << ','
                << (e->parameterTypes.empty() ? "nullptr" : "_legacy_params" + std::to_string(e->callTableSlot))
                << ",&_legacy" << e->callTableSlot << ",nullptr,0,nullptr},\n";
        out << "};\n";
    }
    const auto& l = module.contract.lifecycle;
    std::uint32_t flags = (l.apiVersion ? 1 : 0) | (l.load ? 2 : 0) | (l.update ? 4 : 0) | (l.unload ? 8 : 0) |
                          (l.saveState ? 16 : 0) | (l.restoreState ? 32 : 0);
    const auto hook = [&](FunctionId id, const std::string& result, const std::string& parameter,
                          const std::string& arg, const std::string& fallback) {
        if (!id)
            return std::string("nullptr");
        return "+[](" + parameter + ") noexcept -> " + result + " { try { return " + functionName(id) + "(" + arg +
               "); } catch (...) { " + fallback + " } }";
    };
    out << "static const WioSdkModuleContract "
           "_sdk_contract{WIO_SDK_MODULE_CONTRACT_VERSION,sizeof(WioSdkModuleContract),WIO_SDK_MODULE_WIO_LIBRARY,"
        << l.stateSchemaVersion << ',' << module.contract.stableId << "ULL," << quoted(module.contract.stableKey) << ','
        << flags << ',' << exports.size() << ',' << (exports.empty() ? "nullptr" : "_sdk_exports") << ','
        << module.contract.reflection.size() << ','
        << (module.contract.reflection.empty() ? "nullptr" : "_sdk_reflection") << ',' << exports.size() << ','
        << (exports.empty() ? "nullptr" : "_sdk_calls") << "};\n";
    out << "static const WioModuleApi _legacy_api = [] { WioModuleApi a{}; "
           "a.descriptorVersion=WIO_MODULE_API_DESCRIPTOR_VERSION; a.descriptorSize=sizeof(a); a.capabilities="
        << flags << "; a.stateSchemaVersion=" << l.stateSchemaVersion << ';'
        << "a.apiVersion=" << hook(l.apiVersion, "std::uint32_t", "", "", "return 0;") << ';'
        << "a.load=" << hook(l.load, "std::int32_t", "", "", "return -1;") << ';'
        << "a.update=" << hook(l.update, "void", "float delta", "delta", "") << ';'
        << "a.unload=" << hook(l.unload, "void", "", "", "") << ';'
        << "a.saveState=" << hook(l.saveState, "std::int32_t", "", "", "return -1;") << ';'
        << "a.restoreState=" << hook(l.restoreState, "std::int32_t", "std::int32_t state", "state", "return -1;") << ';'
        << (module.contract.application
                ? "a.application=&_wio_application; a.capabilities|=WIO_MODULE_CAP_APPLICATION_HOST_V1;"
                : "")
        << (module.contract.application && !module.contract.application->stages.empty()
                ? "a.capabilities|=WIO_MODULE_CAP_APPLICATION_SCHEDULE_V1;"
                : "")
        << "a.exportCount=" << legacy.size() << "; a.exports=" << (legacy.empty() ? "nullptr" : "_legacy_exports")
        << "; return a; }();\n"
        << "extern \"C\" WIO_WIR_EXPORT const WioModuleApi* WioModuleGetApi() noexcept { return &_legacy_api; }\n"
        << "extern \"C\" WIO_WIR_EXPORT const WioSdkModuleContract* WioGetSdkModuleContract() noexcept { return "
           "&_sdk_contract; }\n";
    return out.str();
}
} // namespace wio::codegen
