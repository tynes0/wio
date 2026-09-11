#include "wio/codegen/wir_cpp_text.h"
#include "wio/codegen/wir_application_emitter.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wio::codegen {
std::string WirApplicationEmitter::emit(const wir::lowered::Module& module,
                                        const WirModuleEmitter::TypeSpelling& type) {
    if (!module.contract.application)
        return {};
    const auto& app = *module.contract.application;
    const auto& layout = module.types.get(app.type);
    std::ostringstream out;
    auto field = [&](std::string_view name) {
        return "value._f" +
               std::to_string(std::ranges::find(layout.fields, name, &wir::FieldLayout::name) - layout.fields.begin());
    };
    auto call = [&](wir::FunctionId id, std::string args) {
        const auto& fn = *std::ranges::find(module.functions, id, &wir::lowered::Function::id);
        return "_wio_f" + std::to_string(id.value()) + "(" + type(fn.parameters.front().type) + "::borrow(value)" +
               args + ")";
    };
    out << "struct _wio_app_hooks {\n"
           "    using Value = "
        << type(app.type)
        << ";\n\n"
           "    static void construct(Value& value) {\n"
           "        "
        << call(app.construct, "")
        << ";\n"
           "    }\n\n"
           "    static void start(Value& value) {\n"
           "        "
        << call(app.start, "")
        << ";\n"
           "    }\n\n"
           "    static void update(Value& value, double delta) {\n"
           "        "
        << call(app.update, ", delta")
        << ";\n"
           "    }\n\n"
           "    static void close(Value& value) {\n"
           "        "
        << call(app.close, "")
        << ";\n"
           "    }\n\n"
           "    static void exit(Value& value, std::int32_t code) {\n"
           "        "
        << call(app.exit, ", code")
        << ";\n"
           "    }\n\n"
           "    static bool exited(const Value& value) {\n"
           "        return "
        << field("__exitRequested")
        << ";\n"
           "    }\n\n"
           "    static std::int32_t code(const Value& value) {\n"
           "        return "
        << field("__exitCode")
        << ";\n"
           "    }\n"
           "};\n\n"
           "using _wio_app_host = wio::wir_backend::ApplicationHost<_wio_app_hooks>;\n";
    if (!app.stages.empty()) {
        out << "static const WioApplicationStageDescriptor _wio_app_stages[] = {\n";
        for (const auto& stage : app.stages) {
            unsigned flags = (stage.kind == wir::ApplicationStageKind::Fixed ? 1 : 0) |
                             (stage.affinity == wir::ApplicationAffinity::Main ? 2 : 0) |
                             (stage.legacyExplicit ? 16 : 0);
            for (const auto& run : stage.runs)
                flags |= run.applicationTarget ? 8 : 4;
            out << "    {" << WirCppText::quote(stage.name) << ", " << WirCppText::quote(stage.after) << ", "
                << std::setprecision(17) << stage.fixedHz << ", " << stage.order << ", " << flags << "},\n";
        }
        out << "};\n\n";
    }
    out << "static const WioApplicationDescriptor _wio_application = _wio_app_host::descriptor("
        << WirCppText::quote(app.logicalName) << ", " << app.stages.size() << ", "
        << (app.stages.empty() ? "nullptr" : "_wio_app_stages") << ");\n";
    return out.str();
}
} // namespace wio::codegen
