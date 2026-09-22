#include "wio/codegen/wir_module_emitter.h"
#include "wio/codegen/wir_application_emitter.h"
#include "wio/codegen/wir_cpp_text.h"
#include "wio/codegen/wir_reflection_emitter.h"
#include "wio/wir/native_abi_types.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace wio::codegen
{
    namespace
    {
        using namespace wir;
        std::string quoted(const std::string& value)
        {
            return WirCppText::quote(value);
        }
        std::string functionName(FunctionId id)
        {
            return "_wio_f" + std::to_string(id.value());
        }
        std::string legacyKind(const Type& t)
        {
            switch (t.kind)
            {
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
        std::string legacyField(std::string kind)
        {
            for (char& c : kind)
                c = static_cast<char>(std::tolower(c));
            return "v_" + kind;
        }

        std::string nominalCppType(TypeId id)
        {
            return "_wio_obj" + std::to_string(id.value());
        }

        std::string legacyAccess(FieldVisibility visibility)
        {
            switch (visibility)
            {
            case FieldVisibility::Public:
                return "WIO_MODULE_ACCESS_PUBLIC";
            case FieldVisibility::Protected:
                return "WIO_MODULE_ACCESS_PROTECTED";
            case FieldVisibility::Private:
                return "WIO_MODULE_ACCESS_PRIVATE";
            }
            return "WIO_MODULE_ACCESS_UNKNOWN";
        }

        std::string legacyAttributeOrigin(AttributeOriginKind origin)
        {
            switch (origin)
            {
            case AttributeOriginKind::Composed:
                return "WIO_MODULE_ATTRIBUTE_COMPOSED";
            case AttributeOriginKind::Scoped:
                return "WIO_MODULE_ATTRIBUTE_SCOPED";
            default:
                return "WIO_MODULE_ATTRIBUTE_DIRECT";
            }
        }

        std::string legacyProcessorPhase(AttributeProcessorPhase phase)
        {
            switch (phase)
            {
            case AttributeProcessorPhase::Validation:
                return "WIO_MODULE_ATTRIBUTE_PHASE_VALIDATION";
            case AttributeProcessorPhase::Derive:
                return "WIO_MODULE_ATTRIBUTE_PHASE_DERIVE";
            case AttributeProcessorPhase::Pre:
                return "WIO_MODULE_ATTRIBUTE_PHASE_PRE";
            case AttributeProcessorPhase::Post:
                return "WIO_MODULE_ATTRIBUTE_PHASE_POST";
            case AttributeProcessorPhase::Finally:
                return "WIO_MODULE_ATTRIBUTE_PHASE_FINALLY";
            case AttributeProcessorPhase::Around:
                return "WIO_MODULE_ATTRIBUTE_PHASE_AROUND";
            case AttributeProcessorPhase::Unknown:
                return "WIO_MODULE_ATTRIBUTE_PHASE_UNKNOWN";
            }
            return "WIO_MODULE_ATTRIBUTE_PHASE_UNKNOWN";
        }

        struct LegacyExportInfo
        {
            std::string logicalName;
            std::string symbolName;
            std::string returnKind;
            std::vector<std::string> parameterKinds;
            std::string invokeName;
            std::string rawFunction = "nullptr";
            std::string attributes = "nullptr";
            std::uint32_t attributeCount = 0;
            const ModuleExport* contractExport = nullptr;
        };

        struct LegacyFieldInfo
        {
            const ReflectedFieldDescriptor* descriptor = nullptr;
            TypeId ownerType;
            std::size_t ownerFieldIndex = 0;
            std::size_t typeDescriptor = 0;
            std::optional<std::size_t> getterExport;
            std::optional<std::size_t> setterExport;
            std::string dynamicGetter = "nullptr";
            std::string dynamicSetter = "nullptr";
            std::string attributes = "nullptr";
            std::uint32_t attributeCount = 0;
        };

        struct LegacyMethodInfo
        {
            const ReflectedMethodDescriptor* descriptor = nullptr;
            std::size_t exportIndex = 0;
            std::string attributes = "nullptr";
            std::uint32_t attributeCount = 0;
        };

        struct LegacyTypeInfo
        {
            const ReflectionDescriptor* descriptor = nullptr;
            std::optional<std::size_t> createExport;
            std::size_t destroyExport = 0;
            std::vector<std::size_t> constructors;
            std::vector<LegacyFieldInfo> fields;
            std::vector<LegacyMethodInfo> methods;
            std::string attributes = "nullptr";
            std::uint32_t attributeCount = 0;
        };

        struct LegacyTypeDescriptorInfo
        {
            struct EnumMember
            {
                std::string name;
                std::string value;
            };

            std::string displayName;
            std::string logicalTypeName;
            std::string kind = "WIO_MODULE_TYPE_DESC_UNKNOWN";
            std::string abi = "WIO_ABI_UNKNOWN";
            std::uint64_t staticExtent = 0;
            std::optional<std::size_t> element;
            std::optional<std::size_t> key;
            std::optional<std::size_t> value;
            std::optional<std::size_t> result;
            std::vector<std::size_t> parameters;
            std::vector<EnumMember> enumMembers;
            std::vector<std::size_t> genericArguments;
            std::optional<std::size_t> constValueType;
            std::optional<std::string> constValue;
        };
    } // namespace
    std::string WirModuleEmitter::emit(const wir::lowered::Module& module, const TypeSpelling& type)
    {
        std::ostringstream out;
        std::map<FunctionId, const lowered::Function*> functions;
        for (const auto& f : module.functions)
            functions[f.id] = &f;
        const auto wireId = [&](TypeId id)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == id)
                    return reflected.stableTypeId;
            return stableModuleHash(nativeAbiTypeKey(module.types, id));
        };
        const auto codec = [&](TypeId id)
        { return "wio::wir_backend::abi::Codec<" + type(id) + ", " + std::to_string(wireId(id)) + "ULL>"; };
        std::function<bool(TypeId)> concrete = [&](TypeId id)
        {
            const auto& t = module.types.get(id);
            if (t.kind == TypeKind::GenericParameter || t.kind == TypeKind::ConstGenericParameter ||
                t.kind == TypeKind::GenericParameterPack || t.kind == TypeKind::TypePack ||
                t.kind == TypeKind::ValuePack)
                return false;
            for (auto a : t.arguments)
                if (!concrete(a))
                    return false;
            return true;
        };
        std::function<std::string(TypeId)> legacyAbiKind = [&](TypeId id)
        {
            const auto& valueType = module.types.get(id);
            if (valueType.kind == TypeKind::Named &&
                (valueType.nominalKind == NominalKind::Enum || valueType.nominalKind == NominalKind::Flagset) &&
                valueType.enumUnderlyingType)
                return legacyAbiKind(valueType.enumUnderlyingType);
            return legacyKind(valueType);
        };
        const auto storageCppType = [&](TypeId id)
        { return module.types.get(id).nominalKind == NominalKind::Object ? nominalCppType(id) : type(id); };
        out << "#if defined(_WIN32)\n#define WIO_WIR_EXPORT __declspec(dllexport)\n#else\n#define WIO_WIR_EXPORT "
               "__attribute__((visibility(\"default\")))\n#endif\n";
        const auto thunk = [&](const lowered::Function& f, const std::string& name)
        {
            out << "static WioNativeAbiStatus " << name
                << "(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result, "
                   "WioNativeAbiFailure* "
                   "failure) noexcept {\n"
                   "  if (failure) *failure = {};\n  if (count != "
                << f.parameters.size()
                << " || (count && !args) || !result) return WIO_NATIVE_ABI_INVALID_ARGUMENT;\n"
                   "  *result = {}; wio::wir_backend::abi::Frame frame;\n  try {\n";
            for (std::size_t i = 0; i < f.parameters.size(); ++i)
            {
                auto id = f.parameters[i].type;
                const auto& t = module.types.get(id);
                out << "    auto p" << i << " = ";
                if (t.kind == TypeKind::Reference)
                {
                    auto base = t.arguments.front();
                    out << type(id) << (t.isMutable ? "::borrow(" : "::borrowView(") << "frame.borrow<" << type(base)
                        << ", " << wireId(base) << "ULL>(args[" << i << "], " << (t.isMutable ? "true" : "false")
                        << "))";
                }
                else
                    out << codec(id) << "::read(args[" << i << "])";
                out << ";\n";
            }
            const auto& r = module.types.get(f.returnType);
            out << "    ";
            if (r.kind != TypeKind::Void)
                out << "auto value = ";
            out << functionName(f.id) << '(';
            for (std::size_t i = 0; i < f.parameters.size(); ++i)
            {
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
                   "wio::wir_backend::abi::failure(failure,WIO_NATIVE_ABI_EXCEPTION,\"unknown native failure\"); "
                   "}\n}\n";
        };
        std::vector<const lowered::Function*> natives;
        std::set<std::string> nativeKeys;
        for (const auto& f : module.functions)
            if (f.nativeBinding && f.genericParameters.empty() && concrete(f.callableType))
            {
                // Multiple Wio aliases may name the same concrete C++ binding.
                if (!nativeKeys.insert(f.nativeBinding->stableKey).second)
                    continue;
                thunk(f, f.nativeBinding->thunkSymbol);
                natives.push_back(&f);
            }
        if (!natives.empty())
        {
            out << "static const WioNativeAbiFunctionDescriptor _native_functions[] = {\n";
            for (auto* f : natives)
            {
                const auto& b = *f->nativeBinding;
                out << "{WIO_NATIVE_ABI_VERSION,0," << stableModuleHash(b.stableKey) << "ULL," << quoted(b.stableKey)
                    << ',' << quoted(b.symbol) << ',' << quoted(b.thunkSymbol) << ",&" << b.thunkSymbol << "},\n";
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
        for (const std::string action : {"ready", "cancel", "read"})
        {
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
            for (auto id : taskTypes)
            {
                const auto payload = module.types.get(id).arguments.front();
                out << " if (task->typeId==" << wireId(id) << "ULL) { auto value=" << codec(id) << "::read(*task); ";
                if (action == "ready")
                    out << "*ready=value.IsReady();";
                else if (action == "cancel")
                    out << "value.Cancel();";
                else
                {
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
               "extern \"C\" WIO_WIR_EXPORT const WioNativeTaskApi* WioGetNativeTaskApi() noexcept { return "
               "&_task_api; "
               "}\n";
        const auto& exports = module.contract.exports;
        std::set<std::uint64_t> directExports;
        for (const auto& entry : exports)
        {
            if (!entry.function || entry.isAsync || entry.kind != ModuleExportKind::Function)
                continue;
            directExports.insert(entry.stableId);
            out << "extern \"C\" WIO_WIR_EXPORT " << type(entry.returnType) << ' ' << entry.symbolName << '(';
            for (std::size_t parameterIndex = 0; parameterIndex < entry.parameterTypes.size(); ++parameterIndex)
            {
                if (parameterIndex > 0)
                    out << ',';
                out << type(entry.parameterTypes[parameterIndex]) << " p" << parameterIndex;
            }
            out << "){";
            if (module.types.get(entry.returnType).kind != TypeKind::Void)
                out << "return ";
            out << functionName(entry.function) << '(';
            for (std::size_t parameterIndex = 0; parameterIndex < entry.parameterTypes.size(); ++parameterIndex)
            {
                if (parameterIndex > 0)
                    out << ',';
                out << "p" << parameterIndex;
            }
            out << ");}\n";
        }
        std::vector<std::size_t> legacyAsyncExports;
        for (std::size_t exportIndex = 0; exportIndex < exports.size(); ++exportIndex)
        {
            const auto& entry = exports[exportIndex];
            if (!entry.isAsync || !entry.function)
                continue;
            const auto& taskType = module.types.get(entry.returnType);
            if (taskType.kind != TypeKind::AsyncTask || taskType.arguments.size() != 1)
                continue;
            const auto resultType = taskType.arguments.front();
            const auto resultKind = legacyAbiKind(resultType);
            bool compatible = resultKind != "UNKNOWN";
            for (const auto parameter : entry.parameterTypes)
                compatible &= legacyAbiKind(parameter) != "UNKNOWN";
            if (!compatible)
                continue;

            legacyAsyncExports.push_back(exportIndex);
            const auto suffix = std::to_string(exportIndex);
            const auto stateType = "_legacy_async_state_" + suffix;
            out << "struct " << stateType << "{std::atomic<std::uint64_t> references{1u};" << type(entry.returnType)
                << " task;mutable std::mutex errorMutex{};mutable std::string lastError{};};\n";
            out << "static void _legacy_async_retain_" << suffix << "(void* opaque)noexcept{if(auto* state=static_cast<"
                << stateType << "*>(opaque))state->references.fetch_add(1u,std::memory_order_relaxed);}\n";
            out << "static void _legacy_async_release_" << suffix << "(void* opaque)noexcept{auto* state=static_cast<"
                << stateType
                << "*>(opaque);if(state&&state->references.fetch_sub(1u,std::memory_order_acq_rel)==1u)delete "
                   "state;}\n";
            out << "static WioAsyncTaskStatus _legacy_async_status_" << suffix << "(const void* opaque)noexcept{"
                << "const auto* state=static_cast<const " << stateType
                << "*>(opaque);if(!state||!state->task.IsReady())return WIO_ASYNC_TASK_PENDING;"
                   "if(state->task.IsCancelled())return WIO_ASYNC_TASK_CANCELLED;"
                   "if(state->task.IsFaulted())return WIO_ASYNC_TASK_FAULTED;return WIO_ASYNC_TASK_READY;}\n";
            out << "static void _legacy_async_cancel_" << suffix << "(void* opaque)noexcept{try{if(auto* state="
                << "static_cast<" << stateType << "*>(opaque))state->task.Cancel();}catch(...){}}\n";
            out << "static std::int32_t _legacy_async_wait_" << suffix
                << "(void* opaque,std::uint64_t milliseconds)noexcept{auto* state=static_cast<" << stateType
                << "*>(opaque);if(!state)return WIO_ASYNC_BAD_ARGUMENTS;try{auto initial=_legacy_async_status_"
                << suffix
                << "(state);if(initial==WIO_ASYNC_TASK_CANCELLED)return WIO_ASYNC_CANCELLED;"
                   "if(initial==WIO_ASYNC_TASK_FAULTED)return WIO_ASYNC_FAULTED;"
                   "if(!state->task.WaitFor(milliseconds))return WIO_ASYNC_TIMED_OUT;auto status="
                << "_legacy_async_status_" << suffix
                << "(state);return status==WIO_ASYNC_TASK_CANCELLED?WIO_ASYNC_CANCELLED:"
                   "(status==WIO_ASYNC_TASK_FAULTED?WIO_ASYNC_FAULTED:WIO_ASYNC_OK);}catch(...){return "
                   "WIO_ASYNC_FAULTED;}}\n";
            out << "static std::int32_t _legacy_async_result_" << suffix
                << "(void* opaque,WioValue* result)noexcept{auto* state=static_cast<" << stateType
                << "*>(opaque);if(!state||!result)return WIO_ASYNC_BAD_ARGUMENTS;auto status=_legacy_async_status_"
                << suffix
                << "(state);if(status==WIO_ASYNC_TASK_PENDING)return WIO_ASYNC_NOT_READY;"
                   "if(status==WIO_ASYNC_TASK_CANCELLED)return WIO_ASYNC_CANCELLED;"
                   "if(status==WIO_ASYNC_TASK_FAULTED)return WIO_ASYNC_FAULTED;try{";
            if (module.types.get(resultType).kind == TypeKind::Void)
            {
                out << "state->task.Get();result->type=WIO_ABI_VOID;";
            }
            else
            {
                out << "auto value=state->task.Get();result->type=WIO_ABI_" << resultKind << ";result->value."
                    << legacyField(resultKind) << "=value;";
            }
            out << "return WIO_ASYNC_OK;}catch(const std::exception& error){std::lock_guard lock(state->errorMutex);"
                   "state->lastError=error.what();return state->task.IsCancelled()?WIO_ASYNC_CANCELLED:"
                   "WIO_ASYNC_FAULTED;}catch(...){return WIO_ASYNC_FAULTED;}}\n";
            out << "static std::int32_t _legacy_async_complete_" << suffix
                << "(void* opaque,WioAsyncCompletionFn callback,void* userData,WioAsyncCompletionTarget target)"
                   "noexcept{auto* state=static_cast<"
                << stateType
                << "*>(opaque);if(!state||!callback)return WIO_ASYNC_BAD_ARGUMENTS;"
                   "if(target!=WIO_ASYNC_COMPLETION_CURRENT_EXECUTOR&&target!=WIO_ASYNC_COMPLETION_MAIN_EXECUTOR)"
                   "return WIO_ASYNC_BAD_ARGUMENTS;_legacy_async_retain_"
                << suffix
                << "(state);try{state->task.SharedState()->AddCompletionCallback([state,callback,userData,target]{"
                   "auto deliver=[state,callback,userData]{try{callback(userData,_legacy_async_status_"
                << suffix << "(state));}catch(...){}_legacy_async_release_" << suffix
                << "(state);};if(target==WIO_ASYNC_COMPLETION_MAIN_EXECUTOR){if(!wio::runtime::"
                   "DefaultAsyncMainExecutor().Post(std::move(deliver)))deliver();}else deliver();});return "
                   "WIO_ASYNC_OK;}catch(...){_legacy_async_release_"
                << suffix << "(state);return WIO_ASYNC_FAULTED;}}\n";
            out << "static const char* _legacy_async_error_" << suffix
                << "(const void* opaque)noexcept{const auto* state=static_cast<const " << stateType
                << "*>(opaque);if(!state)return \"async task state is null\";std::lock_guard lock(state->errorMutex);"
                   "auto message=state->task.SharedState()->FailureMessage();if(!message.empty())state->lastError="
                   "std::move(message);return state->lastError.c_str();}\n";
            out << "static const WioAsyncTaskOps _legacy_async_ops_" << suffix << "{&_legacy_async_retain_" << suffix
                << ",&_legacy_async_release_" << suffix << ",&_legacy_async_status_" << suffix
                << ",&_legacy_async_cancel_" << suffix << ",&_legacy_async_wait_" << suffix << ",&_legacy_async_result_"
                << suffix << ",&_legacy_async_complete_" << suffix << ",&_legacy_async_error_" << suffix << "};\n";
            out << "static std::int32_t _legacy_async_invoke_" << suffix
                << "(const WioValue* args,std::uint32_t count,WioAsyncTaskHandle* result)noexcept{if(!result||count!="
                << entry.parameterTypes.size() << "u||(count&&!args))return WIO_ASYNC_BAD_ARGUMENTS;";
            for (std::size_t parameterIndex = 0; parameterIndex < entry.parameterTypes.size(); ++parameterIndex)
                out << "if(args[" << parameterIndex << "].type!=WIO_ABI_"
                    << legacyAbiKind(entry.parameterTypes[parameterIndex]) << ")return WIO_ASYNC_TYPE_MISMATCH;";
            out << "try{auto* state=new " << stateType << "{1u," << functionName(entry.function) << '(';
            for (std::size_t parameterIndex = 0; parameterIndex < entry.parameterTypes.size(); ++parameterIndex)
            {
                if (parameterIndex)
                    out << ',';
                out << "args[" << parameterIndex << "].value."
                    << legacyField(legacyAbiKind(entry.parameterTypes[parameterIndex]));
            }
            out << ")};*result={state,&_legacy_async_ops_" << suffix << ",WIO_ABI_" << resultKind
                << "};return WIO_ASYNC_OK;}catch(...){return WIO_ASYNC_FAULTED;}}\n";
            if (!entry.parameterTypes.empty())
            {
                out << "static const WioAbiType _legacy_async_params_" << suffix << "[]={";
                for (const auto parameter : entry.parameterTypes)
                    out << "WIO_ABI_" << legacyAbiKind(parameter) << ',';
                out << "};\n";
            }
        }
        if (!legacyAsyncExports.empty())
        {
            out << "static const WioModuleAsyncExport _legacy_async_exports[]={";
            for (const auto exportIndex : legacyAsyncExports)
            {
                const auto& entry = exports[exportIndex];
                const auto resultType = module.types.get(entry.returnType).arguments.front();
                out << '{' << quoted(entry.logicalName) << ",WIO_ABI_" << legacyAbiKind(resultType) << ','
                    << entry.parameterTypes.size() << "u,"
                    << (entry.parameterTypes.empty() ? "nullptr"
                                                     : "_legacy_async_params_" + std::to_string(exportIndex))
                    << ",&_legacy_async_invoke_" << exportIndex << "},";
            }
            out << "};\nstatic const WioAsyncHostDescriptor _legacy_async_host{0u,0u,"
                   "+[]()noexcept{try{wio::runtime::BindAsyncMainExecutor();}catch(...){}},"
                   "+[]()noexcept->std::uint64_t{try{return wio::runtime::DrainAsyncMainExecutor();}catch(...){return "
                   "0u;}},+[]()noexcept->std::uint64_t{return wio::runtime::AsyncMainPendingCount();},"
                   "+[]()noexcept{wio::runtime::ShutdownAsyncRuntime();}};\n";
        }
        std::vector<LegacyExportInfo> legacy;
        std::map<std::uint64_t, std::size_t> legacySlots;
        for (std::size_t i = 0; i < exports.size(); ++i)
        {
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
            if (!e.function || e.isAsync || legacyAbiKind(e.returnType) == "UNKNOWN")
                continue;
            bool compatible = true;
            for (auto p : e.parameterTypes)
                compatible &= legacyAbiKind(p) != "UNKNOWN";
            if (!compatible)
                continue;
            legacySlots.emplace(e.stableId, legacy.size());
            LegacyExportInfo legacyExport;
            legacyExport.logicalName = e.logicalName;
            legacyExport.symbolName = e.symbolName;
            legacyExport.returnKind = legacyAbiKind(e.returnType);
            legacyExport.invokeName = "_legacy" + std::to_string(i);
            if (directExports.contains(e.stableId))
                legacyExport.rawFunction = "reinterpret_cast<const void*>(&" + e.symbolName + ")";
            legacyExport.contractExport = &e;
            for (auto parameter : e.parameterTypes)
                legacyExport.parameterKinds.push_back(legacyAbiKind(parameter));
            legacy.push_back(std::move(legacyExport));
            const auto resultKind = legacyAbiKind(e.returnType);
            out << "static std::int32_t _legacy" << i
                << "(const WioValue* args, std::uint32_t count, WioValue* result) noexcept {\n"
                   " if (count != "
                << e.parameterTypes.size() << " || (count && !args)" << (resultKind == "VOID" ? "" : " || !result")
                << ") return WIO_INVOKE_BAD_ARGUMENTS;\n try {\n";
            for (std::size_t p = 0; p < e.parameterTypes.size(); ++p)
                out << " if (args[" << p << "].type != WIO_ABI_" << legacyAbiKind(e.parameterTypes[p])
                    << ") return WIO_INVOKE_TYPE_MISMATCH;\n";
            if (resultKind != "VOID")
                out << " result->type = WIO_ABI_" << resultKind << "; ";
            if (resultKind != "VOID")
                out << "result->value." << legacyField(resultKind) << " = ";
            out << functionName(e.function) << '(';
            for (std::size_t p = 0; p < e.parameterTypes.size(); ++p)
            {
                if (p)
                    out << ',';
                out << "args[" << p << "].value." << legacyField(legacyAbiKind(e.parameterTypes[p]));
            }
            out << "); return WIO_INVOKE_OK; } catch (...) { return WIO_INVOKE_NOT_CALLABLE; } }\n";
            if (!e.parameterTypes.empty())
            {
                out << "static const WioAbiType _legacy_params" << i << "[] = {";
                for (auto p : e.parameterTypes)
                    out << "WIO_ABI_" << legacyAbiKind(p) << ',';
                out << "};\n";
            }
        }
        if (!exports.empty())
        {
            out << "static const WioSdkExportDescriptor _sdk_exports[] = {\n";
            for (const auto& e : exports)
                out << '{' << e.stableId << "ULL," << quoted(e.stableKey) << ',' << quoted(e.logicalName) << ','
                    << quoted(e.symbolName) << ',' << quoted(e.roleName) << ',' << e.callTableSlot
                    << ",static_cast<WioSdkExportKind>(" << static_cast<int>(e.kind)
                    << "),static_cast<WioSdkExportRole>(" << static_cast<int>(e.role)
                    << "),WIO_SDK_CALL_NATIVE_ABI_V2},\n";
            out << "};\nstatic const WioSdkCallEntry _sdk_calls[] = {\n";
            for (std::size_t i = 0; i < exports.size(); ++i)
                out << '{' << exports[i].stableId << "ULL,&_sdk_call" << i << ",nullptr},\n";
            out << "};\n";
        }
        if (!module.contract.reflection.empty())
        {
            out << "static const WioSdkReflectionDescriptor _sdk_reflection[] = {\n";
            for (const auto& r : module.contract.reflection)
                out << '{' << r.stableTypeId << "ULL," << quoted(r.logicalName) << ','
                    << static_cast<int>(r.nominalKind) << ',' << (r.isExported ? 1 : 0) << "},\n";
            out << "};\n";
        }

        std::unordered_map<std::uint64_t, const AttributeApplicationDescriptor*> attributesById;
        for (const auto& attribute : module.contract.attributes)
            attributesById.emplace(attribute.stableId, &attribute);

        std::size_t attributeTableIndex = 0;
        const auto emitAttributeTable = [&](const std::vector<std::uint64_t>& ids)
        {
            std::vector<const AttributeApplicationDescriptor*> retained;
            for (const auto id : ids)
            {
                const auto found = attributesById.find(id);
                if (found != attributesById.end() && found->second->runtimeRetained)
                    retained.push_back(found->second);
            }
            if (retained.empty())
                return std::pair<std::uint32_t, std::string>{0u, "nullptr"};

            const auto tableIndex = attributeTableIndex++;
            for (std::size_t attributeIndex = 0; attributeIndex < retained.size(); ++attributeIndex)
            {
                const auto& attribute = *retained[attributeIndex];
                if (attribute.processors.empty())
                    continue;
                out << "static const WioModuleAttributeProcessorDescriptor _legacy_attribute_processors_" << tableIndex
                    << '_' << attributeIndex << "[] = {\n";
                for (std::size_t processorIndex = 0; processorIndex < attribute.processors.size(); ++processorIndex)
                {
                    const auto& processor = attribute.processors[processorIndex];
                    out << '{' << quoted(processor.canonicalTypeName) << ',' << legacyProcessorPhase(processor.phase)
                        << ',' << quoted(processor.hookMode) << ',' << processorIndex << "u},\n";
                }
                out << "};\n";
            }

            out << "static const WioModuleAttributeDescriptor _legacy_attributes_" << tableIndex << "[] = {\n";
            for (std::size_t attributeIndex = 0; attributeIndex < retained.size(); ++attributeIndex)
            {
                const auto& attribute = *retained[attributeIndex];
                std::string argumentText;
                for (std::size_t argumentIndex = 0; argumentIndex < attribute.arguments.size(); ++argumentIndex)
                {
                    if (argumentIndex)
                        argumentText += ',';
                    argumentText += attribute.arguments[argumentIndex].name;
                    argumentText += '=';
                    auto value = attribute.arguments[argumentIndex].sourceText;
                    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                        value = value.substr(1, value.size() - 2);
                    argumentText += value;
                }
                out << '{' << WirCppText::quote(attribute.canonicalName) << ',' << WirCppText::quote(argumentText) << ','
                    << legacyAttributeOrigin(attribute.origin) << ',' << attribute.processors.size() << "u,"
                    << (attribute.processors.empty() ? "nullptr"
                                                     : "_legacy_attribute_processors_" + std::to_string(tableIndex) +
                                                           "_" + std::to_string(attributeIndex))
                    << "},\n";
            }
            out << "};\n";
            return std::pair<std::uint32_t, std::string>{static_cast<std::uint32_t>(retained.size()),
                                                         "_legacy_attributes_" + std::to_string(tableIndex)};
        };

        std::unordered_map<std::uint64_t, const ReflectionDescriptor*> reflectionByType;
        for (const auto& reflected : module.contract.reflection)
            reflectionByType.emplace(reflected.type.value(), &reflected);

        std::vector<LegacyTypeDescriptorInfo> typeDescriptors;
        typeDescriptors.reserve(module.types.size());
        std::unordered_map<std::uint64_t, std::size_t> typeDescriptorById;
        const auto shortTypeName = [](const std::string& name)
        {
            const auto separator = name.rfind("::");
            return separator == std::string::npos ? name : name.substr(separator + 2);
        };
        std::function<std::size_t(TypeId)> ensureTypeDescriptor = [&](TypeId id) -> std::size_t
        {
            if (const auto found = typeDescriptorById.find(id.value()); found != typeDescriptorById.end())
                return found->second;

            const auto descriptorIndex = typeDescriptors.size();
            typeDescriptorById.emplace(id.value(), descriptorIndex);
            typeDescriptors.emplace_back();
            auto& descriptor = typeDescriptors[descriptorIndex];
            const auto& valueType = module.types.get(id);
            descriptor.displayName =
                valueType.name.empty() ? std::string(typeKindName(valueType.kind)) : valueType.name;

            const auto addGenericArguments = [&]
            {
                for (const auto argument : valueType.arguments)
                    descriptor.genericArguments.push_back(ensureTypeDescriptor(argument));
                if (!valueType.arguments.empty())
                {
                    descriptor.displayName = valueType.name + '<';
                    for (std::size_t index = 0; index < descriptor.genericArguments.size(); ++index)
                    {
                        if (index)
                            descriptor.displayName += ", ";
                        descriptor.displayName += typeDescriptors[descriptor.genericArguments[index]].displayName;
                    }
                    descriptor.displayName += '>';
                }
            };

            switch (valueType.kind)
            {
            case TypeKind::String:
                descriptor.displayName = "string";
                descriptor.kind = "WIO_MODULE_TYPE_DESC_STRING";
                break;
            case TypeKind::Text:
                descriptor.displayName = "text";
                descriptor.kind = "WIO_MODULE_TYPE_DESC_TEXT";
                break;
            case TypeKind::Any:
                descriptor.displayName = "any";
                descriptor.kind = "WIO_MODULE_TYPE_DESC_ANY";
                break;
            case TypeKind::Opaque:
                descriptor.displayName = "opaque";
                descriptor.kind = "WIO_MODULE_TYPE_DESC_OPAQUE";
                break;
            case TypeKind::Nullable:
                descriptor.kind = "WIO_MODULE_TYPE_DESC_NULLABLE";
                if (!valueType.arguments.empty())
                {
                    descriptor.element = ensureTypeDescriptor(valueType.arguments.front());
                    descriptor.displayName = typeDescriptors[*descriptor.element].displayName + '?';
                    descriptor.abi = "WIO_ABI_" + legacyAbiKind(valueType.arguments.front());
                }
                break;
            case TypeKind::Array:
                descriptor.kind =
                    valueType.staticExtent ? "WIO_MODULE_TYPE_DESC_STATIC_ARRAY" : "WIO_MODULE_TYPE_DESC_DYNAMIC_ARRAY";
                descriptor.staticExtent = valueType.staticExtent.value_or(0);
                if (!valueType.arguments.empty())
                {
                    descriptor.element = ensureTypeDescriptor(valueType.arguments.front());
                    const auto& elementName = typeDescriptors[*descriptor.element].displayName;
                    descriptor.displayName = valueType.staticExtent ? '[' + elementName + "; " +
                                                                          std::to_string(*valueType.staticExtent) + ']'
                                                                    : elementName + "[]";
                }
                break;
            case TypeKind::Dictionary:
                descriptor.kind =
                    valueType.name == "ordered" ? "WIO_MODULE_TYPE_DESC_TREE" : "WIO_MODULE_TYPE_DESC_DICT";
                if (valueType.arguments.size() >= 2)
                {
                    descriptor.key = ensureTypeDescriptor(valueType.arguments[0]);
                    descriptor.value = ensureTypeDescriptor(valueType.arguments[1]);
                    descriptor.displayName = std::string(valueType.name == "ordered" ? "Tree<" : "Dict<") +
                                             typeDescriptors[*descriptor.key].displayName + ", " +
                                             typeDescriptors[*descriptor.value].displayName + '>';
                }
                break;
            case TypeKind::Function:
                descriptor.kind = "WIO_MODULE_TYPE_DESC_FUNCTION";
                if (!valueType.arguments.empty())
                {
                    for (std::size_t index = 0; index + 1 < valueType.arguments.size(); ++index)
                        descriptor.parameters.push_back(ensureTypeDescriptor(valueType.arguments[index]));
                    descriptor.result = ensureTypeDescriptor(valueType.arguments.back());
                    descriptor.displayName = "fn(";
                    for (std::size_t index = 0; index < descriptor.parameters.size(); ++index)
                    {
                        if (index)
                            descriptor.displayName += ", ";
                        descriptor.displayName += typeDescriptors[descriptor.parameters[index]].displayName;
                    }
                    descriptor.displayName += ") -> " + typeDescriptors[*descriptor.result].displayName;
                }
                break;
            case TypeKind::AsyncTask:
                descriptor.kind = "WIO_MODULE_TYPE_DESC_ASYNC_TASK";
                if (!valueType.arguments.empty())
                {
                    descriptor.element = ensureTypeDescriptor(valueType.arguments.front());
                    descriptor.displayName = "coroutine<" + typeDescriptors[*descriptor.element].displayName + '>';
                }
                break;
            case TypeKind::ConstValue:
                descriptor.kind = "WIO_MODULE_TYPE_DESC_CONST_VALUE";
                descriptor.constValue = valueType.name;
                if (!valueType.arguments.empty())
                {
                    descriptor.constValueType = ensureTypeDescriptor(valueType.arguments.front());
                    descriptor.displayName =
                        "const " + typeDescriptors[*descriptor.constValueType].displayName + " = " + valueType.name;
                }
                break;
            case TypeKind::Named:
            {
                descriptor.logicalTypeName = valueType.name;
                addGenericArguments();
                const auto shortName = shortTypeName(valueType.name);
                if (valueType.nominalKind == NominalKind::Enum || valueType.nominalKind == NominalKind::Flagset)
                {
                    descriptor.kind = valueType.nominalKind == NominalKind::Enum ? "WIO_MODULE_TYPE_DESC_ENUM"
                                                                                 : "WIO_MODULE_TYPE_DESC_FLAGSET";
                    descriptor.abi = "WIO_ABI_" + legacyAbiKind(id);
                    for (const auto& member : valueType.enumCases)
                    {
                        descriptor.enumMembers.push_back({member.name, "WioMakeAbiIntegerValue(" + descriptor.abi +
                                                                           ", " + std::to_string(member.rawValue) +
                                                                           "ULL)"});
                    }
                }
                else if (valueType.nominalKind == NominalKind::Interface)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_INTERFACE";
                else if (valueType.nominalValueModel == NominalValueModel::Option)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_OPTION";
                else if (valueType.nominalValueModel == NominalValueModel::Result)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_RESULT";
                else if (valueType.nominalValueModel == NominalValueModel::Tuple)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_TUPLE";
                else if (valueType.nominalValueModel == NominalValueModel::Span)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_SPAN";
                else if (shortName == "ResultUnit")
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_UNIT";
                else if (shortName == "Queue")
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_QUEUE";
                else if (shortName == "UnorderedSet")
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_UNORDERED_SET";
                else if (shortName == "OrderedSet")
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_ORDERED_SET";
                else if (shortName == "ByteBuffer")
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_BYTE_BUFFER";
                else if (shortName == "box")
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_BOX";
                else if (valueType.nominalKind == NominalKind::Object)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_OBJECT";
                else if (valueType.nominalKind == NominalKind::Component)
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_COMPONENT";
                else if (!valueType.arguments.empty())
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_GENERIC_INSTANCE";
                else
                    descriptor.kind = "WIO_MODULE_TYPE_DESC_OPAQUE";
                break;
            }
            case TypeKind::Void:
            case TypeKind::Bool:
            case TypeKind::I8:
            case TypeKind::I16:
            case TypeKind::I32:
            case TypeKind::I64:
            case TypeKind::ISize:
            case TypeKind::U8:
            case TypeKind::U16:
            case TypeKind::U32:
            case TypeKind::U64:
            case TypeKind::USize:
            case TypeKind::F32:
            case TypeKind::F64:
            case TypeKind::Byte:
            case TypeKind::Char:
                descriptor.kind = "WIO_MODULE_TYPE_DESC_PRIMITIVE";
                descriptor.abi = "WIO_ABI_" + legacyKind(valueType);
                break;
            default:
                break;
            }
            return descriptorIndex;
        };

        for (const auto& reflected : module.contract.reflection)
        {
            if (!reflected.isExported)
                continue;
            for (const auto& field : reflected.fields)
                if (field.visibility == FieldVisibility::Public)
                    ensureTypeDescriptor(field.type);
        }

        if (!typeDescriptors.empty())
        {
            for (std::size_t index = 0; index < typeDescriptors.size(); ++index)
                out << "extern const WioModuleTypeDescriptor _legacy_type_descriptor_" << index << ";\n";
            for (std::size_t index = 0; index < typeDescriptors.size(); ++index)
            {
                const auto& descriptor = typeDescriptors[index];
                if (!descriptor.parameters.empty())
                {
                    out << "static const WioModuleTypeDescriptor* _legacy_type_descriptor_params_" << index << "[]={";
                    for (const auto parameter : descriptor.parameters)
                        out << "&_legacy_type_descriptor_" << parameter << ',';
                    out << "};\n";
                }
                if (!descriptor.genericArguments.empty())
                {
                    out << "static const WioModuleTypeDescriptor* _legacy_type_descriptor_args_" << index << "[]={";
                    for (const auto argument : descriptor.genericArguments)
                        out << "&_legacy_type_descriptor_" << argument << ',';
                    out << "};\n";
                }
                if (!descriptor.enumMembers.empty())
                {
                    out << "static const WioModuleEnumMemberDescriptor _legacy_type_descriptor_members_" << index
                        << "[]={";
                    for (const auto& member : descriptor.enumMembers)
                        out << '{' << quoted(member.name) << ',' << member.value << "},";
                    out << "};\n";
                }
            }
            for (std::size_t index = 0; index < typeDescriptors.size(); ++index)
            {
                const auto& descriptor = typeDescriptors[index];
                const auto pointer = [](const char* prefix, const std::optional<std::size_t>& value)
                { return value ? std::string(prefix) + std::to_string(*value) : std::string("nullptr"); };
                out << "const WioModuleTypeDescriptor _legacy_type_descriptor_" << index << '{'
                    << quoted(descriptor.displayName) << ','
                    << (descriptor.logicalTypeName.empty() ? "nullptr" : quoted(descriptor.logicalTypeName)) << ','
                    << descriptor.kind << ',' << descriptor.abi << ',' << descriptor.staticExtent << "ULL,"
                    << pointer("&_legacy_type_descriptor_", descriptor.element) << ','
                    << pointer("&_legacy_type_descriptor_", descriptor.key) << ','
                    << pointer("&_legacy_type_descriptor_", descriptor.value) << ','
                    << pointer("&_legacy_type_descriptor_", descriptor.result) << ',' << descriptor.parameters.size()
                    << "u,"
                    << (descriptor.parameters.empty() ? "nullptr"
                                                      : "_legacy_type_descriptor_params_" + std::to_string(index))
                    << ',' << descriptor.enumMembers.size() << "u,"
                    << (descriptor.enumMembers.empty() ? "nullptr"
                                                       : "_legacy_type_descriptor_members_" + std::to_string(index))
                    << ",WioStableTypeId(" << quoted(descriptor.displayName) << "),"
                    << descriptor.genericArguments.size() << "u,"
                    << (descriptor.genericArguments.empty() ? "nullptr"
                                                            : "_legacy_type_descriptor_args_" + std::to_string(index))
                    << ',' << pointer("&_legacy_type_descriptor_", descriptor.constValueType) << ','
                    << (descriptor.constValue ? quoted(*descriptor.constValue) : "nullptr") << "};\n";
            }
        }

        const auto argumentExpression = [&](TypeId parameterType, std::size_t argumentIndex)
        {
            const auto kind = legacyAbiKind(parameterType);
            const auto payload = "args[" + std::to_string(argumentIndex) + "].value." + legacyField(kind);
            const auto& parameter = module.types.get(parameterType);
            if (parameter.kind == TypeKind::Named &&
                (parameter.nominalKind == NominalKind::Enum || parameter.nominalKind == NominalKind::Flagset))
                return "static_cast<" + type(parameterType) + ">(" + payload + ')';
            return payload;
        };

        const auto assignResult = [&](TypeId resultType, const std::string& expression)
        {
            const auto kind = legacyAbiKind(resultType);
            const auto& result = module.types.get(resultType);
            if (result.kind == TypeKind::Named &&
                (result.nominalKind == NominalKind::Enum || result.nominalKind == NominalKind::Flagset))
                return "result->value." + legacyField(kind) + "=static_cast<decltype(result->value." +
                       legacyField(kind) + ")>(" + expression + ");";
            return "result->value." + legacyField(kind) + '=' + expression + ';';
        };

        std::vector<LegacyTypeInfo> legacyTypes;
        std::function<bool(TypeId)> hasDirectDynamicBridge = [&](TypeId id)
        {
            const auto& valueType = module.types.get(id);
            switch (valueType.kind)
            {
            case TypeKind::String:
            case TypeKind::Bool:
            case TypeKind::I8:
            case TypeKind::I16:
            case TypeKind::I32:
            case TypeKind::U16:
            case TypeKind::U32:
            case TypeKind::F32:
            case TypeKind::F64:
            case TypeKind::Char:
                return true;
            case TypeKind::Array:
                return !valueType.arguments.empty() && hasDirectDynamicBridge(valueType.arguments.front());
            case TypeKind::Dictionary:
                return valueType.arguments.size() >= 2 && hasDirectDynamicBridge(valueType.arguments[0]) &&
                       hasDirectDynamicBridge(valueType.arguments[1]);
            case TypeKind::Function:
                return std::ranges::all_of(valueType.arguments, hasDirectDynamicBridge);
            default:
                return false;
            }
        };
        std::function<std::optional<std::string>(TypeId)> dynamicBridgeType;
        dynamicBridgeType = [&](TypeId id) -> std::optional<std::string>
        {
            const auto& valueType = module.types.get(id);
            switch (valueType.kind)
            {
            case TypeKind::Text:
                return "std::string";
            case TypeKind::Byte:
                return "wio::sdk::WioByte";
            case TypeKind::U8:
                return "wio::sdk::WioU8";
            case TypeKind::I64:
                return "wio::sdk::WioI64";
            case TypeKind::U64:
                return "wio::sdk::WioU64";
            case TypeKind::ISize:
                return "wio::sdk::WioISize";
            case TypeKind::USize:
                return "wio::sdk::WioUSize";
            case TypeKind::Array:
            {
                if (valueType.arguments.empty())
                    return std::nullopt;
                const auto element = dynamicBridgeType(valueType.arguments.front());
                if (!element)
                    return std::nullopt;
                return valueType.staticExtent
                           ? std::optional<std::string>("std::array<" + *element + ", " +
                                                        std::to_string(*valueType.staticExtent) + ">")
                           : std::optional<std::string>("std::vector<" + *element + ">");
            }
            case TypeKind::Dictionary:
            {
                if (valueType.arguments.size() < 2)
                    return std::nullopt;
                const auto key = dynamicBridgeType(valueType.arguments[0]);
                const auto value = dynamicBridgeType(valueType.arguments[1]);
                if (!key || !value)
                    return std::nullopt;
                return std::string(valueType.name == "ordered" ? "std::map<" : "std::unordered_map<") + *key + ", " +
                       *value + ">";
            }
            case TypeKind::Function:
                return hasDirectDynamicBridge(id) ? std::optional<std::string>(type(id)) : std::nullopt;
            case TypeKind::Named:
            {
                const auto& descriptor = typeDescriptors[ensureTypeDescriptor(id)];
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_UNIT")
                    return "wio::sdk::WioUnit";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_SPAN")
                    return "wio::sdk::WioSpanRange";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_BYTE_BUFFER")
                    return "wio::sdk::WioByteBuffer";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_OPTION" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_RESULT" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_QUEUE" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_UNORDERED_SET" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_ORDERED_SET")
                {
                    if (valueType.arguments.size() != 1)
                        return std::nullopt;
                    const auto value = dynamicBridgeType(valueType.arguments.front());
                    if (!value)
                        return std::nullopt;
                    const std::string wrapper = descriptor.kind == "WIO_MODULE_TYPE_DESC_OPTION"   ? "WioOption"
                                                : descriptor.kind == "WIO_MODULE_TYPE_DESC_RESULT" ? "WioResult"
                                                : descriptor.kind == "WIO_MODULE_TYPE_DESC_QUEUE"  ? "WioQueue"
                                                : descriptor.kind == "WIO_MODULE_TYPE_DESC_UNORDERED_SET"
                                                    ? "WioUnorderedSet"
                                                    : "WioOrderedSet";
                    return "wio::sdk::" + wrapper + '<' + *value + '>';
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_TUPLE")
                {
                    std::string result = "wio::sdk::WioTuple<";
                    for (std::size_t index = 0; index < valueType.arguments.size(); ++index)
                    {
                        const auto argument = dynamicBridgeType(valueType.arguments[index]);
                        if (!argument)
                            return std::nullopt;
                        if (index)
                            result += ", ";
                        result += *argument;
                    }
                    return result + '>';
                }
                return std::nullopt;
            }
            default:
                return hasDirectDynamicBridge(id) ? std::optional<std::string>(type(id)) : std::nullopt;
            }
        };

        std::function<std::optional<std::string>(const std::string&, TypeId, std::size_t)> dynamicToHost;
        std::function<std::optional<std::string>(const std::string&, TypeId, std::size_t)> dynamicFromHost;
        dynamicToHost = [&](const std::string& expression, TypeId id,
                            const std::size_t depth) -> std::optional<std::string>
        {
            const auto& valueType = module.types.get(id);
            const auto hostType = dynamicBridgeType(id);
            if (!hostType)
                return std::nullopt;
            switch (valueType.kind)
            {
            case TypeKind::Text:
                return '(' + expression + ").Utf8()";
            case TypeKind::Byte:
            case TypeKind::U8:
            case TypeKind::I64:
            case TypeKind::U64:
            case TypeKind::ISize:
            case TypeKind::USize:
                return *hostType + '(' + expression + ')';
            case TypeKind::Array:
            {
                if (hasDirectDynamicBridge(id))
                    return expression;
                const auto item =
                    dynamicToHost("_item" + std::to_string(depth), valueType.arguments.front(), depth + 1);
                if (!item)
                    return std::nullopt;
                if (valueType.staticExtent)
                {
                    std::string values;
                    for (std::size_t index = 0; index < *valueType.staticExtent; ++index)
                    {
                        const auto converted =
                            dynamicToHost("_source" + std::to_string(depth) + '[' + std::to_string(index) + ']',
                                          valueType.arguments.front(), depth + 1);
                        if (!converted)
                            return std::nullopt;
                        if (index)
                            values += ',';
                        values += *converted;
                    }
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ";return " +
                           *hostType + '{' + values + "};}())";
                }
                return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ';' + *hostType +
                       " _output;_output.reserve(_source" + std::to_string(depth) + ".size());for(const auto& _item" +
                       std::to_string(depth) + ":_source" + std::to_string(depth) + "){_output.push_back(" + *item +
                       ");}return _output;}())";
            }
            case TypeKind::Dictionary:
            {
                if (hasDirectDynamicBridge(id))
                    return expression;
                const auto key = dynamicToHost("_key" + std::to_string(depth), valueType.arguments[0], depth + 1);
                const auto value = dynamicToHost("_value" + std::to_string(depth), valueType.arguments[1], depth + 1);
                if (!key || !value)
                    return std::nullopt;
                return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ';' + *hostType +
                       " _output;for(const auto& [_key" + std::to_string(depth) + ",_value" + std::to_string(depth) +
                       "]:_source" + std::to_string(depth) + "){_output.emplace(" + *key + ',' + *value +
                       ");}return _output;}())";
            }
            case TypeKind::Named:
            {
                const auto& descriptor = typeDescriptors[ensureTypeDescriptor(id)];
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_UNIT")
                    return "wio::sdk::WioUnit{}";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_SPAN")
                    return "wio::sdk::WioSpanRange{static_cast<std::size_t>((" + expression +
                           ")._f0),"
                           "static_cast<std::size_t>((" +
                           expression + ")._f1)}";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_BYTE_BUFFER")
                {
                    return "([&](){auto _source" + std::to_string(depth) + '=' + expression +
                           ";wio::sdk::WioByteBuffer _output;if(!_source" + std::to_string(depth) +
                           ")return _output;_output.reserve(_source" + std::to_string(depth) +
                           "->_f0.capacity());for(auto _byte:_source" + std::to_string(depth) +
                           "->_f0)_output.write(static_cast<std::byte>(_byte));(void)_output.seek(_source" +
                           std::to_string(depth) + "->_f1);return _output;}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_OPTION")
                {
                    const auto converted = dynamicToHost("_source" + std::to_string(depth) + "->_f1",
                                                         valueType.arguments.front(), depth + 1);
                    if (!converted)
                        return std::nullopt;
                    return "([&](){auto _source" + std::to_string(depth) + '=' + expression + ";if(!_source" +
                           std::to_string(depth) + "||!_source" + std::to_string(depth) + "->_f0)return " + *hostType +
                           "::none();return " + *hostType + "::some(" + *converted + ");}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_RESULT")
                {
                    const auto converted = dynamicToHost("_source" + std::to_string(depth) + "->_f1",
                                                         valueType.arguments.front(), depth + 1);
                    if (!converted || valueType.fields.size() < 3)
                        return std::nullopt;
                    return "([&](){auto _source" + std::to_string(depth) + '=' + expression + ";if(_source" +
                           std::to_string(depth) + "&&_source" + std::to_string(depth) + "->_f0)return " + *hostType +
                           "::ok(" + *converted + ");const auto& _error=_source" + std::to_string(depth) +
                           "->_f2;return " + *hostType +
                           "::error(wio::sdk::WioResultError{static_cast<wio::sdk::WioResultDomain>(static_cast<"
                           "std::int32_t>(_error._f0)),_error._f1,_error._f2,_error._f3});}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_TUPLE")
                {
                    std::string values;
                    for (std::size_t index = 0; index < valueType.arguments.size(); ++index)
                    {
                        const auto converted = dynamicToHost("std::get<" + std::to_string(index) + ">(_source" +
                                                                 std::to_string(depth) + "->_f0)",
                                                             valueType.arguments[index], depth + 1);
                        if (!converted)
                            return std::nullopt;
                        if (index)
                            values += ',';
                        values += *converted;
                    }
                    return "([&](){auto _source" + std::to_string(depth) + '=' + expression + ";if(!_source" +
                           std::to_string(depth) + ")throw std::runtime_error(\"Wio tuple is null\");return " +
                           *hostType + '{' + values + "};}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_QUEUE" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_UNORDERED_SET" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_ORDERED_SET")
                {
                    const auto item =
                        dynamicToHost("_item" + std::to_string(depth), valueType.arguments.front(), depth + 1);
                    if (!item)
                        return std::nullopt;
                    const bool queue = descriptor.kind == "WIO_MODULE_TYPE_DESC_QUEUE";
                    const std::string append =
                        queue ? "_output.push(" + *item + ");" : "(void)_output.add(" + *item + ");";
                    const std::string collection = queue ? "_source" + std::to_string(depth) + "->_f0"
                                                         : "_source" + std::to_string(depth) + "->_f0";
                    const std::string itemExpression =
                        queue ? "_item" + std::to_string(depth) : "_entry" + std::to_string(depth) + ".first";
                    const auto convertedItem = dynamicToHost(itemExpression, valueType.arguments.front(), depth + 1);
                    if (!convertedItem)
                        return std::nullopt;
                    const std::string appendConverted =
                        queue ? "_output.push(" + *convertedItem + ");" : "(void)_output.add(" + *convertedItem + ");";
                    return "([&](){auto _source" + std::to_string(depth) + '=' + expression + ';' + *hostType +
                           " _output;if(!_source" + std::to_string(depth) + ")return _output;" +
                           (queue ? "for(std::size_t _index=_source" + std::to_string(depth) + "->_f1;_index<_source" +
                                        std::to_string(depth) + "->_f0.size();++_index){const auto& _item" +
                                        std::to_string(depth) + "=_source" + std::to_string(depth) + "->_f0[_index];" +
                                        appendConverted + '}'
                                  : "for(const auto& _entry" + std::to_string(depth) + ':' + collection + "){" +
                                        appendConverted + '}') +
                           "return _output;}())";
                }
                break;
            }
            default:
                if (hasDirectDynamicBridge(id))
                    return expression;
                break;
            }
            return std::nullopt;
        };

        dynamicFromHost = [&](const std::string& expression, TypeId id,
                              const std::size_t depth) -> std::optional<std::string>
        {
            const auto& valueType = module.types.get(id);
            switch (valueType.kind)
            {
            case TypeKind::Text:
                return "wio::runtime::Text::FromUtf8(" + expression + ')';
            case TypeKind::Byte:
            case TypeKind::U8:
            case TypeKind::I64:
            case TypeKind::U64:
            case TypeKind::ISize:
            case TypeKind::USize:
                return "static_cast<" + type(id) + ">((" + expression + ").value())";
            case TypeKind::Array:
            {
                if (hasDirectDynamicBridge(id))
                    return expression;
                const auto item =
                    dynamicFromHost("_item" + std::to_string(depth), valueType.arguments.front(), depth + 1);
                if (!item)
                    return std::nullopt;
                if (valueType.staticExtent)
                {
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ';' + type(id) +
                           " _output{};for(std::size_t _index=0;_index<" + std::to_string(*valueType.staticExtent) +
                           ";++_index){const auto& _item" + std::to_string(depth) + "=_source" + std::to_string(depth) +
                           "[_index];_output[_index]=" + *item + ";}return _output;}())";
                }
                return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ';' + type(id) +
                       " _output;_output.reserve(_source" + std::to_string(depth) + ".size());for(const auto& _item" +
                       std::to_string(depth) + ":_source" + std::to_string(depth) + "){_output.push_back(" + *item +
                       ");}return _output;}())";
            }
            case TypeKind::Dictionary:
            {
                if (hasDirectDynamicBridge(id))
                    return expression;
                const auto key = dynamicFromHost("_key" + std::to_string(depth), valueType.arguments[0], depth + 1);
                const auto value = dynamicFromHost("_value" + std::to_string(depth), valueType.arguments[1], depth + 1);
                if (!key || !value)
                    return std::nullopt;
                return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ';' + type(id) +
                       " _output;for(const auto& [_key" + std::to_string(depth) + ",_value" + std::to_string(depth) +
                       "]:_source" + std::to_string(depth) + "){_output.emplace(" + *key + ',' + *value +
                       ");}return _output;}())";
            }
            case TypeKind::Named:
            {
                const auto& descriptor = typeDescriptors[ensureTypeDescriptor(id)];
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_UNIT")
                    return type(id) + "{}";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_SPAN")
                    return type(id) + "{static_cast<std::size_t>((" + expression +
                           ").start()),"
                           "static_cast<std::size_t>((" +
                           expression + ").count())}";
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_BYTE_BUFFER")
                {
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression +
                           ";auto _output=" + type(id) + "::Create();_output->_f0.reserve(_source" +
                           std::to_string(depth) + ".capacity());for(auto _byte:_source" + std::to_string(depth) +
                           ".data())_output->_f0.push_back(static_cast<std::uint8_t>(_byte));_output->_f1=_source" +
                           std::to_string(depth) + ".position();return _output;}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_OPTION")
                {
                    const auto converted = dynamicFromHost("_source" + std::to_string(depth) + ".value()",
                                                           valueType.arguments.front(), depth + 1);
                    if (!converted)
                        return std::nullopt;
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ";if(_source" +
                           std::to_string(depth) + ".is_none())return " + type(id) + "::Create();return " + type(id) +
                           "::Create(" + *converted + ");}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_RESULT")
                {
                    const auto converted = dynamicFromHost("_source" + std::to_string(depth) + ".value()",
                                                           valueType.arguments.front(), depth + 1);
                    if (!converted || valueType.fields.size() < 3)
                        return std::nullopt;
                    const auto errorType = valueType.fields[2].type;
                    const auto& error = module.types.get(errorType);
                    if (error.fields.size() < 4)
                        return std::nullopt;
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ";if(_source" +
                           std::to_string(depth) + ".is_ok())return " + type(id) + "::Create(" + *converted + ");" +
                           type(errorType) + " _error{};_error._f0=static_cast<" + type(error.fields[0].type) +
                           ">(static_cast<std::int32_t>(_source" + std::to_string(depth) +
                           ".error_value().domain));_error._f1=_source" + std::to_string(depth) +
                           ".error_value().code;_error._f2=_source" + std::to_string(depth) +
                           ".error_value().native_code;_error._f3=_source" + std::to_string(depth) +
                           ".error_value().message;return " + type(id) + "::Create(std::move(_error));}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_TUPLE")
                {
                    std::string values;
                    for (std::size_t index = 0; index < valueType.arguments.size(); ++index)
                    {
                        const auto converted = dynamicFromHost("std::get<" + std::to_string(index) + ">(_source" +
                                                                   std::to_string(depth) + ')',
                                                               valueType.arguments[index], depth + 1);
                        if (!converted)
                            return std::nullopt;
                        if (index)
                            values += ',';
                        values += *converted;
                    }
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression + ";return " +
                           type(id) + "::Create(std::make_tuple(" + values + "));}())";
                }
                if (descriptor.kind == "WIO_MODULE_TYPE_DESC_QUEUE" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_UNORDERED_SET" ||
                    descriptor.kind == "WIO_MODULE_TYPE_DESC_ORDERED_SET")
                {
                    const bool queue = descriptor.kind == "WIO_MODULE_TYPE_DESC_QUEUE";
                    const std::string sourceValues = queue ? ".to_array()" : ".values()";
                    const auto converted =
                        dynamicFromHost("_item" + std::to_string(depth), valueType.arguments.front(), depth + 1);
                    if (!converted)
                        return std::nullopt;
                    return "([&](){const auto& _source" + std::to_string(depth) + '=' + expression +
                           ";auto _output=" + type(id) + "::Create();for(const auto& _item" + std::to_string(depth) +
                           ":_source" + std::to_string(depth) + sourceValues +
                           "){"
                           "if constexpr(true){" +
                           (queue ? "_output->_f0.push_back(" + *converted + ");"
                                  : "_output->_f0.emplace(" + *converted + ",true);") +
                           "}}return _output;}())";
                }
                break;
            }
            default:
                if (hasDirectDynamicBridge(id))
                    return expression;
                break;
            }
            return std::nullopt;
        };
        for (const auto& reflected : module.contract.reflection)
        {
            if (!reflected.isExported ||
                (reflected.nominalKind != NominalKind::Object && reflected.nominalKind != NominalKind::Component))
                continue;

            LegacyTypeInfo legacyType;
            legacyType.descriptor = &reflected;
            const auto& reflectedType = module.types.get(reflected.type);
            const bool isObject = reflected.nominalKind == NominalKind::Object;
            const auto cppType = storageCppType(reflected.type);

            const auto appendExport = [&](LegacyExportInfo exportInfo)
            {
                const auto index = legacy.size();
                legacy.push_back(std::move(exportInfo));
                return index;
            };

            const auto destroyInvoke = "_legacy_type_call_" + std::to_string(legacy.size());
            out << "static std::int32_t " << destroyInvoke
                << "(const WioValue* args,std::uint32_t count,WioValue*) noexcept {"
                   "if(count!=1u||!args)return WIO_INVOKE_BAD_ARGUMENTS;"
                   "if(args[0].type!=WIO_ABI_USIZE)return WIO_INVOKE_TYPE_MISMATCH;"
                   "auto* instance=reinterpret_cast<"
                << cppType << "*>(args[0].value.v_usize);if(!instance)return WIO_INVOKE_BAD_ARGUMENTS;";
            if (isObject)
                out << "wio::runtime::RefDeleter<" << cppType << ">::Execute(instance);";
            else
                out << "delete instance;";
            out << "return WIO_INVOKE_OK;}\n";
            legacyType.destroyExport = appendExport({reflected.logicalName + ".destroy",
                                                     reflected.logicalName + ".destroy",
                                                     "VOID",
                                                     {"USIZE"},
                                                     destroyInvoke});

            for (const auto& method : reflected.methods)
            {
                if (method.name != "OnConstruct")
                    continue;
                bool compatible = true;
                for (const auto parameter : method.parameterTypes)
                    compatible &= legacyAbiKind(parameter) != "UNKNOWN";
                if (!compatible)
                    continue;

                const auto invokeName = "_legacy_type_call_" + std::to_string(legacy.size());
                out << "static std::int32_t " << invokeName
                    << "(const WioValue* args,std::uint32_t count,WioValue* result) noexcept {"
                       "if(count!="
                    << method.parameterTypes.size() << "u||(count&&!args)||!result)return WIO_INVOKE_BAD_ARGUMENTS;";
                for (std::size_t parameterIndex = 0; parameterIndex < method.parameterTypes.size(); ++parameterIndex)
                    out << "if(args[" << parameterIndex << "].type!=WIO_ABI_"
                        << legacyAbiKind(method.parameterTypes[parameterIndex]) << ")return WIO_INVOKE_TYPE_MISMATCH;";
                out << "try{";
                if (isObject)
                {
                    out << "auto value=wio::runtime::Ref<" << cppType
                        << ">::Create(wio::wir_backend::SkipConstructor{});";
                    out << functionName(method.function) << "(wio::wir_backend::Place<wio::runtime::Ref<" << cppType
                        << ">>::borrow(value)";
                }
                else
                {
                    out << "auto* instance=new " << cppType << "{};" << functionName(method.function)
                        << "(wio::wir_backend::Place<" << cppType << ">::borrow(*instance)";
                }
                for (std::size_t parameterIndex = 0; parameterIndex < method.parameterTypes.size(); ++parameterIndex)
                {
                    out << ',' << argumentExpression(method.parameterTypes[parameterIndex], parameterIndex);
                }
                out << ");";
                if (isObject)
                    out << "auto* instance=value.Detach();";
                out << "result->type=WIO_ABI_USIZE;result->value.v_usize=reinterpret_cast<std::uintptr_t>(instance);"
                       "return WIO_INVOKE_OK;}catch(...){return WIO_INVOKE_NOT_CALLABLE;}}\n";

                LegacyExportInfo constructorExport;
                constructorExport.logicalName = reflected.logicalName + ".OnConstruct";
                constructorExport.symbolName = constructorExport.logicalName;
                constructorExport.returnKind = "USIZE";
                constructorExport.invokeName = invokeName;
                for (const auto parameter : method.parameterTypes)
                    constructorExport.parameterKinds.push_back(legacyAbiKind(parameter));
                const auto constructorIndex = appendExport(std::move(constructorExport));
                legacyType.constructors.push_back(constructorIndex);
                if (method.parameterTypes.empty())
                    legacyType.createExport = constructorIndex;
            }

            if (!legacyType.createExport && !reflectedType.hasConstructor)
            {
                const auto invokeName = "_legacy_type_call_" + std::to_string(legacy.size());
                out << "static std::int32_t " << invokeName
                    << "(const WioValue*,std::uint32_t count,WioValue* result) noexcept {"
                       "if(count!=0u||!result)return WIO_INVOKE_BAD_ARGUMENTS;try{";
                if (isObject)
                    out << "auto* instance=wio::runtime::Ref<" << cppType << ">::Create().Detach();";
                else
                    out << "auto* instance=new " << cppType << "();";
                out << "result->type=WIO_ABI_USIZE;result->value.v_usize=reinterpret_cast<std::uintptr_t>(instance);"
                       "return WIO_INVOKE_OK;}catch(...){return WIO_INVOKE_NOT_CALLABLE;}}\n";
                legacyType.createExport = appendExport(
                    {reflected.logicalName + ".create", reflected.logicalName + ".create", "USIZE", {}, invokeName});
            }

            std::function<void(TypeId)> collectFields = [&](TypeId ownerType)
            {
                const auto foundReflection = reflectionByType.find(ownerType.value());
                if (foundReflection == reflectionByType.end())
                    return;
                const auto& owner = module.types.get(ownerType);
                const auto& ownerReflection = *foundReflection->second;
                for (std::size_t fieldIndex = 0; fieldIndex < ownerReflection.fields.size(); ++fieldIndex)
                {
                    const auto& field = ownerReflection.fields[fieldIndex];
                    if (field.visibility != FieldVisibility::Public)
                        continue;
                    LegacyFieldInfo fieldInfo;
                    fieldInfo.descriptor = &field;
                    fieldInfo.ownerType = ownerType;
                    fieldInfo.ownerFieldIndex = fieldIndex;
                    fieldInfo.typeDescriptor = ensureTypeDescriptor(field.type);
                    legacyType.fields.push_back(std::move(fieldInfo));
                }
                for (const auto baseType : owner.baseTypes)
                    collectFields(baseType);
            };
            collectFields(reflected.type);

            for (auto& fieldInfo : legacyType.fields)
            {
                const auto& field = *fieldInfo.descriptor;
                const auto fieldKind = legacyAbiKind(field.type);
                const auto ownerCppType = storageCppType(fieldInfo.ownerType);
                const auto member =
                    "static_cast<" + ownerCppType + "&>(*instance)._f" + std::to_string(fieldInfo.ownerFieldIndex);

                const bool objectHandle =
                    typeDescriptors[fieldInfo.typeDescriptor].kind == "WIO_MODULE_TYPE_DESC_OBJECT";
                const bool componentHandle =
                    typeDescriptors[fieldInfo.typeDescriptor].kind == "WIO_MODULE_TYPE_DESC_COMPONENT";
                if (objectHandle || componentHandle)
                {
                    const auto getterInvoke = "_legacy_type_call_" + std::to_string(legacy.size());
                    out << "static std::int32_t " << getterInvoke
                        << "(const WioValue* args,std::uint32_t count,WioValue* result) noexcept {"
                           "if(count!=1u||!args||!result)return WIO_INVOKE_BAD_ARGUMENTS;"
                           "if(args[0].type!=WIO_ABI_USIZE)return WIO_INVOKE_TYPE_MISMATCH;auto* instance="
                           "reinterpret_cast<"
                        << cppType
                        << "*>(args[0].value.v_usize);if(!instance)return WIO_INVOKE_BAD_ARGUMENTS;"
                           "result->type=WIO_ABI_USIZE;result->value.v_usize=reinterpret_cast<std::uintptr_t>(";
                    if (objectHandle)
                        out << member << ".Get()";
                    else
                        out << '&' << member;
                    out << ");return WIO_INVOKE_OK;}\n";
                    fieldInfo.getterExport = appendExport({reflected.logicalName + ".get." + field.name,
                                                           reflected.logicalName + ".get." + field.name,
                                                           "USIZE",
                                                           {"USIZE"},
                                                           getterInvoke});
                    if (field.isMutable)
                    {
                        const auto setterInvoke = "_legacy_type_call_" + std::to_string(legacy.size());
                        out << "static std::int32_t " << setterInvoke
                            << "(const WioValue* args,std::uint32_t count,WioValue*) noexcept {"
                               "if(count!=2u||!args)return WIO_INVOKE_BAD_ARGUMENTS;"
                               "if(args[0].type!=WIO_ABI_USIZE||args[1].type!=WIO_ABI_USIZE)return "
                               "WIO_INVOKE_TYPE_MISMATCH;auto* instance=reinterpret_cast<"
                            << cppType
                            << "*>(args[0].value.v_usize);if(!instance||!args[1].value.v_usize)return "
                               "WIO_INVOKE_BAD_ARGUMENTS;";
                        if (objectHandle)
                        {
                            out << member << "=wio::runtime::Ref<" << nominalCppType(field.type)
                                << ">(reinterpret_cast<" << nominalCppType(field.type) << "*>(args[1].value.v_usize));";
                        }
                        else
                        {
                            out << member << "=*reinterpret_cast<" << type(field.type) << "*>(args[1].value.v_usize);";
                        }
                        out << "return WIO_INVOKE_OK;}\n";
                        fieldInfo.setterExport = appendExport({reflected.logicalName + ".set." + field.name,
                                                               reflected.logicalName + ".set." + field.name,
                                                               "VOID",
                                                               {"USIZE", "USIZE"},
                                                               setterInvoke});
                    }
                    continue;
                }

                if (fieldKind == "UNKNOWN")
                {
                    const auto bridgeType = dynamicBridgeType(field.type);
                    const auto getterExpression = dynamicToHost(member, field.type, 0);
                    const auto setterExpression = dynamicFromHost("value", field.type, 0);
                    if (!bridgeType || !getterExpression || !setterExpression)
                        continue;

                    const auto bridgeIndex = legacy.size();
                    const auto rawGetter = "_legacy_raw_get_" + std::to_string(bridgeIndex);
                    const auto dynamicGetter = "_legacy_dynamic_get_" + std::to_string(bridgeIndex);
                    out << "static " << *bridgeType << ' ' << rawGetter << "(std::uintptr_t handle){auto* instance="
                        << "reinterpret_cast<" << cppType
                        << "*>(handle);if(!instance)throw std::invalid_argument(\"invalid Wio instance handle\");"
                           "return "
                        << *getterExpression << ";}\n";
                    out << "static WioErasedValue* " << dynamicGetter
                        << "(std::uintptr_t handle){try{return new WioErasedValueModel<" << *bridgeType
                        << ">(&_legacy_type_descriptor_" << fieldInfo.typeDescriptor << ',' << rawGetter
                        << "(handle));}catch(...){return nullptr;}}\n";
                    LegacyExportInfo getterExport{reflected.logicalName + ".get." + field.name,
                                                  reflected.logicalName + ".get." + field.name,
                                                  "UNKNOWN",
                                                  {"USIZE"},
                                                  "nullptr"};
                    getterExport.rawFunction = "reinterpret_cast<const void*>(&" + rawGetter + ')';
                    fieldInfo.getterExport = appendExport(std::move(getterExport));
                    fieldInfo.dynamicGetter = '&' + dynamicGetter;

                    if (field.isMutable)
                    {
                        const auto rawSetter = "_legacy_raw_set_" + std::to_string(bridgeIndex);
                        const auto dynamicSetter = "_legacy_dynamic_set_" + std::to_string(bridgeIndex);
                        out << "static void " << rawSetter << "(std::uintptr_t handle," << *bridgeType
                            << " value){auto* instance=reinterpret_cast<" << cppType
                            << "*>(handle);if(!instance)throw std::invalid_argument(\"invalid Wio instance handle\");"
                            << member << '=' << *setterExpression << ";}\n";
                        out << "static std::int32_t " << dynamicSetter
                            << "(std::uintptr_t handle,const WioErasedValue* value){if(!value)return "
                               "WIO_INVOKE_BAD_ARGUMENTS;auto* typedValue=dynamic_cast<const WioErasedValueModel<"
                            << *bridgeType << ">*>(value);if(!typedValue)return WIO_INVOKE_TYPE_MISMATCH;try{"
                            << rawSetter
                            << "(handle,typedValue->value);return WIO_INVOKE_OK;}catch(...){return "
                               "WIO_INVOKE_NOT_CALLABLE;}}\n";
                        LegacyExportInfo setterExport{reflected.logicalName + ".set." + field.name,
                                                      reflected.logicalName + ".set." + field.name,
                                                      "VOID",
                                                      {"USIZE", "UNKNOWN"},
                                                      "nullptr"};
                        setterExport.rawFunction = "reinterpret_cast<const void*>(&" + rawSetter + ')';
                        fieldInfo.setterExport = appendExport(std::move(setterExport));
                        fieldInfo.dynamicSetter = '&' + dynamicSetter;
                    }
                    continue;
                }

                const auto getterInvoke = "_legacy_type_call_" + std::to_string(legacy.size());
                out << "static std::int32_t " << getterInvoke
                    << "(const WioValue* args,std::uint32_t count,WioValue* result) noexcept {"
                       "if(count!=1u||!args||!result)return WIO_INVOKE_BAD_ARGUMENTS;"
                       "if(args[0].type!=WIO_ABI_USIZE)return WIO_INVOKE_TYPE_MISMATCH;"
                       "auto* instance=reinterpret_cast<"
                    << cppType << "*>(args[0].value.v_usize);if(!instance)return WIO_INVOKE_BAD_ARGUMENTS;"
                    << "result->type=WIO_ABI_" << fieldKind << ';' << assignResult(field.type, member)
                    << "return WIO_INVOKE_OK;}\n";
                fieldInfo.getterExport = appendExport({reflected.logicalName + ".get." + field.name,
                                                       reflected.logicalName + ".get." + field.name,
                                                       fieldKind,
                                                       {"USIZE"},
                                                       getterInvoke});

                if (field.isMutable)
                {
                    const auto setterInvoke = "_legacy_type_call_" + std::to_string(legacy.size());
                    out << "static std::int32_t " << setterInvoke
                        << "(const WioValue* args,std::uint32_t count,WioValue*) noexcept {"
                           "if(count!=2u||!args)return WIO_INVOKE_BAD_ARGUMENTS;"
                           "if(args[0].type!=WIO_ABI_USIZE||args[1].type!=WIO_ABI_"
                        << fieldKind << ")return WIO_INVOKE_TYPE_MISMATCH;auto* instance=reinterpret_cast<" << cppType
                        << "*>(args[0].value.v_usize);if(!instance)return WIO_INVOKE_BAD_ARGUMENTS;" << member << '='
                        << argumentExpression(field.type, 1) << ";return WIO_INVOKE_OK;}\n";
                    fieldInfo.setterExport = appendExport({reflected.logicalName + ".set." + field.name,
                                                           reflected.logicalName + ".set." + field.name,
                                                           "VOID",
                                                           {"USIZE", fieldKind},
                                                           setterInvoke});
                }
            }

            for (const auto& method : reflected.methods)
            {
                if (method.name == "OnConstruct" || method.visibility != FieldVisibility::Public || method.isAsync)
                    continue;
                const auto functionIt = functions.find(method.function);
                if (functionIt == functions.end() || functionIt->second->parameters.empty())
                    continue;
                bool compatible = legacyAbiKind(method.returnType) != "UNKNOWN";
                for (const auto parameter : method.parameterTypes)
                    compatible &= legacyAbiKind(parameter) != "UNKNOWN";
                if (!compatible)
                    continue;

                const auto& function = *functionIt->second;
                const auto receiverType = module.types.get(function.parameters.front().type).arguments.front();
                const auto receiverCppType = storageCppType(receiverType);
                const auto resultKind = legacyAbiKind(method.returnType);
                const auto invokeName = "_legacy_type_call_" + std::to_string(legacy.size());
                out << "static std::int32_t " << invokeName
                    << "(const WioValue* args,std::uint32_t count,WioValue* result) noexcept {if(count!="
                    << method.parameterTypes.size() + 1 << "u||!args" << (resultKind == "VOID" ? "" : "||!result")
                    << ")return WIO_INVOKE_BAD_ARGUMENTS;if(args[0].type!=WIO_ABI_USIZE)return "
                       "WIO_INVOKE_TYPE_MISMATCH;";
                for (std::size_t parameterIndex = 0; parameterIndex < method.parameterTypes.size(); ++parameterIndex)
                    out << "if(args[" << parameterIndex + 1 << "].type!=WIO_ABI_"
                        << legacyAbiKind(method.parameterTypes[parameterIndex]) << ")return WIO_INVOKE_TYPE_MISMATCH;";
                out << "auto* instance=reinterpret_cast<" << cppType
                    << "*>(args[0].value.v_usize);if(!instance)return WIO_INVOKE_BAD_ARGUMENTS;try{";
                if (resultKind != "VOID")
                    out << "auto value=";
                out << functionName(method.function) << '(';
                if (isObject)
                    out << "wio::wir_backend::Place<wio::runtime::Ref<" << receiverCppType << ">>::raw(static_cast<"
                        << receiverCppType << "*>(instance))";
                else
                    out << "wio::wir_backend::Place<" << receiverCppType << ">::borrow(static_cast<" << receiverCppType
                        << "&>(*instance))";
                for (std::size_t parameterIndex = 0; parameterIndex < method.parameterTypes.size(); ++parameterIndex)
                    out << ',' << argumentExpression(method.parameterTypes[parameterIndex], parameterIndex + 1);
                out << ");";
                if (resultKind != "VOID")
                    out << "result->type=WIO_ABI_" << resultKind << ';' << assignResult(method.returnType, "value");
                out << "return WIO_INVOKE_OK;}catch(...){return WIO_INVOKE_NOT_CALLABLE;}}\n";

                LegacyExportInfo methodExport;
                methodExport.logicalName = reflected.logicalName + '.' + method.name;
                methodExport.symbolName = methodExport.logicalName;
                methodExport.returnKind = resultKind;
                methodExport.parameterKinds.push_back("USIZE");
                for (const auto parameter : method.parameterTypes)
                    methodExport.parameterKinds.push_back(legacyAbiKind(parameter));
                LegacyMethodInfo methodInfo;
                methodInfo.descriptor = &method;
                methodInfo.exportIndex = appendExport(std::move(methodExport));
                legacy.back().invokeName = invokeName;
                legacyType.methods.push_back(std::move(methodInfo));
            }

            std::tie(legacyType.attributeCount, legacyType.attributes) = emitAttributeTable(reflected.attributes);
            for (auto& field : legacyType.fields)
                std::tie(field.attributeCount, field.attributes) = emitAttributeTable(field.descriptor->attributes);
            for (auto& method : legacyType.methods)
                std::tie(method.attributeCount, method.attributes) = emitAttributeTable(method.descriptor->attributes);
            legacyTypes.push_back(std::move(legacyType));
        }

        for (std::size_t exportIndex = 0; exportIndex < legacy.size(); ++exportIndex)
        {
            if (legacy[exportIndex].parameterKinds.empty())
                continue;
            out << "static const WioAbiType _legacy_export_params_" << exportIndex << "[]={";
            for (const auto& kind : legacy[exportIndex].parameterKinds)
                out << "WIO_ABI_" << kind << ',';
            out << "};\n";
        }
        if (!legacy.empty())
        {
            out << "static const WioModuleExport _legacy_exports[]={\n";
            for (std::size_t exportIndex = 0; exportIndex < legacy.size(); ++exportIndex)
            {
                const auto& entry = legacy[exportIndex];
                out << '{' << quoted(entry.logicalName) << ',' << quoted(entry.symbolName) << ",WIO_ABI_"
                    << entry.returnKind << ',' << entry.parameterKinds.size() << "u,"
                    << (entry.parameterKinds.empty() ? "nullptr"
                                                     : "_legacy_export_params_" + std::to_string(exportIndex))
                    << ',' << (entry.invokeName == "nullptr" ? "nullptr" : '&' + entry.invokeName) << ','
                    << entry.rawFunction << ',' << entry.attributeCount << "u," << entry.attributes << "},\n";
            }
            out << "};\n";
        }

        for (std::size_t typeIndex = 0; typeIndex < legacyTypes.size(); ++typeIndex)
        {
            const auto& legacyType = legacyTypes[typeIndex];
            if (!legacyType.constructors.empty())
            {
                out << "static const WioModuleConstructor _legacy_type_constructors_" << typeIndex << "[]={";
                for (const auto exportIndex : legacyType.constructors)
                    out << "{&_legacy_exports[" << exportIndex << "]},";
                out << "};\n";
            }
            if (!legacyType.fields.empty())
            {
                out << "static const WioModuleField _legacy_type_fields_" << typeIndex << "[]={\n";
                for (std::size_t fieldIndex = 0; fieldIndex < legacyType.fields.size(); ++fieldIndex)
                {
                    const auto& field = legacyType.fields[fieldIndex];
                    const auto supported = field.getterExport.has_value();
                    const auto flags = supported ? (1u | (field.descriptor->isMutable ? 2u : 4u)) : 0u;
                    out << '{' << quoted(field.descriptor->name) << ",WIO_ABI_" << legacyAbiKind(field.descriptor->type)
                        << ",&_legacy_type_descriptor_" << field.typeDescriptor << ',' << flags << "u,"
                        << legacyAccess(field.descriptor->visibility) << ','
                        << (supported ? "&_legacy_exports[" + std::to_string(*field.getterExport) + "]" : "nullptr")
                        << ','
                        << (field.setterExport ? "&_legacy_exports[" + std::to_string(*field.setterExport) + "]"
                                               : "nullptr")
                        << ',' << field.dynamicGetter << ',' << field.dynamicSetter << ',' << field.attributeCount
                        << "u," << field.attributes << "},\n";
                }
                out << "};\n";
            }
            if (!legacyType.methods.empty())
            {
                out << "static const WioModuleMethod _legacy_type_methods_" << typeIndex << "[]={\n";
                for (const auto& method : legacyType.methods)
                    out << '{' << quoted(method.descriptor->name) << ",&_legacy_exports[" << method.exportIndex << "],"
                        << method.attributeCount << "u," << method.attributes << "},\n";
                out << "};\n";
            }
        }
        if (!legacyTypes.empty())
        {
            out << "static const WioModuleType _legacy_types[]={\n";
            for (std::size_t typeIndex = 0; typeIndex < legacyTypes.size(); ++typeIndex)
            {
                const auto& legacyType = legacyTypes[typeIndex];
                out << '{' << quoted(legacyType.descriptor->logicalName) << ','
                    << quoted(legacyType.descriptor->logicalName) << ','
                    << (legacyType.descriptor->nominalKind == NominalKind::Object ? "WIO_MODULE_TYPE_OBJECT"
                                                                                  : "WIO_MODULE_TYPE_COMPONENT")
                    << ','
                    << (legacyType.createExport ? "&_legacy_exports[" + std::to_string(*legacyType.createExport) + "]"
                                                : "nullptr")
                    << ",&_legacy_exports[" << legacyType.destroyExport << "]," << legacyType.constructors.size()
                    << "u,"
                    << (legacyType.constructors.empty() ? "nullptr"
                                                        : "_legacy_type_constructors_" + std::to_string(typeIndex))
                    << ',' << legacyType.fields.size() << "u,"
                    << (legacyType.fields.empty() ? "nullptr" : "_legacy_type_fields_" + std::to_string(typeIndex))
                    << ',' << legacyType.methods.size() << "u,"
                    << (legacyType.methods.empty() ? "nullptr" : "_legacy_type_methods_" + std::to_string(typeIndex))
                    << ',' << legacyType.attributeCount << "u," << legacyType.attributes << "},\n";
            }
            out << "};\n";
        }

        std::vector<const LegacyExportInfo*> commands;
        std::vector<const LegacyExportInfo*> eventHooks;
        for (const auto& entry : legacy)
        {
            if (entry.contractExport && entry.contractExport->role == ModuleExportRole::Command)
                commands.push_back(&entry);
            else if (entry.contractExport && entry.contractExport->role == ModuleExportRole::EventHook)
                eventHooks.push_back(&entry);
        }
        if (!commands.empty())
        {
            out << "static const WioModuleCommand _legacy_commands[] = {\n";
            for (const LegacyExportInfo* entry : commands)
                out << '{' << quoted(entry->contractExport->roleName) << ",&_legacy_exports["
                    << legacySlots.at(entry->contractExport->stableId) << "]},\n";
            out << "};\n";
        }
        if (!eventHooks.empty())
        {
            out << "static const WioModuleEventHook _legacy_event_hooks[] = {\n";
            for (const LegacyExportInfo* entry : eventHooks)
                out << '{' << quoted(entry->logicalName) << ',' << quoted(entry->contractExport->roleName)
                    << ",&_legacy_exports[" << legacySlots.at(entry->contractExport->stableId) << "]},\n";
            out << "};\n";
        }
        const auto& l = module.contract.lifecycle;
        std::uint32_t flags = (l.apiVersion ? 1 : 0) | (l.load ? 2 : 0) | (l.update ? 4 : 0) | (l.unload ? 8 : 0) |
                              (l.saveState ? 16 : 0) | (l.restoreState ? 32 : 0) | (1u << 6);
        if (!legacyTypes.empty())
            flags |= 1u << 7;
        if (std::ranges::any_of(typeDescriptors, [](const LegacyTypeDescriptorInfo& descriptor)
                                { return descriptor.kind == "WIO_MODULE_TYPE_DESC_TEXT"; }))
            flags |= 1u << 8;
        if (attributeTableIndex != 0)
            flags |= 1u << 9;
        if (!legacyAsyncExports.empty())
            flags |= 1u << 11;
        const auto hook = [&](FunctionId id, const std::string& result, const std::string& parameter,
                              const std::string& arg, const std::string& fallback)
        {
            if (!id)
                return std::string("nullptr");
            return "+[](" + parameter + ") noexcept -> " + result + " { try { return " + functionName(id) + "(" + arg +
                   "); } catch (...) { " + fallback + " } }";
        };
        out << "static const WioSdkModuleContract "
               "_sdk_contract{WIO_SDK_MODULE_CONTRACT_VERSION,sizeof(WioSdkModuleContract),WIO_SDK_MODULE_WIO_LIBRARY,"
            << l.stateSchemaVersion << ',' << module.contract.stableId << "ULL," << quoted(module.contract.stableKey)
            << ',' << flags << ',' << exports.size() << ',' << (exports.empty() ? "nullptr" : "_sdk_exports") << ','
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
            << "a.restoreState=" << hook(l.restoreState, "std::int32_t", "std::int32_t state", "state", "return -1;")
            << ';'
            << (module.contract.application
                    ? "a.application=&_wio_application; a.capabilities|=WIO_MODULE_CAP_APPLICATION_HOST_V1;"
                    : "")
            << (module.contract.application && !module.contract.application->stages.empty()
                    ? "a.capabilities|=WIO_MODULE_CAP_APPLICATION_SCHEDULE_V1;"
                    : "")
            << "a.asyncExportCount=" << legacyAsyncExports.size()
            << "; a.asyncExports=" << (legacyAsyncExports.empty() ? "nullptr" : "_legacy_async_exports")
            << "; a.asyncHost=" << (legacyAsyncExports.empty() ? "nullptr" : "&_legacy_async_host") << ';'
            << "a.exportCount=" << legacy.size() << "; a.exports=" << (legacy.empty() ? "nullptr" : "_legacy_exports")
            << "; a.commandCount=" << commands.size()
            << "; a.commands=" << (commands.empty() ? "nullptr" : "_legacy_commands")
            << "; a.eventHookCount=" << eventHooks.size()
            << "; a.eventHooks=" << (eventHooks.empty() ? "nullptr" : "_legacy_event_hooks")
            << "; a.typeCount=" << legacyTypes.size()
            << "; a.types=" << (legacyTypes.empty() ? "nullptr" : "_legacy_types")
            << "; a.productVersion={WIO_SDK_VERSION_MAJOR,WIO_SDK_VERSION_MINOR,WIO_SDK_VERSION_PATCH}; return a; "
               "}();\n"
            << "extern \"C\" WIO_WIR_EXPORT const WioModuleApi* WioModuleGetApi() noexcept { return &_legacy_api; }\n"
            << "extern \"C\" WIO_WIR_EXPORT const WioSdkModuleContract* WioGetSdkModuleContract() noexcept { return "
               "&_sdk_contract; }\n";
        return out.str();
    }
} // namespace wio::codegen
