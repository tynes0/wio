#include "wio/codegen/wir_cpp_backend.h"
#include "wio/lexer/lexer.h"
#include "wio/parser/parser.h"
#include "wio/sema/analyzer.h"
#include "wio/wir/lowered_ir_printer.h"
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
                .opcode = lowered::Opcode::IteratorCreate,
                .result = ValueId{1},
                .resultType = iteratorType,
                .operands = {ValueId{0}},
                .selector = "array",
                .signatureTypes = {arrayType},
                .storageClass = lowered::StorageClass::Heap,
                .escapeClass = lowered::EscapeClass::Local
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

    std::optional<wio::wir::lowered::Module> makeLanguageSurfaceModule()
    {
        using namespace wio;
        Lexer lexer(
            R"WIO(
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
)WIO",
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
        auto lowered = wir::LoweringPipeline{}.lower(typed.module());
        if (!lowered.succeeded())
        {
            for (const auto& diagnostic : lowered.diagnostics())
                std::cerr << diagnostic.code << " [" << diagnostic.source.begin.toDiagnosticString()
                          << "]: " << diagnostic.message << '\n';
            std::cerr << wir::lowered::Printer{}.print(lowered.module());
            return std::nullopt;
        }
        return lowered.takeModule();
    }

    bool compileGeneratedCode(const std::string& code)
    {
        namespace fs = std::filesystem;
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path directory = fs::temp_directory_path() /
            ("wio-wir-cpp-backend-" + std::to_string(nonce));
        std::error_code error;
        fs::create_directories(directory, error);
        if (error) return false;
        const fs::path source = directory / "generated.cpp";
        const fs::path object = directory / "generated.obj";
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
            command += "-std=c++20 -c \"" + source.string() + "\" -I\"" +
                std::string(WIO_TEST_RUNTIME_INCLUDE) + "\" -o \"" + object.string() + "\"";
        }
#if defined(_WIN32)
        command = "\"" + command + "\"";
#endif
        const int result = std::system(command.c_str());
        fs::remove_all(directory, error);
        return result == 0;
    }
}

int main()
{
    using wio::codegen::WirCppBackend;

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
        { return diagnostic.code == "WCPP1201"; }))
        for (const auto& diagnostic : unsupported.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(!unsupported.succeeded(), "unsupported WIR operations should fail before C++ emission");
    ok &= expect(std::ranges::any_of(unsupported.diagnostics(), [](const auto& diagnostic)
        { return diagnostic.code == "WCPP1201"; }),
        "unsupported operation should have a stable backend diagnostic code");
    ok &= expect(unsupported.code().empty(), "failed generation should not expose partial C++ output");
    return ok ? 0 : 1;
}
