#include "wio/codegen/wir_cpp_backend.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
        const TypeId enumType = module.types.internNominal(Type{
            .kind = TypeKind::Named,
            .name = "State",
            .nominalKind = NominalKind::Enum
        });
        lowered::Function entry;
        entry.id = FunctionId{0};
        entry.name = "Entry";
        entry.returnType = enumType;
        entry.callableType = module.types.intern(Type{
            .kind = TypeKind::Function,
            .arguments = {enumType},
            .ownership = OwnershipModel::ReferenceCounted,
            .cleanup = CleanupKind::ReleaseReference
        });
        lowered::BasicBlock block;
        block.id = BlockId{0};
        block.name = "entry";
        block.instructions = {
            lowered::Instruction{
                .opcode = lowered::Opcode::EnumConstant,
                .result = ValueId{0},
                .resultType = enumType,
                .selector = "Ready",
                .intrinsicFamily = IntrinsicFamily::Enum,
                .targetType = enumType
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Return,
                .operands = {ValueId{0}}
            }
        };
        entry.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(entry));
        return module;
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

    const auto unsupported = WirCppBackend{}.generate(makeUnsupportedModule());
    ok &= expect(!unsupported.succeeded(), "unsupported WIR operations should fail before C++ emission");
    ok &= expect(!unsupported.diagnostics().empty() && unsupported.diagnostics().front().code == "WCPP1201",
        "unsupported operation should have a stable backend diagnostic code");
    ok &= expect(unsupported.code().empty(), "failed generation should not expose partial C++ output");
    return ok ? 0 : 1;
}
