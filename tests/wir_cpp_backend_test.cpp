#include "wio/codegen/wir_cpp_backend.h"
#include "wio/wir/generic_specializer.h"
#include "wio/wir/hierarchy_lowering.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"
#include "wio/wir/typed_ir_builder.h"
#include "wio/wir/typed_ir_printer.h"

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#ifndef WIO_TEST_CXX_COMPILER
#define WIO_TEST_CXX_COMPILER "c++"
#endif
#ifndef WIO_TEST_RUNTIME_INCLUDE
#define WIO_TEST_RUNTIME_INCLUDE "runtime/include"
#endif
#ifndef WIO_TEST_CXX_COMPILER_ID
#define WIO_TEST_CXX_COMPILER_ID ""
#endif

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }

    void initializeContract(wio::wir::lowered::Module& module, const std::string& name)
    {
        module.name = name;
        module.contract.logicalName = name;
        module.contract.stableKey = "program:" + name;
        module.contract.stableId = wio::wir::stableModuleHash(module.contract.stableKey);
        module.contract.callTable.stableId = wio::wir::stableModuleHash(
            module.contract.stableKey + ":sdk-call-table:v" +
            std::to_string(wio::wir::ModuleAbiDescriptorVersion));
    }

    wio::wir::lowered::Module makeExecutableModule()
    {
        using namespace wio::wir;
        namespace lowered = wio::wir::lowered;

        lowered::Module module;
        initializeContract(module, "wir-cpp-backend-test");
        const TypeId u32Type = module.types.intern(Type{.kind = TypeKind::U32});
        const TypeId enumType = module.types.internNominal(Type{
            .kind = TypeKind::Named,
            .name = "State",
            .nominalKind = NominalKind::Enum,
            .enumUnderlyingType = module.types.i32Type(),
            .enumCases = {{"Idle", 0}, {"Ready", 7}}
        });
        const TypeId flagsetType = module.types.internNominal(Type{
            .kind = TypeKind::Named,
            .name = "Permission",
            .nominalKind = NominalKind::Flagset,
            .enumUnderlyingType = u32Type,
            .enumCases = {{"None", 0}, {"Read", 1}, {"Write", 2}}
        });
        lowered::Function entry;
        entry.id = FunctionId{0};
        entry.name = "Entry";
        entry.returnType = module.types.i32Type();
        entry.callableType = module.types.intern(Type{
            .kind = TypeKind::Function,
            .arguments = {module.types.i32Type()},
            .ownership = OwnershipModel::ReferenceCounted,
            .cleanup = CleanupKind::ReleaseReference
        });
        lowered::BasicBlock block;
        block.id = BlockId{0};
        block.name = "entry";
        block.instructions = {
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{0},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{4}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{1},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{5}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Binary,
                .result = ValueId{2},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{0}, ValueId{1}},
                .binaryOperator = typed::BinaryOperator::Add
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::EnumConstant,
                .result = ValueId{3},
                .resultType = enumType,
                .selector = "Ready",
                .intrinsicFamily = IntrinsicFamily::Enum,
                .targetType = enumType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::IntrinsicCall,
                .result = ValueId{4},
                .resultType = module.types.stringType(),
                .operands = {ValueId{3}},
                .selector = "Name",
                .signatureTypes = {enumType},
                .intrinsicFamily = IntrinsicFamily::Enum,
                .targetType = enumType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::EnumConstant,
                .result = ValueId{5},
                .resultType = flagsetType,
                .selector = "Read",
                .intrinsicFamily = IntrinsicFamily::Flagset,
                .targetType = flagsetType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::EnumConstant,
                .result = ValueId{6},
                .resultType = flagsetType,
                .selector = "Write",
                .intrinsicFamily = IntrinsicFamily::Flagset,
                .targetType = flagsetType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Binary,
                .result = ValueId{7},
                .resultType = flagsetType,
                .operands = {ValueId{5}, ValueId{6}},
                .binaryOperator = typed::BinaryOperator::BitwiseOr
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::IntrinsicCall,
                .result = ValueId{8},
                .resultType = module.types.boolType(),
                .operands = {ValueId{7}, ValueId{5}},
                .selector = "Has",
                .signatureTypes = {flagsetType, flagsetType},
                .intrinsicFamily = IntrinsicFamily::Flagset,
                .targetType = flagsetType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Return,
                .operands = {ValueId{2}}
            }
        };
        entry.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(entry));
        return module;
    }

    wio::wir::lowered::Module makeUnsupportedModule()
    {
        using namespace wio::wir;
        namespace lowered = wio::wir::lowered;

        lowered::Module module;
        initializeContract(module, "wir-cpp-unsupported-test");
        const TypeId arrayType = module.types.intern(Type{
            .kind = TypeKind::Array,
            .arguments = {module.types.i32Type()},
            .ownership = OwnershipModel::OwnedValue,
            .cleanup = CleanupKind::DestroyValue
        });
        const TypeId iteratorType = module.types.intern(Type{
            .kind = TypeKind::Iterator,
            .arguments = {module.types.i32Type()}
        });
        lowered::Function entry;
        entry.id = FunctionId{0};
        entry.name = "Entry";
        entry.returnType = module.types.i32Type();
        entry.callableType = module.types.intern(Type{
            .kind = TypeKind::Function,
            .arguments = {module.types.i32Type()},
            .ownership = OwnershipModel::ReferenceCounted,
            .cleanup = CleanupKind::ReleaseReference
        });
        lowered::BasicBlock block;
        block.id = BlockId{0};
        block.name = "entry";
        block.instructions = {
            lowered::Instruction{
                .opcode = lowered::Opcode::ArrayCreate,
                .result = ValueId{0},
                .resultType = arrayType,
                .storageClass = lowered::StorageClass::Heap,
                .escapeClass = lowered::EscapeClass::Local
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::IntrinsicCall,
                .result = ValueId{1},
                .resultType = iteratorType,
                .operands = {ValueId{0}},
                .selector = "UnsupportedOperation",
                .signatureTypes = {arrayType},
                .intrinsicFamily = IntrinsicFamily::Array,
                .targetType = arrayType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{2},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{0}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Return,
                .operands = {ValueId{2}}
            }
        };
        entry.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(entry));
        return module;
    }

    std::optional<wio::wir::lowered::Module> makeLanguageSurfaceModule(const std::string& customSource = {},
        wio::wir::typed::Module* sourceModule = nullptr)
    {
        using namespace wio;
        Lexer lexer(
            customSource.empty() ? R"WIO(
realm std {
    object Option<T> {
        private present: bool;
        private value: T;
        OnConstruct(value: T) { self.present = true; self.value = value; }
        OnConstruct() { self.present = false; }
    }
    component ResultError { public code: i32; }
    object Result<T> {
        private ok: bool;
        private value: T;
        private error: ResultError;
        OnConstruct(value: T) { self.ok = true; self.value = value; }
        OnConstruct(error: ResultError) { self.ok = false; self.error = error; }
    }
}
enum State { Idle = 0, Ready = 7 };
flagset Permission { None = 0u32, Read = 1u32, Write = 1u32 << 1u32 };
fn TryValue() -> std::Result<i32> { return std::Result<i32>(7); }
fn Forward() -> std::Result<i32> {
    let value = TryValue?();
    return std::Result<i32>(value + 2);
}
fn Entry() -> i32 {
    let option = std::Option<i32>(4);
    let fromOption = match (option) { Some(value): value; None(): 0; };
    let state = State::Ready;
    let permissions = Permission::Read | Permission::Write;
    if (state.Name() == "Ready" and permissions.Has(Permission::Read)) {
        return fromOption + Forward!();
    }
    return 1;
}
)WIO" : customSource,
            "wir_cpp_backend_values.wio");
        Parser parser(lexer.lex());
        const Ref<Program> program = parser.parseProgram();
        sema::SemanticAnalyzer analyzer;
        analyzer.analyze(program);
        auto typed = wir::typed::Builder{}.build(program);
        if (!typed.succeeded())
        {
            for (const auto& diagnostic : typed.diagnostics())
                std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
            std::cerr << wir::typed::Printer{}.print(typed.module());
            return std::nullopt;
        }
        if (sourceModule) *sourceModule = typed.module();
        auto lowered = wir::LoweringPipeline{}.lower(typed.module());
        if (!lowered.succeeded())
        {
            auto specialized = typed.module();
            const auto specializationDiagnostics = wir::GenericSpecializer{}.specialize(specialized);
            std::cerr << wir::typed::Printer{}.print(specialized);
            for (const auto& diagnostic : lowered.diagnostics())
                std::cerr << diagnostic.code << " [" << diagnostic.source.begin.toDiagnosticString()
                          << "]: " << diagnostic.message << '\n';
            std::cerr << wir::lowered::Printer{}.print(lowered.module());
            return std::nullopt;
        }
        return lowered.takeModule();
    }

    bool compileGeneratedCode(const std::string& code, const bool run = false)
    {
        namespace fs = std::filesystem;
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path directory = fs::temp_directory_path() /
            ("wio-wir-cpp-backend-" + std::to_string(nonce));
        std::error_code error;
        fs::create_directories(directory, error);
        if (error) return false;
        const fs::path source = directory / "generated.cpp";
        const fs::path object = directory / (run ? "generated.exe" : "generated.obj");
        {
            std::ofstream stream(source, std::ios::binary);
            stream << code;
        }

        const std::string compiler = WIO_TEST_CXX_COMPILER;
        const std::string compilerId = WIO_TEST_CXX_COMPILER_ID;
        std::string command = "\"" + compiler + "\" ";
        if (compilerId == "MSVC")
        {
            command += "/nologo /std:c++20 /EHsc /c \"" + source.string() + "\" /I\"" +
                std::string(WIO_TEST_RUNTIME_INCLUDE) + "\" /Fo\"" + object.string() + "\"";
        }
        else
        {
            command += std::string("-std=c++20 ") + (run ? "" : "-c ") + "\"" + source.string() + "\" -I\"" +
                std::string(WIO_TEST_RUNTIME_INCLUDE) + "\" -o \"" + object.string() + "\"";
            if (run) command += " \"" + std::string(WIO_TEST_RUNTIME_LIBRARY) + "\"";
        }
#if defined(_WIN32)
        command = "\"" + command + "\"";
#endif
        int result = std::system(command.c_str());
        if (result == 0 && run)
        {
            std::string runCommand = "\"" + object.string() + "\"";
#if defined(_WIN32)
            runCommand = "\"" + runCommand + "\"";
#endif
            result = std::system(runCommand.c_str());
            if (result != 0) std::cerr << "Generated program returned status " << result << '\n';
        }
        fs::remove_all(directory, error);
        return result == 0;
    }
}

