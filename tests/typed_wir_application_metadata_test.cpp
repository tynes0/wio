#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"
#include "wio/wir/typed_ir_verifier.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }

    wio::ComponentDeclaration* findApplication(const wio::Ref<wio::Program>& program)
    {
        for (const auto& statement : program->statements)
        {
            const auto* group = statement ? statement->as<wio::DeclarationGroup>() : nullptr;
            if (!group) continue;
            for (const auto& declaration : group->declarations)
                if (auto* component = declaration ? declaration->as<wio::ComponentDeclaration>() : nullptr;
                    component && component->attributeTargetOverride == "application")
                    return component;
        }
        return nullptr;
    }

    wio::FunctionDeclaration* findApplicationEntry(const wio::Ref<wio::Program>& program)
    {
        for (const auto& statement : program->statements)
        {
            const auto* group = statement ? statement->as<wio::DeclarationGroup>() : nullptr;
            if (!group) continue;
            for (const auto& declaration : group->declarations)
                if (auto* function = declaration ? declaration->as<wio::FunctionDeclaration>() : nullptr;
                    function && function->isApplicationEntry)
                    return function;
        }
        return nullptr;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::wir;
    namespace typed = wio::wir::typed;
    namespace lowered = wio::wir::lowered;

    bool ok = true;
    Lexer lexer(
        "attribute app_tag(name: string) for application retain runtime; "
        "attribute system_tag() for system retain compile; "
        "attribute state_tag() for field retain runtime; "
        "attribute arg_tag() for parameter retain compile; "
        "attribute legacy(message: string) for enum_case retain compile; "
        "enum Mode { online, [legacy(\"use online\")] compatibility }; "
        "fn Identity([arg_tag] value: i32) -> i32 { return value; } "
        "component World { [state_tag] value: i32 = 0; } "
        "[system_tag] system Motion { "
        "  on update(delta: f64, world: ref World) { if (delta >= 0.0) { world.value += 1; } } "
        "} "
        "[app_tag(\"demo\")] application Demo { "
        "  resource world: World; "
        "  system motion: Motion; "
        "  schedule { "
        "    stage simulation on main { run motion.update(ref self.world); } "
        "    stage finish after simulation on main { run self.update; } "
        "  } "
        "  on update { self.Exit(0); } "
        "}",
        "typed_wir_application_metadata_test.wio");
    Parser parser(lexer.lex());
    const Ref<Program> program = parser.parseProgram();
    // The focused unit builder does not run Compiler::parseAndMerge, which is
    // responsible for std::async. Keep parser-produced application metadata,
    // but replace only the synthetic host loop with a self-contained entry.
    if (FunctionDeclaration* entry = findApplicationEntry(program))
    {
        const common::Location location = entry->location();
        std::vector<NodePtr<Statement>> statements;
        statements.push_back(makeNodePtr<ReturnStatement>(
            makeNodePtr<IntegerLiteral>(Token{
                .type = TokenType::integerLiteral,
                .value = "0",
                .loc = location
            }), location));
        entry->body = makeNodePtr<BlockStatement>(std::move(statements), location);
    }
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    ComponentDeclaration* applicationAst = findApplication(program);
    ok &= expect(applicationAst && !applicationAst->attributes.empty(),
        "Fixture must expose the desugared application component and its attribute");
    if (applicationAst && !applicationAst->attributes.empty())
    {
        applicationAst->attributes.front()->processorBindings.push_back(AttributeStatement::ProcessorBinding{
            .canonicalTypeName = "demo::GuardProcessor",
            .phase = "pre",
            .hookCppName = "BeforeUpdate",
            .hookMode = "body"
        });
    }

    typed::BuildResult build = typed::Builder{}.build(program, typed::BuildOptions{
        .logicalModuleName = "example.application_metadata"
    });
    if (!build.succeeded())
        for (const auto& diagnostic : build.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(build.succeeded(), "Application/system source must build into Typed WIR");
    const typed::Module& module = build.module();
    ok &= expect(module.contract.application.has_value() && module.contract.systems.size() == 1,
        "Module contract must expose one application and one owned system type");

    if (module.contract.application)
    {
        const ApplicationDescriptor& application = *module.contract.application;
        ok &= expect(application.logicalName == "Demo" && application.entry && application.start &&
            application.update && application.close && application.exit && application.systems.size() == 1,
            "Application identity and complete lifecycle must be frozen in WIR");
        ok &= expect(application.stages.size() == 2 && application.stages[0].name == "simulation" &&
            application.stages[0].affinity == ApplicationAffinity::Main &&
            application.stages[1].after == "simulation",
            "Schedule order, dependency, and main affinity must survive parsing");
        ok &= expect(application.stages[0].runs.size() == 1 &&
            application.stages[0].runs[0].targetName == "motion" &&
            application.stages[0].runs[0].resources.size() == 1 &&
            application.stages[0].runs[0].resources[0].name == "world" &&
            application.stages[0].runs[0].resources[0].access == ResourceAccess::Write,
            "System runs must resolve their callable and mutable resource access");
        ok &= expect(application.stages[1].runs.size() == 1 &&
            application.stages[1].runs[0].applicationTarget,
            "Application update must remain an explicit schedule run");
    }

    const auto appAttribute = std::ranges::find_if(module.contract.attributes,
        [](const AttributeApplicationDescriptor& attribute)
        {
            return attribute.canonicalName.find("app_tag") != std::string::npos;
        });
    ok &= expect(appAttribute != module.contract.attributes.end() && appAttribute->runtimeRetained &&
        appAttribute->targetKind == MetadataTargetKind::Application &&
        appAttribute->arguments.size() == 1 && appAttribute->arguments[0].sourceText == "demo" &&
        appAttribute->processors.size() == 1 &&
        appAttribute->processors[0].phase == AttributeProcessorPhase::Pre,
        "Attribute origin, target, arguments, retention, and processor phase must be backend-neutral metadata");
    ok &= expect(std::ranges::any_of(module.contract.attributes,
            [](const AttributeApplicationDescriptor& attribute)
            {
                return attribute.targetKind == MetadataTargetKind::Parameter &&
                    attribute.canonicalName.find("arg_tag") != std::string::npos;
            }),
        "Parameter attributes must keep their function target and parameter index");

    const auto worldReflection = std::ranges::find_if(module.contract.reflection,
        [](const ReflectionDescriptor& descriptor) { return descriptor.logicalName == "World"; });
    ok &= expect(worldReflection != module.contract.reflection.end() &&
        worldReflection->fields.size() == 1 && worldReflection->fields[0].name == "value" &&
        worldReflection->fields[0].isMutable && worldReflection->fields[0].attributes.size() == 1,
        "Reflection must expose field layout and its applied attributes");
    const auto modeReflection = std::ranges::find_if(module.contract.reflection,
        [](const ReflectionDescriptor& descriptor) { return descriptor.logicalName == "Mode"; });
    ok &= expect(modeReflection != module.contract.reflection.end() && modeReflection->cases.size() == 2 &&
        modeReflection->cases[1].name == "compatibility" && modeReflection->cases[1].attributes.size() == 1,
        "Enum/flagset cases and their attributes must be reflected with stable identities");

    const auto typedVerification = typed::Verifier{}.verify(module);
    if (!typedVerification.succeeded())
        for (const auto& diagnostic : typedVerification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(typedVerification.succeeded(), "Application/attribute Typed WIR must verify");
    const std::string typedText = typed::Printer{}.print(module);
    ok &= expect(typedText.find("application \"Demo\"") != std::string::npos &&
        typedText.find("stage[0] \"simulation\"") != std::string::npos &&
        typedText.find("access=write") != std::string::npos &&
        typedText.find("phase=pre") != std::string::npos,
        "Typed WIR text must expose application scheduling and attribute processors");

    LoweringResult lowering = LoweringPipeline{}.lower(module);
    ok &= expect(lowering.succeeded() && lowering.module().contract == module.contract,
        "Lowering must preserve application, system, attribute, and reflection metadata exactly");
    ok &= expect(lowered::Verifier{}.verify(lowering.module()).succeeded() &&
        lowered::Printer{}.print(lowering.module()).find("application \"Demo\"") != std::string::npos,
        "Lowered WIR must verify and print the same application contract");

    typed::Module damaged = module;
    if (damaged.contract.application && damaged.contract.application->stages.size() > 1)
        damaged.contract.application->stages[1].order = 9;
    ok &= expect(!typed::Verifier{}.verify(damaged).succeeded(),
        "Verifier must reject a non-canonical application schedule");
    return ok ? 0 : 1;
}
