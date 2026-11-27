#include "wio/codegen/wir_native_requirements.h"
#include "wio/codegen/wir_native_bridge_plan.h"
#include "wio/common/logger.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"

#include <iostream>
#include <stdexcept>

namespace
{
    wio::wir::lowered::Module build(const std::string& source, const std::string& file = "requirements.wio")
    {
        wio::Lexer lexer(source, file);
        wio::Parser parser(lexer.lex());
        auto program = parser.parseProgram();
        wio::sema::SemanticAnalyzer analyzer;
        analyzer.analyze(program);
        if (wio::Logger::get().getErrorCount())
            throw std::runtime_error("fixture semantic analysis failed");
        wio::wir::typed::BuildOptions options;
        options.logicalModuleName = "requirements";
        auto typed = wio::wir::typed::Builder{}.build(program, options);
        if (!typed.succeeded())
            throw std::runtime_error("fixture typed WIR failed");
        auto lowered = wio::wir::LoweringPipeline{}.lower(typed.takeModule());
        if (!lowered.succeeded())
            throw std::runtime_error("fixture lowering failed");
        return lowered.takeModule();
    }
    void require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    try
    {
        using wio::codegen::WirNativeRequirements;
        const std::string declaration = "[Native, CppName(Answer)] fn Native(value: i32) -> i32;\n";
        const auto source = build(declaration + "fn Entry() -> i32 { return Native(41); }");
        const auto baseline = WirNativeRequirements::build(source);
        require(baseline.sha256.size() == 64 && baseline.nativeBindings.size() == 1, "missing native fingerprint");
        require(WirNativeRequirements::build(source).sha256 == baseline.sha256, "nondeterministic fingerprint");
        const auto changed = build("fn Unrelated() -> string { return \"ignored\"; }\n" + declaration +
                                       "component VmOnly { public value: string; }\nfn Entry() -> i32 { let x = "
                                       "VmOnly(); return Native(9) + 2; }",
                                   "moved.wio");
        require(WirNativeRequirements::build(changed).sha256 == baseline.sha256,
                "unrelated VM bodies, source locations or sparse IDs affected native requirements");
        auto signature = source;
        for (auto& function : signature.functions)
            if (function.nativeBinding)
                function.nativeBinding->header = "other.h";
        require(WirNativeRequirements::build(signature).sha256 != baseline.sha256, "header change was lost");
        signature = source;
        for (auto& function : signature.functions)
            if (function.nativeBinding)
                function.nativeBinding->callingConvention = wio::wir::NativeCallingConvention::Cdecl;
        require(WirNativeRequirements::build(signature).sha256 != baseline.sha256,
                "calling convention change was lost");
        const auto pod = [](std::string field)
        {
            return build("[Native, CppName(Point)] component Point { public x: " + field +
                         "; }\n"
                         "[Native, CppName(Read)] fn Read(point: Point) -> i32;\nfn Entry() -> i32 { return 0; }");
        };
        require(WirNativeRequirements::build(pod("i32")).sha256 != WirNativeRequirements::build(pod("i64")).sha256,
                "native POD field layout change was lost");
        const auto generic = [](std::string type)
        {
            return build("[Native, CppName(Echo), Instantiate(" + type +
                         ")] fn Echo<T>(value: T) -> T;\n"
                         "fn Entry() -> i32 { Echo<" +
                         type + ">(3" + type + "); return 0; }");
        };
        require(WirNativeRequirements::build(generic("i32")).sha256 !=
                    WirNativeRequirements::build(generic("f64")).sha256,
                "native generic instantiation change was lost");
        const auto payload = [](std::string expression)
        {
            return build("[Native, CppName(Echo)] fn Echo(value: any) -> any;\n"
                         "fn Entry() -> i32 { let value: any = " +
                         expression + "; Echo(value); return 0; }");
        };
        require(WirNativeRequirements::build(payload("3")).sha256 !=
                    WirNativeRequirements::build(payload("\"text\"")).sha256,
                "dynamic native Any payload change was lost");
        const auto embedded = [](std::string value)
        {
            return build("component Payload { public value: i32 = " + value +
                         "; }\n"
                         "[Native, CppName(Read)] fn Read(value: Payload) -> i32;\nfn Entry() -> i32 { return 0; }");
        };
        require(WirNativeRequirements::build(embedded("3")).sha256 !=
                    WirNativeRequirements::build(embedded("7")).sha256,
                "C++-retained field initialization body change was lost");
        std::cout << "Native requirements cover ABI/layout/generics/Any and retained C++ bodies, not VM-only bodies\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