int main(int argc, char** argv)
{
    using wio::codegen::WirCppBackend;

    if (argc == 2 && std::string(argv[1]) == "--objects-only")
    {
        std::ifstream fixture(std::string(WIO_TEST_SOURCE_DIR) + "/tests/wir_cpp_objects_run.wio");
        const std::string source{std::istreambuf_iterator<char>(fixture), std::istreambuf_iterator<char>()};
        if (!expect(!source.empty(), "object fixture should be readable")) return 1;
        wio::wir::typed::Module original;
        const auto module = makeLanguageSurfaceModule(source, &original);
        if (!expect(module.has_value(), "object fixture must lower")) return 1;
        const auto generated = WirCppBackend{}.generate(*module);
        for (const auto& diagnostic : generated.diagnostics()) std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        if (!generated.succeeded()) { std::cerr << wio::wir::lowered::Printer{}.print(*module); return 1; }
        bool ok = true;
        const auto repeated = wio::wir::LoweringPipeline{}.lower(original);
        ok &= expect(repeated.succeeded() && wio::wir::lowered::Printer{}.print(repeated.module()) ==
            wio::wir::lowered::Printer{}.print(*module), "generic hierarchy lowering must be deterministic");
        auto broken = *module;
        bool damaged = false;
        for (std::size_t i = 0; i < broken.types.size(); ++i)
        {
            auto& type = broken.types.getMutable(wio::wir::TypeId{static_cast<std::uint32_t>(i)});
            if (type.name == "Derived" && !type.dispatchEntries.empty())
            { ++type.dispatchEntries.front().slot; damaged = true; break; }
        }
        ok &= expect(damaged && !wio::wir::lowered::Verifier{}.verify(broken).succeeded(), "invalid dispatch contracts must fail verification");
        ok &= expect(!WirCppBackend{}.generate(broken).succeeded(), "invalid dispatch must fail closed before emission");
        broken = *module;
        for (auto& function : broken.functions)
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                    if (instruction.opcode == wio::wir::lowered::Opcode::FieldPlace) ++instruction.projectionIndex;
        ok &= expect(!wio::wir::lowered::Verifier{}.verify(broken).succeeded(), "invalid declaring-subobject field indices must fail verification");
        broken = *module;
        for (auto& function : broken.functions)
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                    if (instruction.opcode == wio::wir::lowered::Opcode::ConstructObject)
                        instruction.callee = wio::wir::FunctionId{999999};
        ok &= expect(!WirCppBackend{}.generate(broken).succeeded(), "unknown constructor body must fail before emission");
        broken = *module;
        for (std::size_t i = 0; i < broken.types.size(); ++i)
        {
            const wio::wir::TypeId id{static_cast<std::uint32_t>(i)};
            if (broken.types.get(id).name == "Base") { broken.types.getMutable(id).baseTypes.push_back(id); break; }
        }
        ok &= expect(!wio::wir::lowerHierarchy(broken).empty(), "cyclic hierarchy must fail bounded lowering");
        const auto typeId = [&](const std::string& name)
        {
            for (std::size_t i = 0; i < module->types.size(); ++i) if (module->types.types()[i].name == name) return std::to_string(i);
            throw std::runtime_error("missing test type " + name);
        };
        const auto functionId = [&](const std::string& name)
        {
            for (const auto& function : module->functions) if (function.name == name) return std::to_string(function.id.value());
            throw std::runtime_error("missing test function " + name);
        };
        std::string orderGlobal;
        for (const auto& global : module->globals) if (global.name == "destructionOrder") orderGlobal = "_wio_g" + std::to_string(global.id.value());
        std::string harness = "#define main wio_object_entry\n" + generated.code() + "\n#undef main\nint main() {\n"
            "  if (int result = wio_object_entry()) return result;\n"
            "  bool rejected = false; try { (void)_wio_f" + functionId("InvalidFit") + "(); } catch (const std::runtime_error&) { rejected = true; }\n"
            "  if (!rejected) return 91;\n"
            "  using D = _wio_obj" + typeId("Derived") + "; using I = _wio_obj" + typeId("IRead") + ";\n"
            "  if (wio::wir_backend::object_cast<I>(wio::runtime::Ref<D>{})) return 90;\n"
            "  auto object = wio::runtime::Ref<D>::Create();\n"
            "  auto borrow = wio::wir_backend::Place<wio::runtime::Ref<D>>::borrow(object);\n"
            "  if (object->StrongCount() != 1) return 92;\n"
            "  wio::runtime::Ref<I> alias(wio::wir_backend::object_cast<I>(borrow));\n"
            "  if (object->StrongCount() != 2 || wio::wir_backend::object_identity(object) != wio::wir_backend::object_identity(alias)) return 93;\n"
            "  using L = _wio_obj" + typeId("Life") + "; " + orderGlobal + " = 0;\n"
            "  auto life = wio::runtime::Ref<L>::Create(); wio::runtime::WeakRef<L> weak(life); life.Reset();\n"
            "  if (weak.Lock() || " + orderGlobal + " != 0) return 94;\n"
            "  weak.Reset(); if (" + orderGlobal + " != 21) return 95;\n"
            "  return 0;\n}\n";
        ok &= expect(compileGeneratedCode(harness, true), "object/interface source and lifetime/cast boundary probes must execute");
        return ok ? 0 : 1;
    }

    if (argc == 2 && std::string(argv[1]) == "--generics-only")
    {
        std::ifstream fixture(std::string(WIO_TEST_SOURCE_DIR) + "/tests/wir_cpp_generics_run.wio");
        const std::string source{std::istreambuf_iterator<char>(fixture), std::istreambuf_iterator<char>()};
        if (!expect(!source.empty(), "generic fixture should be readable")) return 1;
        wio::wir::typed::Module original;
        const auto module = makeLanguageSurfaceModule(source, &original);
        if (!expect(module.has_value(), "generic source must materialize and lower")) return 1;
        bool ok = true;
        auto specialized = original;
        ok &= expect(wio::wir::GenericSpecializer{}.specialize(specialized).empty(), "materialization should succeed");
        const auto snapshot = wio::wir::typed::Printer{}.print(specialized);
        ok &= expect(wio::wir::GenericSpecializer{}.specialize(specialized).empty() &&
            snapshot == wio::wir::typed::Printer{}.print(specialized), "materialization must be idempotent");
        auto repeated = original;
        ok &= expect(wio::wir::GenericSpecializer{}.specialize(repeated).empty() &&
            snapshot == wio::wir::typed::Printer{}.print(repeated), "specialization identities must be deterministic");
        const auto count = [&](const std::string& prefix)
        {
            return std::ranges::count_if(module->functions, [&](const auto& function)
                { return function.name.starts_with(prefix + "$specialized."); });
        };
        ok &= expect(count("Identity") == 3 && count("Repeat") == 2 && count("FixedHead") == 1 &&
            count("MutualA") == 1 && count("MutualB") == 1,
            "duplicate calls, recursion, and inferred/explicit const arguments must share concrete bodies");
        auto limited = original;
        const auto limit = wio::wir::GenericSpecializer{1}.specialize(limited);
        ok &= expect(std::ranges::any_of(limit, [](const auto& d) { return d.code == "WIR3101"; }),
            "expansion limits must fail with a stable diagnostic");
        auto invalid = original;
        bool changed = false;
        for (auto& function : invalid.functions)
            if (function.name == "Entry")
                for (auto& block : function.blocks)
                    for (auto& instruction : block.instructions)
                        if (!changed && !instruction.genericArguments.empty())
                        { instruction.genericArguments.front() = invalid.types.stringType(); changed = true; }
        const auto rejected = wio::wir::GenericSpecializer{}.specialize(invalid);
        ok &= expect(changed && std::ranges::any_of(rejected, [](const auto& d) { return d.code == "WIR3100"; }),
            "conflicting pinned generic arguments must not produce an incorrect specialization");
        // A concrete function-reference signature is already a WIR contract;
        // exercise it independently of contextual generic-reference source syntax.
        auto references = original;
        using namespace wio::wir;
        const TypeId callable = references.types.intern(Type{.kind = TypeKind::Function,
            .arguments = {references.types.i32Type(), references.types.i32Type()},
            .ownership = OwnershipModel::ReferenceCounted, .cleanup = CleanupKind::ReleaseReference});
        typed::Function probe;
        probe.id = FunctionId{std::ranges::max(references.functions, {}, [](const auto& f) { return f.id.value(); }).id.value() + 1};
        probe.name = "GenericReferenceProbe";
        probe.returnType = references.types.i32Type();
        probe.callableType = references.types.intern(Type{.kind = TypeKind::Function,
            .arguments = {probe.returnType}, .ownership = OwnershipModel::ReferenceCounted, .cleanup = CleanupKind::ReleaseReference});
        const auto identity = std::ranges::find(references.functions, std::string("Identity"), &typed::Function::name);
        typed::BasicBlock body;
        body.id = BlockId{0};
        body.name = "entry";
        body.instructions = {
            typed::Instruction{.opcode = typed::Opcode::FunctionReference, .result = ValueId{0}, .resultType = callable,
                .callee = identity->id, .specializationKey = "reference-probe", .resultOwnership = typed::ValueOwnership::Owned},
            typed::Instruction{.opcode = typed::Opcode::Constant, .result = ValueId{1}, .resultType = probe.returnType,
                .literal = std::int64_t{42}},
            typed::Instruction{.opcode = typed::Opcode::IndirectCall, .result = ValueId{2}, .resultType = probe.returnType,
                .operands = {ValueId{0}, ValueId{1}}, .signatureTypes = {callable, probe.returnType}},
            typed::Instruction{.opcode = typed::Opcode::Release, .operands = {ValueId{0}}},
            typed::Instruction{.opcode = typed::Opcode::Return, .operands = {ValueId{2}}}
        };
        probe.blocks.push_back(std::move(body));
        const FunctionId probeId = probe.id;
        references.functions.push_back(std::move(probe));
        const auto referenceModule = LoweringPipeline{}.lower(references);
        for (const auto& diagnostic : referenceModule.diagnostics()) std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        if (!expect(referenceModule.succeeded(), "generic function-reference probe must lower")) return 1;
        const auto code = WirCppBackend{}.generate(referenceModule.module());
        for (const auto& diagnostic : code.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        if (!expect(code.succeeded(), "specialized generic bodies must emit C++")) return 1;
        const std::string harness = "#define main wio_fixture_main\n" + code.code() +
            "\n#undef main\nint main() { int result = wio_fixture_main(); if (result) return result; return _wio_f" +
            std::to_string(probeId.value()) + "() == 42 ? 0 : 91; }\n";
        ok &= expect(compileGeneratedCode(harness, true), "generic program and function-reference probe must execute successfully");
        return ok ? 0 : 1;
    }

    const auto generated = WirCppBackend{}.generate(makeExecutableModule());
    bool ok = true;
    ok &= expect(generated.succeeded(), "valid scalar Lowered WIR should generate C++");
    ok &= expect(generated.code().find("_wio_f0") != std::string::npos,
        "generated code should use stable WIR function identities");
    ok &= expect(generated.code().find("switch (_block)") != std::string::npos,
        "generated code should preserve canonical CFG structure");
    ok &= expect(generated.code().find("int main()") != std::string::npos,
        "program modules should receive an entry adapter");
    ok &= expect(compileGeneratedCode(generated.code()),
        "generated WIR C++ should compile with the configured host compiler");

    const auto languageModule = makeLanguageSurfaceModule();
    ok &= expect(languageModule.has_value(),
        "enum/flagset and Option/Result source should lower to canonical WIR");
    if (languageModule)
    {
        const auto languageGenerated = WirCppBackend{}.generate(*languageModule);
        if (!languageGenerated.succeeded())
            for (const auto& diagnostic : languageGenerated.diagnostics())
                std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        ok &= expect(languageGenerated.succeeded(),
            "enum/flagset and Option/Result WIR should generate C++");
        ok &= expect(languageGenerated.code().find("_wio_enum_name") != std::string::npos,
            "enum reflection helpers should be emitted from canonical layout metadata");
        ok &= expect(languageGenerated.code().find("Result does not contain a success value") != std::string::npos,
            "Result unwrap should preserve its checked failure boundary");
        ok &= expect(compileGeneratedCode(languageGenerated.code()),
            "generated enum/variant/Result C++ should compile with the configured host compiler");
    }

    const auto unsupported = WirCppBackend{}.generate(makeUnsupportedModule());
    if (unsupported.succeeded() || !std::ranges::any_of(unsupported.diagnostics(), [](const auto& diagnostic)
        { return diagnostic.code == "WCPP1205"; }))
        for (const auto& diagnostic : unsupported.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(!unsupported.succeeded(), "unsupported WIR operations should fail before C++ emission");
    ok &= expect(std::ranges::any_of(unsupported.diagnostics(), [](const auto& diagnostic)
        { return diagnostic.code == "WCPP1205"; }),
        "unsupported operation should have a stable backend diagnostic code");
    ok &= expect(unsupported.code().empty(), "failed generation should not expose partial C++ output");
    std::ifstream fixture(std::string(WIO_TEST_SOURCE_DIR) + "/tests/wir_cpp_containers_run.wio");
    const std::string source{std::istreambuf_iterator<char>(fixture), std::istreambuf_iterator<char>()};
    ok &= expect(!source.empty(), "container runtime fixture should be readable");
    const auto containers = makeLanguageSurfaceModule(source);
    ok &= expect(containers.has_value(), "container and iterator source should lower");
    if (containers)
    {
        auto malformed = *containers;
        bool changed = false;
        for (auto& function : malformed.functions)
            for (auto& block : function.blocks)
                for (auto& instruction : block.instructions)
                    if (!changed && instruction.opcode == wio::wir::lowered::Opcode::IteratorValue)
                    {
                        instruction.selector = "nonexistent_field";
                        changed = true;
                    }
        const auto rejected = WirCppBackend{}.generate(malformed);
        ok &= expect(changed && !rejected.succeeded() && rejected.code().empty() &&
            std::ranges::any_of(rejected.diagnostics(), [](const auto& diagnostic)
                { return diagnostic.code == "WCPP1210"; }),
            "malformed iterator projection must fail with a backend diagnostic");
        const auto code = WirCppBackend{}.generate(*containers);
        for (const auto& diagnostic : code.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
        ok &= expect(code.succeeded(), "container and iterator module should emit C++");
        if (code.succeeded())
        {
            const std::string harness = "#define main wio_fixture_main\n" + code.code() + R"CPP(
#undef main
int main() {
    using wio::wir_backend::Iterator;
    int rejected = 0;
    std::vector<int> values{1, 2};
    try { Iterator<int>::range(0, 2, 0, false); }
    catch (const wio::runtime::RuntimeException&) { ++rejected; }
    try { Iterator<std::vector<int>>::container(values, -1); }
    catch (const wio::runtime::RuntimeException&) { ++rejected; }
    try { Iterator<std::vector<int>>::container(values, 0); }
    catch (const wio::runtime::RuntimeException&) { ++rejected; }
    auto minimum = Iterator<int>::range(std::numeric_limits<int>::min() + 1,
        std::numeric_limits<int>::min(), -1, true);
    int count = 0;
    while (minimum.hasNext()) { ++count; minimum.advance(); if (count > 2) return 91; }
    if (rejected != 3 || count != 2) return 92;
    return wio_fixture_main();
}
)CPP";
            ok &= expect(compileGeneratedCode(harness, true), "container/Unicode/iterator program and boundary checks must execute successfully");
        }
    }
    return ok ? 0 : 1;
}
